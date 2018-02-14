/**
 * @brief REST Aggregation Protocol test server
 * @author Johan Lindh <johan@linkdata.se>
 * @note Copyright (c)2015-2017 Johan Lindh
 */

#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>

#include "rap_conn.hpp"
#include "rap_exchange.hpp"
#include "rap_reader.hpp"
#include "rap_request.hpp"
#include "rap_response.hpp"

#include "crap.h"

using boost::asio::ip::tcp;

class session : public std::enable_shared_from_this<session> {
 public:
  session(tcp::socket socket, rap::stats& stats)
      : socket_(std::move(socket)), conn_(0), stats_(stats) {}

  ~session() {
    if (conn_) {
      rap_conn_destroy(conn_);
      conn_ = 0;
    }
  }

  void start() {
    if (!conn_) conn_ = rap_conn_create(s_write_cb, this, s_frame_cb, this);
    read_stream();
  }

 private:
  static int s_write_cb(void* self, const char* src_ptr, int src_len) {
    return static_cast<session*>(self)->write_cb(src_ptr, src_len);
  }

  static int s_frame_cb(void* self, const rap_frame* f, int len) {
    return static_cast<session*>(self)->frame_cb(f, len);
  }

  // may be called from a foreign thread via the callback
  int write_cb(const char* src_ptr, int src_len) {
    // TODO: thread safety?
    buf_towrite_.insert(buf_towrite_.end(), src_ptr, src_ptr + src_len);
    write_some();
    return 0;
  }

  // may be called from a foreign thread via the callback
  int frame_cb(const rap_frame* f, int len) {
    // TODO: thread safety?
    assert(f != NULL);
    assert(len >= rap_frame_header_size);
    assert(len == rap_frame_header_size + f->header().payload_size());

    const rap_header& hdr = f->header();
    rap::exchange& exch = static_cast<rap::conn*>(conn_)->exchange(hdr.id());
    rap::reader r(f);
    if (hdr.has_head()) {
      exch.process_head(r);
      stats_.head_count++;
    }
    if (hdr.has_body()) exch.process_body(r);
    if (hdr.is_final()) exch.process_final(r);
    return 0;
  }

  // writes any buffered data to the stream using conn_t::write_stream()
  // may be called from a foreign thread via the callback
  void write_some() {
    // TODO: thread safety?
    if (!buf_writing_.empty() || buf_towrite_.empty()) return;
    buf_writing_.swap(buf_towrite_);
    write_stream(buf_writing_.data(), buf_writing_.size());
    return;
  }

  void write_stream(const char* src_ptr, size_t src_len) {
    auto self(shared_from_this());

#if 0
    const char* src_end = src_ptr + src_len;
    const char* p = src_ptr;
    fprintf(stderr, "W: ");
    while (p < src_end) {
      fprintf(stderr, "%02x ", (*p++) & 0xFF);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
#endif

    boost::asio::async_write(
        socket_, boost::asio::buffer(src_ptr, src_len),
        [this, self](boost::system::error_code ec, std::size_t length) {
          if (ec) {
            fprintf(stderr, "rapper::conn::write_stream(%s, %lu)\n",
                    ec.message().c_str(), static_cast<unsigned long>(length));
            fflush(stderr);
            return;
          } else {
            assert(length == buf_writing_.size());
            stats_.add_bytes_written(length);
          }
          buf_writing_.clear();
          write_some();
        });
    return;
  }

  void read_stream() {
    auto self(shared_from_this());
    socket_.async_read_some(
        boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length) {
          if (ec) {
            if (ec != boost::asio::error::eof) {
              fprintf(stderr, "rapper::conn::read_stream(%s, %lu) [%d]\n",
                      ec.message().c_str(), static_cast<unsigned long>(length),
                      ec.value());
              fflush(stderr);
            }
            return;
          }
          stats_.add_bytes_read(length);
#if 0
          const char* src_end = data_ + length;
          const char* p = data_;
          fprintf(stderr, "R: ");
          while (p < src_end) {
            fprintf(stderr, "%02x ", (*p++) & 0xFF);
          }
          fprintf(stderr, "\n");
          fflush(stderr);
#endif

          int rap_ec = rap_conn_recv(conn_, data_, (int)length);
          if (rap_ec < 0) {
            fprintf(stderr, "rapper::conn::read_stream(): rap error %d\n",
                    rap_ec);
            fflush(stderr);
          } else {
            assert(rap_ec == (int)length);
          }

          read_stream();
        });
  }

  enum { max_length = 1024 };
  tcp::socket socket_;
  char data_[max_length];
  std::vector<char> buf_towrite_;
  std::vector<char> buf_writing_;
  rap_conn* conn_;
  rap::stats& stats_;
};

class server {
 public:
  server(boost::asio::io_service& io_service, short port)
      : acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
        socket_(io_service),
        timer_(io_service),
        last_head_count_(0),
        last_read_iops_(0),
        last_read_bytes_(0),
        last_write_iops_(0),
        last_write_bytes_(0),
        last_stat_mbps_in_(0),
        last_stat_mbps_out_(0) {
    do_timer();
    do_accept();
  }

 protected:
  uint64_t last_head_count_;
  uint64_t last_read_iops_;
  uint64_t last_read_bytes_;
  uint64_t last_write_iops_;
  uint64_t last_write_bytes_;
  uint64_t last_stat_mbps_in_;
  uint64_t last_stat_mbps_out_;

 private:
  void do_timer() {
    timer_.expires_from_now(boost::posix_time::seconds(1));
    timer_.async_wait(
        [this](const boost::system::error_code ec) { handle_timeout(ec); });
  }

  void do_accept() {
    acceptor_.async_accept(socket_, [this](boost::system::error_code ec) {
      if (!ec) {
        std::make_shared<session>(std::move(socket_), stats_)->start();
      }

      do_accept();
    });
  }

  void handle_timeout(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
      uint64_t n;
      uint64_t stat_rps_;
      uint64_t stat_iops_in_;
      uint64_t stat_mbps_in_;
      uint64_t stat_iops_out_;
      uint64_t stat_mbps_out_;

      n = stats_.head_count;
      stat_rps_ = n - last_head_count_;
      last_head_count_ = n;

      n = stats_.read_iops;
      stat_iops_in_ = n - last_read_iops_;
      last_read_iops_ = n;

      n = stats_.read_bytes;
      stat_mbps_in_ = ((n - last_read_bytes_) * 8) / 1024 / 1024;
      last_read_bytes_ = n;

      n = stats_.write_iops;
      stat_iops_out_ = n - last_write_iops_;
      last_write_iops_ = n;

      n = stats_.write_bytes;
      stat_mbps_out_ = ((n - last_write_bytes_) * 8) / 1024 / 1024;
      last_write_bytes_ = n;

      if (stat_mbps_in_ != last_stat_mbps_in_ || stat_mbps_out_ != last_stat_mbps_out_) {
        last_stat_mbps_in_ = stat_mbps_in_;
        last_stat_mbps_out_ = stat_mbps_out_;
        fprintf(
            stderr,
            "%llu Rps - IN: %llu Mbps, %llu iops - OUT: %llu Mbps, %llu iops\n",
            stat_rps_, stat_mbps_in_, stat_iops_in_, stat_mbps_out_,
            stat_iops_out_);
      }
    }
    do_timer();
  }

  boost::asio::deadline_timer timer_;
  tcp::acceptor acceptor_;
  tcp::socket socket_;
  rap::stats stats_;
};

int main(int argc, char* argv[]) {
  const char* port = "10111";
  try {
    if (argc == 2) {
      port = argv[1];
    }

    boost::asio::io_service io_service;

    server s(io_service, std::atoi(port));

    io_service.run();
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
