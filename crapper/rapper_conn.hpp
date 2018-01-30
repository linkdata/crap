#ifndef RAPPER_CONN_HPP
#define RAPPER_CONN_HPP

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

#include "rap_conn.hpp"
#include "rap_server.hpp"

#include "rapper.hpp"
#include "rapper_server.hpp"

namespace rapper {

class rap_conn : public boost::enable_shared_from_this<rap_conn>,
                 public rap::conn {
 public:
  typedef boost::shared_ptr<rapper::rap_server> server_ptr;
  typedef boost::shared_ptr<rapper::rap_conn> ptr;

  rap_conn(server_ptr s);
  boost::asio::ip::tcp::socket& socket() { return socket_; }
  void start();
  void read_stream(char* buf_ptr, size_t buf_max);
  void write_stream(const char* src_ptr, size_t src_len);

 private:
  boost::asio::ip::tcp::socket socket_;

  void handle_read(const boost::system::error_code& error,
                   size_t bytes_transferred);
  void handle_write(const boost::system::error_code& error,
                    size_t bytes_transferred);
};

}  // namespace rapper

#endif  // RAPPER_CONN_HPP
