#ifndef RAP_CONN_HPP
#define RAP_CONN_HPP

#include <cassert>
#include <cstdint>
#include <vector>

#include <atomic>

#include "rap.hpp"
#include "rap_exchange.hpp"
#include "rap_frame.hpp"
#include "rap_stats.hpp"
#include "rap_text.hpp"

namespace rap {

class conn {
 public:
  typedef int (*write_cb_t)(void*, const char*, int);

  static const int16_t max_send_window = 8;

  explicit conn(write_cb_t cb, void* cb_param);

  void process_conn(const frame* f) { assert(!"TODO!"); }

  // consume up to 'len' bytes of data from 'p',
  // return the number of bytes consumed, or a
  // negative value on error.
  int read_stream(const char* p, int len);

  // new stream data available in the read buffer
  // void read_stream_ok(size_t bytes_transferred);

  // buffered write to the network stream
  rap::error write(const char* src_ptr, int src_len);

  // writes any buffered data to the stream using conn_t::write_stream()
  void write_some();

  void write_stream_ok(size_t bytes_transferred);

  // used while constructing when initializing vector of exchanges
  uint16_t next_id();
  const rap::exchange& exchange(uint16_t id) const { return exchanges_[id]; }
  rap::exchange& exchange(uint16_t id) { return exchanges_[id]; }
  int16_t send_window() const { return max_send_window; }

  rap::stats& stats() { return stats_; }
  const rap::stats& stats() const { return stats_; }

 private:
  uint16_t next_id_;
  char buffer_[rap_frame_max_size * 2];
  char* buf_ptr_;  //< current position within buffer
  char* buf_end_;  //< expected end of frame or NULL if no header yet
  std::vector<char> buf_towrite_;
  std::vector<char> buf_writing_;
  std::vector<rap::exchange> exchanges_;
  rap::stats stats_;
  void* write_cb_param_;
  write_cb_t write_cb_;
};

}  // namespace rap

#endif  // RAP_CONN_HPP
