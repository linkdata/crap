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

class exchange : public std::streambuf {
 public:
  explicit exchange()
      : exch_(0)
      , id_(0)
    {
  }

    void init(rap_exchange* exch)
    {
        exch_ = exch;
        id_ = rap_exch_get_id(exch_);
        start_write();
    }

    uint16_t id() const {
        return id_;
    }

  const rap_header& header() const {
    return *reinterpret_cast<const rap_header*>(buf_.data());
  }

  rap_header& header() { return *reinterpret_cast<rap_header*>(buf_.data()); }

  rap::error process_head(rap::reader& r) {
    if (r.read_tag() != rap::record::tag_http_request)
      return rap::rap_err_unknown_frame_type;
    rap::request req(r);
    req_echo_.clear();
    req.render(req_echo_);
    req_echo_ += '\n';
    header().set_head();
    int64_t content_length = req.content_length();
    if (content_length >= 0) content_length += req_echo_.size();
    rap::writer(*this) << rap::response(200, content_length);
    header().set_body();
    sputn(req_echo_.data(), req_echo_.size());
    return r.error();
  }

  rap::error process_body(rap::reader& r) {
    assert(r.size() > 0);
    header().set_body();
    sputn(r.data(), r.size());
    r.consume();
    return r.error();
  }

  rap::error process_final(rap::reader& r) {
    assert(r.size() == 0);
    header().set_final();
    pubsync();
    return r.error();
  }

 protected:
  exchange::int_type exchange::overflow(int_type ch) {
    if (buf_.size() < rap_frame_max_size) {
      size_t new_size = buf_.size() * 2;
      if (new_size > rap_frame_max_size) new_size = rap_frame_max_size;
      buf_.resize(new_size);
      setp(buf_.data() + rap_frame_header_size, buf_.data() + buf_.size());
    }

    bool was_head = header().has_head();
    bool was_body = header().has_body();
    bool was_final = header().is_final();

    if (was_final && ch != traits_type::eof()) header().clr_final();

    if (sync() != 0) {
      if (was_final) header().set_final();
      return traits_type::eof();
    }

    if (was_body)
      header().set_body();
    else if (was_head)
      header().set_head();

    if (was_final) header().set_final();

    if (ch != traits_type::eof()) {
      *pptr() = ch;
      pbump(1);
    }

    return ch;
  }

  int write_frame(const rap_frame* f) {
      return rap_exch_write_frame(exch_, f);
  }

  int exchange::sync() {
    header().set_size_value(pptr() - (buf_.data() + rap_frame_header_size));
    if (write_frame(reinterpret_cast<rap_frame*>(buf_.data()))) {
      assert(!"rap::exchange::sync(): write_frame() failed");
      return -1;
    }
    start_write();
    return 0;
  }

 private:
  rap_exchange* exch_;
  uint16_t id_;
  std::vector<char> buf_;
  rap::string_t req_echo_;

  void exchange::start_write() {
    if (buf_.size() < 256) buf_.resize(256);
    buf_[0] = '\0';
    buf_[1] = '\0';
    buf_[2] = static_cast<char>(id_ >> 8);
    buf_[3] = static_cast<char>(id_);
    setp(buf_.data() + rap_frame_header_size, buf_.data() + buf_.size());
  }
};

class session : public std::enable_shared_from_this<session> {
 public:
  session(tcp::socket socket, rap::stats& stats)
      : socket_(std::move(socket)), conn_(0), stats_(stats), exchanges_(rap_max_exchange_id+1) {}

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

  static int s_frame_cb(void* self, rap_exchange* exch, const rap_frame* f, int len) {
    return static_cast<session*>(self)->frame_cb(exch, f, len);
  }

  // may be called from a foreign thread via the callback
  int write_cb(const char* src_ptr, int src_len) {
    // TODO: thread safety?
    buf_towrite_.insert(buf_towrite_.end(), src_ptr, src_ptr + src_len);
    write_some();
    return 0;
  }

  // may be called from a foreign thread via the callback
  int frame_cb(rap_exchange* exch, const rap_frame* f, int len) {
    // TODO: thread safety?
    assert(f != NULL);
    assert(len >= rap_frame_header_size);
    assert(len == rap_frame_header_size + f->header().payload_size());

    const rap_header& hdr = f->header();
    uint16_t id = hdr.id();
    exchange& ex = exchanges_[id];
    if (ex.id() != id) {
        ex.init(exch);
    }

    rap::reader r(f);
    if (hdr.has_head()) {
      ex.process_head(r);
      stats_.head_count++;
    }
    if (hdr.has_body()) ex.process_body(r);
    if (hdr.is_final()) ex.process_final(r);
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
  std::vector<exchange> exchanges_;
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
