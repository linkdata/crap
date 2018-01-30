#ifndef RAPPER_SERVER_HPP
#define RAPPER_SERVER_HPP

#include "rap_server.hpp"
#include "rapper.hpp"
#include "rapper_conn.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

namespace rapper {

class rap_server : public rap::server,
                   public boost::enable_shared_from_this<rap_server> {
 public:
  typedef boost::shared_ptr<rap_server> ptr;

  virtual ~rap_server() {}
  rap::server* as_rap_server() { return this; }
  rap_server(boost::asio::io_service& io_service, short port);
  void start();
  virtual void once_per_second() {}
  const boost::asio::io_service& io_service() const { return io_service_; }
  boost::asio::io_service& io_service() { return io_service_; }

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

  void start_accept();
  void handle_accept(rap_conn::ptr new_conn,
                     const boost::system::error_code& error);
  void handle_timeout(const boost::system::error_code& e);
};

}  // namespace rapper

#endif  // RAPPER_SERVER_HPP
