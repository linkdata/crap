#ifndef RAP_CONN_HPP
#define RAP_CONN_HPP

#include <atomic>
#include <cassert>
#include <cstdint>
#include <vector>

#include "rap.hpp"
#include "rap_connbase.hpp"
#include "rap_exchange.hpp"
#include "rap_frame.hpp"
#include "rap_text.hpp"

namespace rap {

class conn : connbase {
 public:
  typedef int (*write_cb_t)(void*, const char*, int);
  typedef int (*frame_cb_t)(void*, const char*, int);

  explicit conn::conn(void* cb_param, write_cb_t write_cb, frame_cb_t frame_cb)
      : next_id_(0),
        frame_ptr_(frame_buf_),
        cb_param_(cb_param),
        write_cb_(write_cb),
        frame_cb_(frame_cb),
        exchanges_(rap_max_exchange_id + 1, rap::exchange(*this)) {
    assert(write_cb != 0);
    assert(frame_cb != 0);
    // assert correctly initialized exchange vector
    assert(exchanges_.size() == rap_max_exchange_id + 1);
    for (uint16_t id = 0; id < exchanges_.size(); ++id) {
      assert(&(exchanges_[id].conn()) == this);
      assert(exchanges_[id].id() == id);
      assert(exchanges_[id].send_window() == send_window());
    }
  }

  void process_conn(const frame* f) { assert(!"TODO!"); }

  // consume up to 'len' bytes of data from 'p',
  // return the number of bytes consumed, or a
  // negative value on error.
  int read_stream(const char* src_buf, int src_len) {
    int retv = 0;
    if (!src_buf || src_len < 0) return -1;

    const char* src_ptr = src_buf;
    const char* src_end = src_ptr + src_len;

    while (src_ptr < src_end) {
      // make sure we have header
      while (frame_ptr_ < frame_buf_ + rap_frame_header_size) {
        assert(src_ptr < src_end);
        *frame_ptr_++ = *src_ptr++;
        if (src_ptr >= src_end) return retv;
      }

      // copy data until frame is complete
      size_t frame_len = frame::needed_bytes(frame_buf_);
      assert(frame_len <= sizeof(frame_buf_));
      const char* frame_end = frame_buf_ + frame_len;
      assert(frame_end <= frame_buf_ + sizeof(frame_buf_));
      while (frame_ptr_ < frame_end) {
        assert(src_ptr < src_end);
        *frame_ptr_++ = *src_ptr++;
        if (src_ptr >= src_end) return retv;
      }

      // frame completed
      const frame* f = reinterpret_cast<const frame*>(frame_buf_);
      uint16_t id = f->header().id();
      if (id == rap_conn_exchange_id) {
        process_conn(f);
      } else if (id < exchanges_.size()) {
        exchanges_[id].process_frame(f);
        /*
        int ec = frame_cb_(cb_param_, frame_buf_, static_cast<int>(frame_ptr_ -
        frame_buf_)); if (ec < 0) { frame_ptr_ = frame_buf_; return ec;
        }
        */
        retv++;
      } else {
      // exchange id out of range
#ifndef NDEBUG
        fprintf(stderr,
                "rap::conn::read_stream_ok(): exchange id %04x out of range\n",
                id);
#endif
        frame_ptr_ = frame_buf_;
        return -1;
      }
      frame_ptr_ = frame_buf_;
    }

    assert(src_ptr == src_end);
    return retv;
  }

  // new stream data available in the read buffer
  // void read_stream_ok(size_t bytes_transferred);

  // buffered write to the network stream
  rap::error write(const char* src_ptr, int src_len) {
    if (!src_ptr || src_len < 0 || !write_cb_) return rap_err_invalid_parameter;
    return write_cb_(cb_param_, src_ptr, src_len) < 0 ? rap_err_payload_too_big
                                                      : rap_err_ok;
    /*
      buf_towrite_.insert(buf_towrite_.end(), src_ptr, src_ptr + src_len);
      write_some();
      **/
  }

  // writes any buffered data to the stream using conn_t::write_stream()
  void write_some();

  void write_stream_ok(size_t bytes_transferred);

  // used while constructing when initializing vector of exchanges
  uint16_t next_id() {
    if (next_id_ > rap_max_exchange_id) return rap_conn_exchange_id;
    return next_id_++;
  }

  const rap::exchange& exchange(uint16_t id) const { return exchanges_[id]; }
  rap::exchange& exchange(uint16_t id) { return exchanges_[id]; }

 private:
  uint16_t next_id_;
  char frame_buf_[rap_frame_max_size];
  char* frame_ptr_;
  std::vector<rap::exchange> exchanges_;
  rap::stats stats_;
  void* cb_param_;
  write_cb_t write_cb_;
  frame_cb_t frame_cb_;
};

}  // namespace rap

#endif  // RAP_CONN_HPP
