//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

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
  session(tcp::socket socket) : socket_(std::move(socket)), conn_(0) {}

  ~session() {
    if (conn_) {
      rap_conn_destroy(conn_);
      conn_ = 0;
    }
  }

  void start() {
    if (!conn_) conn_ = rap_conn_create(s_write_cb, this, s_frame_cb, this);
    do_read();
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
    if (hdr.has_head()) exch.process_head(r);
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
    boost::asio::async_write(
        socket_, boost::asio::buffer(src_ptr, src_len),
        [this, self](boost::system::error_code ec, std::size_t length) {
          if (!ec) {
            write_stream_ok(length);
            write_some();
          }
        });
    return;
  }

  void write_stream_ok(size_t bytes_transferred) {
    assert(bytes_transferred == buf_writing_.size());
    buf_writing_.clear();
    return;
  }

  void do_read() {
    auto self(shared_from_this());
    socket_.async_read_some(
        boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length) {
          if (!ec) {
            int rap_ec = rap_conn_recv(conn_, data_, (int)length);
            if (!rap_ec) {
              do_read();
            }
          }
        });
  }

#if 0
  void do_write(std::size_t length) {
    auto self(shared_from_this());
    boost::asio::async_write(
        socket_, boost::asio::buffer(data_, length),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
          if (!ec) {
            do_read();
          }
        });
  }
#endif

  enum { max_length = 1024 };
  tcp::socket socket_;
  char data_[max_length];
  std::vector<char> buf_towrite_;
  std::vector<char> buf_writing_;
  rap_conn* conn_;
};

class server {
 public:
  server(boost::asio::io_service& io_service, short port)
      : acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
        socket_(io_service) {
    do_accept();
  }

 private:
  void do_accept() {
    acceptor_.async_accept(socket_, [this](boost::system::error_code ec) {
      if (!ec) {
        std::make_shared<session>(std::move(socket_))->start();
      }

      do_accept();
    });
  }

  tcp::acceptor acceptor_;
  tcp::socket socket_;
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

#if 0
/**
 * @brief REST Aggregation Protocol test server
 * @author Johan Lindh <johan@linkdata.se>
 * @note Copyright (c)2015-2017 Johan Lindh
 */

#include <iostream>
#include <sstream>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

#include "rap_exchange.hpp"
#include "rap_reader.hpp"
#include "rap_request.hpp"
#include "rap_response.hpp"
#include "rap_stats.hpp"
#include "rap_writer.hpp"


class rap_exchange : public rap::exchange {
 public:
  explicit rap_exchange(rap::conn& conn) : rap::exchange(conn) {}

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

 private:
  rap::string_t req_echo_;
};

class crapper_conn : rap::conn,
                     public boost::enable_shared_from_this<crapper_conn> {
 public:
  typedef boost::shared_ptr<crapper_conn> ptr;

  crapper_conn(boost::asio::io_service& io_service, rap::stats& stats)
      : rap::conn(&write_cb, this),
        socket_(io_service),
        stats_(stats),
        timer_(io_service),
        buf_ptr_(buffer_),
        buf_end_(buf_ptr_) {}

  boost::asio::ip::tcp::socket& socket() { return socket_; }

  static int write_cb(void* self, const char* p, int n) {
    return static_cast<crapper_conn*>(self)->write_stream(p, n);
  }

  void start() {
    timer_.expires_from_now(boost::posix_time::seconds(1));
    read_some();
    return;
  }

  void read_some() {
    socket_.async_read_some(
        boost::asio::buffer(buf_end_, sizeof(buffer_) - (buf_end_ - buffer_)),
        boost::bind(&crapper_conn::handle_read, shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
    return;
  }

  // writes any buffered data to the stream using conn_t::write_stream()
  void write_some() {
    if (!buf_writing_.empty() || buf_towrite_.empty()) return;
    buf_writing_.swap(buf_towrite_);
    write_stream(buf_writing_.data(), buf_writing_.size());
    return;
  }

  int write_stream(const char* src_ptr, size_t src_len) {
    if (!write_error_) {
      boost::asio::async_write(
          socket_, boost::asio::buffer(src_ptr, src_len),
          boost::bind(&crapper_conn::handle_write, this->shared_from_this(),
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
    }
    return write_error_.value;
  }

 private:
  boost::asio::ip::tcp::socket socket_;
  boost::asio::deadline_timer timer_;
  rap::stats& stats_;
  boost::system::error_code write_error_;
  char buffer_[rap::rap_frame_max_size * 2];
  char* buf_ptr_;  //< current position within buffer
  char* buf_end_;  //< expected end of frame or NULL if no header yet
  std::vector<char> buf_towrite_;
  std::vector<char> buf_writing_;

  void handle_read(const boost::system::error_code& error,
                   size_t bytes_transferred) {
    if (error) {
      fprintf(stderr, "rapper::conn::handle_read(%s, %lu)\n",
              error.message().c_str(),
              static_cast<unsigned long>(bytes_transferred));
      return;
    }

    stats_.add_bytes_read(bytes_transferred);
    buf_end_ += bytes_transferred;
    assert(buf_end_ <= buffer_ + sizeof(buffer_));
    int consumed_bytes =
        read_stream(buffer_, static_cast<int>(buf_end_ - buffer_));
    const char* src_ptr = buffer_ + consumed_bytes;
    // move trailing unprocessed input to the start
    if (src_ptr > buffer_) {
      char* dst_ptr = buffer_;
      while (src_ptr < buf_end_) *(dst_ptr++) = *src_ptr++;
      buf_end_ = dst_ptr;
    }

    read_some();
    return;
  }

  void handle_write(const boost::system::error_code& error,
                    size_t bytes_transferred) {
    if (error) {
      write_error_ = error;
      fprintf(stderr, "rapper::conn::handle_write(%s, %lu)\n",
              error.message().c_str(),
              static_cast<unsigned long>(bytes_transferred));
      return;
    }
    this->write_stream_ok(bytes_transferred);
    this->write_some();
    return;
  }

  void handle_timeout(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
      stats().aggregate_into(stats_);
    }
    timer_.expires_from_now(boost::posix_time::seconds(1));
    timer_.async_wait(boost::bind(&crapper_conn::handle_timeout,
                                  this->shared_from_this(),
                                  boost::asio::placeholders::error));
  }
};

class crapper_server : public boost::enable_shared_from_this<crapper_server> {
 public:
  typedef boost::shared_ptr<crapper_server> ptr;

  crapper_server(boost::asio::io_service& io_service, short port)
      : io_service_(io_service),
        acceptor_(io_service, boost::asio::ip::tcp::endpoint(
                                  boost::asio::ip::tcp::v4(), port)),
        timer_(io_service),
        last_head_count_(0),
        last_read_bytes_(0),
        last_write_bytes_(0),
        stat_rps_(0),
        stat_mbps_in_(0),
        stat_mbps_out_(0) {}

  virtual ~crapper_server() {}

  void start() {
    timer_.expires_from_now(boost::posix_time::seconds(1));
    timer_.async_wait(boost::bind(&crapper_server::handle_timeout,
                                  this->shared_from_this(),
                                  boost::asio::placeholders::error));
    start_accept();
  }

  const boost::asio::io_service& io_service() const { return io_service_; }
  boost::asio::io_service& io_service() { return io_service_; }

  void once_per_second() {
    if (stat_rps_ || stat_mbps_in_ || stat_mbps_out_) {
      fprintf(
          stderr,
          "%llu Rps - IN: %llu Mbps, %llu iops - OUT: %llu Mbps, %llu iops\n",
          stat_rps_, stat_mbps_in_, stat_iops_in_, stat_mbps_out_,
          stat_iops_out_);
    }
  }

 protected:
  uint64_t last_head_count_;
  uint64_t last_read_iops_;
  uint64_t last_read_bytes_;
  uint64_t last_write_iops_;
  uint64_t last_write_bytes_;
  uint64_t stat_rps_;
  uint64_t stat_iops_in_;
  uint64_t stat_mbps_in_;
  uint64_t stat_iops_out_;
  uint64_t stat_mbps_out_;

 private:
  boost::asio::io_service& io_service_;
  boost::asio::ip::tcp::acceptor acceptor_;
  boost::asio::deadline_timer timer_;
  rap::stats stats_;

  void start_accept() {
    crapper_conn::ptr conn(new crapper_conn(io_service(), stats_));
    acceptor_.async_accept(
        conn->socket(),
        boost::bind(&crapper_server::handle_accept, this->shared_from_this(),
                    conn, boost::asio::placeholders::error));
    return;
  }

  void handle_accept(crapper_conn::ptr new_conn,
                     const boost::system::error_code& error) {
    if (!error) {
      new_conn->start();
    }

    start_accept();
    return;
  }

  void handle_timeout(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
      uint64_t n;

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

      once_per_second();
    }
    timer_.expires_from_now(boost::posix_time::seconds(1));
    timer_.async_wait(boost::bind(&crapper_server::handle_timeout,
                                  this->shared_from_this(),
                                  boost::asio::placeholders::error));
  }
};

int main(int, char* []) {
  assert(sizeof(rap::text) == sizeof(const char*) + sizeof(size_t));
  assert(sizeof(rap::header) == 4);
  try {
    boost::asio::io_service io_service;
    crapper_server::ptr s(new crapper_server(io_service, 10111));
    s->start();
    io_service.run();
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
#endif
