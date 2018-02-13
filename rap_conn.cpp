#include "rap_conn.hpp"

namespace rap {

conn::conn(write_cb_t cb, void* cb_param)
    : next_id_(0),
      buf_ptr_(buffer_),
      buf_end_(buf_ptr_),
      write_cb_(cb),
      write_cb_param_(cb_param),
      exchanges_(rap_max_exchange_id + 1, rap::exchange(*this)) {
  // assert correctly initialized exchange vector
  assert(exchanges_.size() == rap_max_exchange_id + 1);
  for (uint16_t id = 0; id < exchanges_.size(); ++id) {
    assert(&(exchanges_[id].conn()) == this);
    assert(exchanges_[id].id() == id);
    assert(exchanges_[id].send_window() == send_window());
  }
}

// consume up to 'len' bytes of data from 'p',
// return the number of bytes consumed, or a
// negative value on error.
int conn::read_stream(const char* buf_ptr, int bytes_transferred) {
  if (!buf_ptr || bytes_transferred < 0) {
    return -1;
  }

  const char* src_ptr = buf_ptr;
  const char* end_ptr = src_ptr + bytes_transferred;
  stats_.add_bytes_read(bytes_transferred);

  while (src_ptr + rap_frame_header_size <= end_ptr) {
    size_t src_len = frame::needed_bytes(src_ptr);
    if (src_ptr + src_len > end_ptr) break;
    const frame* f = reinterpret_cast<const frame*>(src_ptr);
    uint16_t id = f->header().id();
    if (id == rap_conn_exchange_id) {
      process_conn(f);
    } else if (id < exchanges_.size()) {
      exchanges_[f->header().id()].process_frame(f);
    } else {
    // exchange id out of range
#ifndef NDEBUG
      fprintf(stderr,
              "rap::conn::read_stream_ok(): exchange id %04x out of range\n",
              id);
#endif
      break;
    }
    src_ptr += src_len;
    assert(src_ptr <= buf_end_);
  }

  return static_cast<int>(src_ptr - buf_ptr);

/*
  // move trailing unprocessed input to the start
  if (src_ptr > buffer_) {
    char* dst_ptr = buffer_;
    while (src_ptr < buf_end_) *(dst_ptr++) = *src_ptr++;
    buf_end_ = dst_ptr;
  }
  */
}

// new stream data available in the read buffer
/*
void conn::read_stream_ok(size_t bytes_transferred) {
  read_stream(buffer_, bytes_transferred);
}
*/

// buffered write to the network stream
rap::error conn::write(const char* src_ptr, int src_len) {
  if (!src_ptr || src_len<0 || !write_cb_)
    return rap_err_invalid_parameter;
  return write_cb_(write_cb_param_, src_ptr, src_len) < 0 ? rap_err_payload_too_big : rap_err_ok;
/*
  buf_towrite_.insert(buf_towrite_.end(), src_ptr, src_ptr + src_len);
  write_some();
  **/
}

void conn::write_stream_ok(size_t bytes_transferred) {
  assert(bytes_transferred == buf_writing_.size());
  stats_.add_bytes_written(bytes_transferred);
  buf_writing_.clear();
  return;
}

// used while constructing when initializing vector of exchanges
uint16_t conn::next_id() {
  if (next_id_ > rap_max_exchange_id) return rap_conn_exchange_id;
  return next_id_++;
}

}  // namespace rap
