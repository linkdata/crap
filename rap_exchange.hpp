#ifndef RAP_EXCHANGE_HPP
#define RAP_EXCHANGE_HPP

#include <cstdint>

#include "rap.hpp"
#include "rap_connbase.hpp"
#include "rap_frame.h"

namespace rap {

class exchange {
 public:
  explicit exchange()
      : conn_(NULL),
        queue_(NULL),
        id_(rap_conn_exchange_id),
        send_window_(0) {}

  void init(const rap::connbase* conn, uint16_t id) {
    conn_ = conn;
    id_ = id;
    send_window_ = conn->send_window();

    ack_[0] = '\0';
    ack_[1] = '\0';
    ack_[2] = static_cast<char>(id_ >> 8);
    ack_[3] = static_cast<char>(id_);
  }

  virtual ~exchange() {
    while (queue_) framelink::dequeue(&queue_);
    id_ = rap_conn_exchange_id;
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

  bool process_frame(const rap_frame* f, error& ec) {
    if (!f->header().has_payload()) {
      ++send_window_;
      ec = write_queue();
      return false;
    }
    if (!f->header().is_final()) {
      if ((ec = send_ack())) return false;
    }
    return true;
  }

  const rap::connbase* conn() const { return conn_; }
  uint16_t id() const { return id_; }
  int16_t send_window() const { return send_window_; }

 private:
  const rap::connbase* conn_;
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
    if (error e = conn_->write(f->data(), static_cast<int>(f->size()))) {
      assert(!"rap::exchange::send_frame(): conn_.write() failed");
      return e;
    }
    if (!f->header().is_final()) --send_window_;
    return rap_err_ok;
  }

  error send_ack() {
    return conn_->write(ack_, sizeof(ack_));
  }
};

}  // namespace rap

#endif  // RAP_EXCHANGE_HPP
