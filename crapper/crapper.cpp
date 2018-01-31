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

  crapper_conn(boost::asio::io_service& ios) : rap::conn(), socket_(ios) {}

  boost::asio::ip::tcp::socket& socket() { return socket_; }

  void start() {
    this->read_some();
    return;
  }

  void read_stream(char* buf_ptr, size_t buf_max) {
    // fprintf(stderr, "rapper::conn::read_stream(%p, %lu)\n", buf_ptr,
    // buf_max);
    socket_.async_read_some(
        boost::asio::buffer(buf_ptr, buf_max),
        boost::bind(&crapper_conn::handle_read, this->shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
    return;
  }

  void write_stream(const char* src_ptr, size_t src_len) {
    // fprintf(stderr, "rapper::conn::write_stream(%p, %lu)\n", src_ptr,
    // src_len);
    boost::asio::async_write(
        socket_, boost::asio::buffer(src_ptr, src_len),
        boost::bind(&crapper_conn::handle_write, this->shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
    return;
  }

 private:
  boost::asio::ip::tcp::socket socket_;

  void handle_read(const boost::system::error_code& error,
                   size_t bytes_transferred) {
    if (error) {
      fprintf(stderr, "rapper::conn::handle_read(%s, %lu)\n",
              error.message().c_str(),
              static_cast<unsigned long>(bytes_transferred));
      return;
    }
    this->read_stream_ok(bytes_transferred);
    this->read_some();
    return;
  }

  void handle_write(const boost::system::error_code& error,
                    size_t bytes_transferred) {
    if (error) {
      fprintf(stderr, "rapper::conn::handle_write(%s, %lu)\n",
              error.message().c_str(),
              static_cast<unsigned long>(bytes_transferred));
      return;
    }
    this->write_stream_ok(bytes_transferred);
    this->write_some();
    return;
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
        stat_mbps_out_(0),
        stat_head_count_(0),
        stat_read_iops_(0),
        stat_read_bytes_(0),
        stat_write_iops_(0),
        stat_write_bytes_(0) {}

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

  uint64_t stat_head_count() const { return stat_head_count_; }
  void stat_head_count_inc() { ++stat_head_count_; }
  uint64_t stat_read_iops() const { return stat_read_iops_; }
  uint64_t stat_read_bytes() const { return stat_read_bytes_; }
  void stat_read_bytes_add(uint64_t n) {
    ++stat_read_iops_;
    stat_read_bytes_ += n;
  }
  uint64_t stat_write_iops() const { return stat_write_iops_; }
  uint64_t stat_write_bytes() const { return stat_write_bytes_; }
  void stat_write_bytes_add(uint64_t n) {
    ++stat_write_iops_;
    stat_write_bytes_ += n;
  }

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
  uint64_t stat_head_count_;
  uint64_t stat_read_iops_;
  uint64_t stat_read_bytes_;
  uint64_t stat_write_iops_;
  uint64_t stat_write_bytes_;
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

  void start_accept() {
    crapper_conn::ptr conn(new crapper_conn(io_service()));
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

      n = this->stat_head_count();
      stat_rps_ = n - last_head_count_;
      last_head_count_ = n;

      n = this->stat_read_iops();
      stat_iops_in_ = n - last_read_iops_;
      last_read_iops_ = n;

      n = this->stat_read_bytes();
      stat_mbps_in_ = ((n - last_read_bytes_) * 8) / 1024 / 1024;
      last_read_bytes_ = n;

      n = this->stat_write_iops();
      stat_iops_out_ = n - last_write_iops_;
      last_write_iops_ = n;

      n = this->stat_write_bytes();
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
