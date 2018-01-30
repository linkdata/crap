#include "rapper_conn.hpp"

namespace rapper {

rap_conn::rap_conn(server_ptr s)
    : rap::conn(*static_cast<rap::server*>(s.get())),
      socket_(s->io_service()) {}

void rap_conn::start() {
  this->read_some();
  return;
}

void rap_conn::read_stream(char* buf_ptr, size_t buf_max) {
  // fprintf(stderr, "rapper::conn::read_stream(%p, %lu)\n", buf_ptr,
  // buf_max);
  socket_.async_read_some(
      boost::asio::buffer(buf_ptr, buf_max),
      boost::bind(&handle_read, this->shared_from_this(),
                  boost::asio::placeholders::error,
                  boost::asio::placeholders::bytes_transferred));
  return;
}

void rap_conn::write_stream(const char* src_ptr, size_t src_len) {
  // fprintf(stderr, "rapper::conn::write_stream(%p, %lu)\n", src_ptr,
  // src_len);
  boost::asio::async_write(
      socket_, boost::asio::buffer(src_ptr, src_len),
      boost::bind(&handle_write, this->shared_from_this(),
                  boost::asio::placeholders::error,
                  boost::asio::placeholders::bytes_transferred));
  return;
}

void rap_conn::handle_read(const boost::system::error_code& error,
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

void rap_conn::handle_write(const boost::system::error_code& error,
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

}  // namespace rapper
