#ifndef RAP_CONN_HPP
#define RAP_CONN_HPP

#include <cassert>
#include <cstdint>
#include <vector>

#include <atomic>

#include "rap.hpp"
#include "rap_exchange.hpp"
#include "rap_frame.hpp"
#include "rap_server.hpp"
#include "rap_text.hpp"
#include "rap_stats.hpp"

namespace rap {

class conn {
 public:
  static const int16_t max_send_window = 8;

  explicit conn();
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

  rap::stats& stats() { return stats_; }
  const rap::stats& stats() const { return stats_; }

  uint64_t stat_head_count() { return stats_.head_count.exchange(0); }
  uint64_t stat_read_iops() { return stats_.read_iops.exchange(0); }
  uint64_t stat_read_bytes() { return stats_.read_bytes.exchange(0); }
  uint64_t stat_write_iops() { return stats_.read_iops.exchange(0); }
  uint64_t stat_write_bytes() { return stats_.read_bytes.exchange(0); }

 private:
  uint16_t next_id_;
  char buffer_[rap_frame_max_size * 2];
  char* buf_ptr_;  //< current position within buffer
  char* buf_end_;  //< expected end of frame or NULL if no header yet
  std::vector<char> buf_towrite_;
  std::vector<char> buf_writing_;
  std::vector<rap::exchange> exchanges_;
  rap::stats stats_;

  virtual void read_stream(char* buf_ptr, size_t buf_max) {
    // read input stream into buffer provided,
    // call read_stream_ok() when done (or abort the conn)
    assert(0);
    return;
  }
  virtual void write_stream(const char* src_ptr, size_t src_len) {
    // read input stream into buffer provided,
    // call read_stream_ok() when done (or abort the conn)
    assert(0);
    return;
  }
};

}  // namespace rap

#endif  // RAP_CONN_HPP
