#ifndef RAP_KVV_HPP
#define RAP_KVV_HPP

#include "rap.hpp"
#include "rap_frame.h"
#include "rap_reader.hpp"
#include "rap_text.hpp"
#include "rap_writer.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace rap {

class kvv {
 public:
  kvv() {}

  kvv(const kvv& other) : data_(other.data_) {}

  kvv(reader& r) {
    for (;;) {
      text key(r.read_text());
      if (key.is_null()) break;
      data_.push_back(key);
      for (;;) {
        text val(r.read_text());
        data_.push_back(val);
        if (val.is_null()) break;
      }
    }
  }

  size_t size() const { return data_.size(); }
  text at(size_t n) const { return data_.at(n); }
  size_t find(const char* key) const {
    size_t i = 0;
    while (i < data_.size()) {
      if (data_.at(i).is_null()) break;
      if (data_.at(i) == key) return i + 1;
      while (!data_.at(i).is_null()) ++i;
    }
    return 0;
  }

  const rap::writer& operator>>(const rap::writer& w) const {
    for (size_t i = 0; i < size(); ++i) w << at(i);
    w << text();
    return w;
  }

 protected:
  std::vector<text> data_;
};

class query : public kvv {
 public:
  query() : kvv() {}
  query(reader& r) : kvv(r) {}
  void render(string_t& out) const {
    char prefix = '?';
    for (size_t i = 0; i < size(); ++i) {
      text key(at(i));
      if (key.is_null()) break;
      for (;;) {
        if (++i >= size()) break;
        text val(at(i));
        if (val.is_null()) break;
        out += prefix;
        key.render(out);
        prefix = '&';
        if (!val.empty()) {
          out += '=';
          val.render(out);
        }
      }
    }
  }
};

class headers : public kvv {
 public:
  headers() : kvv() {}
  headers(reader& r) : kvv(r) {}
  void render(string_t& out) const {
    for (size_t i = 0; i < size(); ++i) {
      text key(at(i));
      if (key.is_null()) break;
      for (;;) {
        if (++i >= size()) break;
        text val(at(i));
        if (val.is_null()) break;
        if (!val.empty()) {
          key.render(out);
          out += ": ";
          val.render(out);
          out += '\n';
        }
      }
    }
  }
};

inline const rap::writer& operator<<(const rap::writer& w,
                                     const rap::kvv& kvv) {
  return kvv >> w;
}

}  // namespace rap

#endif  // RAP_KVV_HPP
