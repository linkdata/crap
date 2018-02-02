#ifndef RAP_STATS_HPP
#define RAP_STATS_HPP

#include <atomic>

namespace rap {

class stats {
 public:
  stats()
      : head_count(0),
        read_iops(0),
        read_bytes(0),
        write_iops(0),
        write_bytes(0) {}

  std::atomic<uint64_t> head_count;
  std::atomic<uint64_t> read_iops;
  std::atomic<uint64_t> read_bytes;
  std::atomic<uint64_t> write_iops;
  std::atomic<uint64_t> write_bytes;

  void add_bytes_read(uint64_t n) {
    read_iops++;
    read_bytes += n;
  }

  void add_bytes_written(uint64_t n) {
    write_iops++;
    write_bytes += n;
  }

  void aggregate_into(stats& other) {
    other.head_count += head_count.exchange(0);
    other.read_iops += read_iops.exchange(0);
    other.read_bytes += read_bytes.exchange(0);
    other.write_iops += write_iops.exchange(0);
    other.write_bytes += write_bytes.exchange(0);
  }
};

}  // namespace rap

#endif  // RAP_STATS_HPP
