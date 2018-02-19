#ifndef RAP_CONNBASE_HPP
#define RAP_CONNBASE_HPP

#include "rap.hpp"
#include "rap_frame.h"
#include "rap_header.h"
#include "rap_stats.hpp"

enum { rap_max_exchange_id = rap_conn_exchange_id - 1 };

namespace rap {

class connbase {
 public:
  typedef int (*write_cb_t)(void*, const char*, int);
  typedef int (*exchange_cb_t)(void*, rap::exchange*, const rap_frame*, int);

  static const int16_t max_send_window = 8;

  explicit connbase(write_cb_t write_cb, void* write_cb_param)
      : next_id_(0), write_cb_(write_cb), write_cb_param_(write_cb_param) {
    assert(write_cb_ != 0);
  }
  virtual ~connbase() {}

  // used while constructing when initializing vector of exchanges
  uint16_t next_id() {
    if (next_id_ > rap_max_exchange_id) return rap_conn_exchange_id;
    return next_id_++;
  }

  rap::error write(const char* src_ptr, int src_len) const {
    if (!src_ptr || src_len < 0 || !write_cb_) return rap_err_invalid_parameter;
    return write_cb_(write_cb_param_, src_ptr, src_len) < 0
               ? rap_err_payload_too_big
               : rap_err_ok;
  }

  int16_t send_window() const { return max_send_window; }

 protected:
  uint16_t next_id_;
  write_cb_t write_cb_;
  void* write_cb_param_;
};

}  // namespace rap

#endif  // RAP_CONNBASE_HPP
