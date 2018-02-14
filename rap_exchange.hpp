#ifndef RAP_EXCHANGE_HPP
#define RAP_EXCHANGE_HPP

#include <cstdint>
#include <streambuf>
#include <vector>

#include "rap.hpp"
#include "rap_connbase.hpp"
#include "rap_frame.h"
#include "rap_reader.hpp"
#include "rap_request.hpp"
#include "rap_response.hpp"
#include "rap_writer.hpp"

namespace rap {

class exchange : public std::streambuf {
 public:
  explicit exchange(const exchange& other)
      : conn_(other.conn_),
        queue_(NULL),
        id_(other.id_ == rap_conn_exchange_id ? conn_.next_id() : other.id_),
        send_window_(other.send_window_) {
    start_write();
  }

  explicit exchange(rap::connbase& conn)
      : conn_(conn),
        queue_(NULL),
        id_(rap_conn_exchange_id),
        send_window_(conn.send_window()) {
    start_write();
  }

  virtual ~exchange() {
    while (queue_) framelink::dequeue(&queue_);
    id_ = rap_conn_exchange_id;
  }

  const rap_header& header() const {
    return *reinterpret_cast<const rap_header*>(buf_.data());
  }

  rap_header& header() { return *reinterpret_cast<rap_header*>(buf_.data()); }

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
      if (ec = send_ack()) return false;
    }
    return true;
  }

  rap::error process_head(rap::reader& r) {
    if (r.read_tag() != rap::record::tag_http_request)
      return rap::rap_err_unknown_frame_type;
    rap::request req(r);
    req_echo_.clear();
    req.render(req_echo_);
    req_echo_ += '\n';
    header().set_head();
    int64_t content_length = req.content_length();
    if (content_length >= 0) content_length += req_echo_.size();
    rap::writer(*this) << rap::response(200, content_length);
    header().set_body();
    sputn(req_echo_.data(), req_echo_.size());
    return r.error();
  }

  rap::error process_body(rap::reader& r) {
    assert(r.size() > 0);
    header().set_body();
    sputn(r.data(), r.size());
    r.consume();
    return r.error();
  }

  rap::error process_final(rap::reader& r) {
    assert(r.size() == 0);
    header().set_final();
    pubsync();
    return r.error();
  }

  /*
    exchange_t &self() { return *static_cast<exchange_t *>(this); }
    const exchange_t &self() const {
          return *static_cast<const exchange_t *>(this);
    }
    */
  rap::connbase& conn() const { return conn_; }
  uint16_t id() const { return id_; }
  int16_t send_window() const { return send_window_; }

 protected:
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
    if (write_frame(reinterpret_cast<rap_frame*>(buf_.data()))) {
      assert(!"rap::exchange::sync(): write_frame() failed");
      return -1;
    }
    start_write();
    return 0;
  }

 private:
  rap::connbase& conn_;
  framelink* queue_;
  uint16_t id_;
  int16_t send_window_;
  std::vector<char> buf_;
  rap::string_t req_echo_; 

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
      rap_frame* f = reinterpret_cast<rap_frame*>(queue_ + 1);
      if (!f->header().is_final() && send_window_ < 1) return rap_err_ok;
      if (error e = send_frame(f)) return e;
      framelink::dequeue(&queue_);
    }
    return rap_err_ok;
  }

  error exchange::send_frame(const rap_frame* f) {
    if (error e = conn_.write(f->data(), static_cast<int>(f->size()))) {
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
};

}  // namespace rap

#endif  // RAP_EXCHANGE_HPP
