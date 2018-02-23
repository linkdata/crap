#ifndef RAP_CONN_HPP
#define RAP_CONN_HPP

#include <atomic>
#include <cassert>
#include <cstdint>
#include <vector>

#include "rap.hpp"
#include "rap_callbacks.h"
#include "rap_exchange.hpp"
#include "rap_frame.h"
#include "rap_text.hpp"

namespace rap {

enum { rap_max_exchange_id = rap_conn_exchange_id - 1 };

class conn {
 public:
  static const int16_t max_send_window = 8;

  explicit conn(rap_write_cb_t write_cb, void* write_cb_param)
      : write_cb_(write_cb),
        write_cb_param_(write_cb_param),
        frame_ptr_(frame_buf_),
        exchanges_(rap_max_exchange_id + 1) {
    // assert correctly initialized exchange vector
    assert(exchanges_.size() == rap_max_exchange_id + 1);
    for (uint16_t id = 0; id < exchanges_.size(); ++id) {
      exchanges_[id].init(id, send_window(), write_cb_, write_cb_param_, 0, 0);
      assert(exchanges_[id].id() == id);
      assert(exchanges_[id].send_window() == send_window());
    }
  }

  rap::exchange* get_exchange(int id) {
    if (id < 0 || id >= exchanges_.size()) return 0;
    return &exchanges_[id];
  }

  void process_conn(const rap_frame* f) { assert(!"TODO!"); }

  // consume up to 'len' bytes of data from 'p',
  // returns the number of bytes consumed, or less if there is an error.
  int read_stream(const char* src_buf, int src_len) {
    if (!src_buf || src_len < 0) return 0;

    const char* src_ptr = src_buf;
    const char* src_end = src_ptr + src_len;

    while (src_ptr < src_end) {
      // make sure we have header
      while (frame_ptr_ < frame_buf_ + rap_frame_header_size) {
        assert(src_ptr < src_end);
        *frame_ptr_++ = *src_ptr++;
        if (src_ptr >= src_end) return (int)(src_ptr - src_buf);
      }

      // copy data until frame is complete
      size_t frame_len = rap_frame::needed_bytes(frame_buf_);
      assert(frame_len <= sizeof(frame_buf_));
      const char* frame_end = frame_buf_ + frame_len;
      assert(frame_end <= frame_buf_ + sizeof(frame_buf_));
      while (src_ptr < src_end && frame_ptr_ < frame_end) {
        *frame_ptr_++ = *src_ptr++;
      }

      if (frame_ptr_ < frame_end) return (int)(src_ptr - src_buf);

      // frame completed
      const rap_frame* f = reinterpret_cast<const rap_frame*>(frame_buf_);
      uint16_t id = f->header().id();
      if (id == rap_conn_exchange_id) {
        process_conn(f);
      } else if (id < exchanges_.size()) {
        error ec = rap_err_ok;
        exchanges_[id].process_frame(
            f, static_cast<int>(frame_ptr_ - frame_buf_), ec);
      } else {
      // exchange id out of range
#ifndef NDEBUG
        fprintf(stderr,
                "rap::conn::read_stream_ok(): exchange id %04x out of range\n",
                id);
#endif
        frame_ptr_ = frame_buf_;
        return (int)(src_ptr - src_buf);
      }
      frame_ptr_ = frame_buf_;
    }

    assert(src_ptr == src_end);
    return (int)(src_ptr - src_buf);
  }

  // new stream data available in the read buffer
  // void read_stream_ok(size_t bytes_transferred);

  // writes any buffered data to the stream using conn_t::write_stream()
  void write_some();

  const rap::exchange& exchange(uint16_t id) const { return exchanges_[id]; }
  rap::exchange& exchange(uint16_t id) { return exchanges_[id]; }

  rap::error write(const char* src_ptr, int src_len) const {
    if (!src_ptr || src_len < 0 || !write_cb_) return rap_err_invalid_parameter;
    return write_cb_(write_cb_param_, src_ptr, src_len) < 0
               ? rap_err_payload_too_big
               : rap_err_ok;
  }

  int16_t send_window() const { return max_send_window; }

 private:
  rap_write_cb_t write_cb_;
  void* write_cb_param_;
  char frame_buf_[rap_frame_max_size];
  char* frame_ptr_;
  std::vector<rap::exchange> exchanges_;
};

}  // namespace rap

#endif  // RAP_CONN_HPP
