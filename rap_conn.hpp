#ifndef RAP_CONN_HPP
#define RAP_CONN_HPP

#include "rap.hpp"
#include "rap_exchange.hpp"
#include "rap_frame.hpp"
#include "rap_server.hpp"
#include "rap_text.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace rap {

class conn {
 public:
  static const int16_t max_send_window = 8;

  explicit conn(rap::server& server);
  void process_conn(const frame* f) { assert(!"TODO!"); }

  // results in a call to conn_t::read_stream()
  void read_some() {
    read_stream(buf_end_, sizeof(buffer_) - (buf_end_ - buffer_));
    return;
  }

  // consume up to 'len' bytes of data from 'p',
  // return the number of bytes consumed, or a
  // negative value on error.
  int read_stream(const char* p, int len);

  // new stream data available in the read buffer
  void read_stream_ok(size_t bytes_transferred);

  // buffered write to the network stream
  error write(const char* src_ptr, size_t src_len);

  // writes any buffered data to the stream using conn_t::write_stream()
  void write_some();

  void write_stream_ok(size_t bytes_transferred);

  // used while constructing when initializing vector of exchanges
  uint16_t next_id();

  const rap::exchange& exchange(uint16_t id) const { return exchanges_[id]; }
  rap::exchange& exchange(uint16_t id) { return exchanges_[id]; }
  int16_t send_window() const { return max_send_window; }

 private:
  uint16_t next_id_;
  char buffer_[rap_frame_max_size * 2];
  char* buf_ptr_;  //< current position within buffer
  char* buf_end_;  //< expected end of frame or NULL if no header yet
  std::vector<char> buf_towrite_;
  std::vector<char> buf_writing_;
  std::vector<rap::exchange> exchanges_;
  rap::server& server_;

  // must be implemented in conn_t
  void read_stream(char* buf_ptr, size_t buf_max);

  // must be implemented in conn_t
  void write_stream(const char* src_ptr, size_t src_len);
};

}  // namespace rap

#endif  // RAP_CONN_HPP
