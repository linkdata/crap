#include "rap_exchange.hpp"

namespace rap {
exchange::exchange(const exchange& other)
    : conn_(other.conn_),
      queue_(NULL),
      id_(other.id_ == rap_conn_exchange_id ? conn_.next_id() : other.id_),
      send_window_(other.send_window_) {
  start_write();
}

exchange::exchange(rap::conn& conn)
    : conn_(conn),
      queue_(NULL),
      id_(rap_conn_exchange_id),
      send_window_(conn.send_window()) {
  start_write();
}

exchange::~exchange() {
  while (queue_) framelink::dequeue(&queue_);
  id_ = rap_conn_exchange_id;
}

error exchange::write_frame(const frame* f) {
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

error exchange::process_frame(const frame* f) {
  if (!f->header().has_payload()) {
    ++send_window_;
    write_queue();
    return rap_err_ok;
  }
  if (!f->header().is_final()) send_ack();
  reader r(f);
  if (f->header().has_head()) {
    conn_.stats().head_count++;
    if (error e = process_head(r)) return e;
  }
  if (f->header().has_body()) {
    if (error e = process_body(r)) return e;
  }
  if (f->header().is_final()) {
    if (error e = process_final(r)) return e;
  }
  return rap_err_ok;
}

/*
exchange_t &self() { return *static_cast<exchange_t *>(this); }
const exchange_t &self() const {
return *static_cast<const exchange_t *>(this);
}
*/

exchange::int_type exchange::overflow(int_type ch) {
  if (buf_.size() < rap_frame_max_size) {
    size_t new_size = buf_.size() * 2;
    if (new_size > rap_frame_max_size) new_size = rap_frame_max_size;
    buf_.resize(new_size);
    setp(buf_.data() + rap_frame_header_size, buf_.data() + buf_.size());
  }

  bool was_head = header().has_head();
  bool was_body = header().has_body();
  bool was_final = header().is_final();

  if (was_final && ch != traits_type::eof()) header().clr_final();

  if (sync() != 0) {
    if (was_final) header().set_final();
    return traits_type::eof();
  }

  if (was_body)
    header().set_body();
  else if (was_head)
    header().set_head();

  if (was_final) header().set_final();

  if (ch != traits_type::eof()) {
    *pptr() = ch;
    pbump(1);
  }

  return ch;
}

int exchange::sync() {
  header().set_size_value(pptr() - (buf_.data() + rap_frame_header_size));
  if (write_frame(reinterpret_cast<frame*>(buf_.data()))) {
    assert(!"rap::exchange::sync(): write_frame() failed");
    return -1;
  }
  start_write();
  return 0;
}

void exchange::start_write() {
  if (buf_.size() < 256) buf_.resize(256);
  buf_[0] = '\0';
  buf_[1] = '\0';
  buf_[2] = static_cast<char>(id_ >> 8);
  buf_[3] = static_cast<char>(id_);
  setp(buf_.data() + rap_frame_header_size, buf_.data() + buf_.size());
}

error exchange::write_queue() {
  while (queue_ != NULL) {
    frame* f = reinterpret_cast<frame*>(queue_ + 1);
    if (!f->header().is_final() && send_window_ < 1) return rap_err_ok;
    if (error e = send_frame(f)) return e;
    framelink::dequeue(&queue_);
  }
  return rap_err_ok;
}

error exchange::send_frame(const frame* f) {
  if (error e = conn_.write(f->data(), f->size())) {
    assert(!"rap::exchange::send_frame(): conn_.write() failed");
    return e;
  }
  if (!f->header().is_final()) --send_window_;
  return rap_err_ok;
}

error exchange::send_ack() {
  char buf[4];
  buf[0] = '\0';
  buf[1] = '\0';
  buf[2] = static_cast<char>(id_ >> 8);
  buf[3] = static_cast<char>(id_);
  return conn_.write(buf, 4);
}

}  // namespace rap
