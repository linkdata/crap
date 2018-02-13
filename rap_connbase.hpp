#ifndef RAP_CONNBASE_HPP
#define RAP_CONNBASE_HPP

#include "rap.hpp"
#include "rap_stats.hpp"

namespace rap {

class connbase {
 public:
  static const int16_t max_send_window = 8;

  connbase() {}
  virtual ~connbase() {}
  virtual uint16_t next_id() = 0;
  virtual rap::error write(const char* src_ptr, int src_len) = 0;
  virtual int16_t send_window() const { return max_send_window; }
  rap::stats& stats() { return stats_; }
  const rap::stats& stats() const { return stats_; }

 protected:
  rap::stats stats_;
};

}  // namespace rap

#endif  // RAP_CONNBASE_HPP
