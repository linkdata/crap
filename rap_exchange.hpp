#ifndef RAP_EXCHANGE_HPP
#define RAP_EXCHANGE_HPP

#include <cstdint>

#include "rap.hpp"
#include "rap_callbacks.h"
#include "rap_frame.h"

enum { rap_max_exchange_id = rap_conn_exchange_id - 1 };

namespace rap {

class exchange {
 public:
  explicit exchange()
      : write_cb_(0),
        write_cb_param_(0),
        exchange_cb_(0),
        exchange_cb_param_(0),
        queue_(NULL),
        id_(rap_conn_exchange_id),
        send_window_(0) {}

  int init(uint16_t id, int send_window, rap_write_cb_t write_cb,
           void* write_cb_param, rap_exchange_cb_t exchange_cb,
           void* exchange_cb_param) {
    if (!write_cb || id > rap_max_exchange_id) return -1;
    write_cb_ = write_cb;
    write_cb_param_ = write_cb_param;
    exchange_cb_ = exchange_cb;
    exchange_cb_param_ = exchange_cb_param;
    queue_ = NULL;
    id_ = id;
    send_window_ = send_window;

    ack_[0] = '\0';
    ack_[1] = '\0';
    ack_[2] = static_cast<char>(id_ >> 8);
    ack_[3] = static_cast<char>(id_);

    return 0;
  }

  virtual ~exchange() {
    while (queue_) framelink::dequeue(&queue_);
    id_ = rap_conn_exchange_id;
  }

  int set_callback(rap_exchange_cb_t exchange_cb, void* exchange_cb_param) {
    exchange_cb_ = exchange_cb;
    exchange_cb_param_ = exchange_cb_param;
    return 0;
  }

  int get_callback(rap_exchange_cb_t* p_exchange_cb,
                   void** p_exchange_cb_param) const {
    *p_exchange_cb = exchange_cb_;
    *p_exchange_cb_param = exchange_cb_param_;
    return 0;
  }

  error write_frame(const rap_frame* f) {
    if (error e = write_queue()) return e;
    if (send_window_ < 1) {
#ifndef NDEBUG
      fprintf(stderr, "exchange %04x waiting for ack\n", id_);
#endif
      framelink::enqueue(&queue_, f);
      return rap_err_ok;
    }
    return send_frame(f);
  }

  bool process_frame(const rap_frame* f, int len, error& ec) {
    if (!f->header().has_payload()) {
      ++send_window_;
      ec = write_queue();
      return false;
    }
    if (!f->header().is_final()) {
      if ((ec = send_ack())) return false;
    }

    bool had_head = f->header().has_head();
    if (exchange_cb_) exchange_cb_(exchange_cb_param_, this, f, len);
    return had_head;
  }

  uint16_t id() const { return id_; }
  int16_t send_window() const { return send_window_; }
  int write(const char* p, int n) const {
    return write_cb_(write_cb_param_, p, n);
  }

 private:
  rap_write_cb_t write_cb_;
  void* write_cb_param_;
  rap_exchange_cb_t exchange_cb_;
  void* exchange_cb_param_;
  framelink* queue_;
  uint16_t id_;
  int16_t send_window_;
  char ack_[4];

  error write_queue() {
    while (queue_ != NULL) {
      rap_frame* f = reinterpret_cast<rap_frame*>(queue_ + 1);
      if (!f->header().is_final() && send_window_ < 1) return rap_err_ok;
      if (error e = send_frame(f)) return e;
      framelink::dequeue(&queue_);
    }
    return rap_err_ok;
  }

  error send_frame(const rap_frame* f) {
    if (write(f->data(), static_cast<int>(f->size()))) {
      assert(!"rap::exchange::send_frame(): conn_.write() failed");
      return rap_err_output_buffer_too_small;
    }
    if (!f->header().is_final()) --send_window_;
    return rap_err_ok;
  }

  error send_ack() {
    return write(ack_, sizeof(ack_)) ? rap_err_output_buffer_too_small
                                     : rap_err_ok;
  }
};

}  // namespace rap

#endif  // RAP_EXCHANGE_HPP
