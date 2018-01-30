#ifndef RAP_EXCHANGE_HPP
#define RAP_EXCHANGE_HPP

#include <cstdint>
#include <streambuf>
#include <vector>

#include "rap.hpp"
#include "rap_conn.hpp"
#include "rap_frame.hpp"
#include "rap_reader.hpp"

namespace rap {

class exchange : public std::streambuf {
 public:
  explicit exchange(const exchange& other);
  explicit exchange(conn& conn);
  ~exchange();

  const rap::header& header() const {
    return *reinterpret_cast<const rap::header*>(buf_.data());
  }

  rap::header& header() { return *reinterpret_cast<rap::header*>(buf_.data()); }

  error write_frame(const frame* f);

  error process_frame(const frame* f);

  // implement this in your own exchange_t
  error process_head(reader& r) { return r.error(); }

  // implement this in your own exchange_t
  error process_body(reader& r) { return r.error(); }

  // implement this in your own exchange_t
  error process_final(reader& r) { return r.error(); }

  /*
    exchange_t &self() { return *static_cast<exchange_t *>(this); }
    const exchange_t &self() const {
          return *static_cast<const exchange_t *>(this);
    }
    */
  rap::conn& conn() const { return conn_; }
  uint16_t id() const { return id_; }
  int16_t send_window() const { return send_window_; }

 protected:
  int_type overflow(int_type ch);
  int sync();

 private:
  rap::conn& conn_;
  framelink* queue_;
  uint16_t id_;
  int16_t send_window_;
  std::vector<char> buf_;

  void start_write();
  error write_queue();
  error send_frame(const frame* f);
  error send_ack();
};

}  // namespace rap

#endif  // RAP_EXCHANGE_HPP
