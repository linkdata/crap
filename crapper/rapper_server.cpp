#include "rapper_server.hpp"

namespace rapper {

rap_server::rap_server(boost::asio::io_service& io_service, short port)
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

void rap_server::start() {
  timer_.expires_from_now(boost::posix_time::seconds(1));
  timer_.async_wait(boost::bind(&rap_server::handle_timeout,
                                this->shared_from_this(),
                                boost::asio::placeholders::error));
  start_accept();
}

void rap_server::start_accept() {
  rap_conn::ptr conn(new rap_conn(this->shared_from_this()));
  acceptor_.async_accept(
      conn->socket(),
      boost::bind(&rap_server::handle_accept, this->shared_from_this(), conn,
                  boost::asio::placeholders::error));
  return;
}

void rap_server::handle_accept(rap_conn::ptr new_conn,
                               const boost::system::error_code& error) {
  if (!error) {
    new_conn->start();
  }

  start_accept();
  return;
}

void rap_server::handle_timeout(const boost::system::error_code& e) {
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
  timer_.async_wait(boost::bind(&handle_timeout, this->shared_from_this(),
                                boost::asio::placeholders::error));
}

}  // namespace rapper
