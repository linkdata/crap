#ifndef RAP_READER_HPP
#define RAP_READER_HPP

#include "rap.hpp"
#include "rap_frame.h"
#include "rap_record.hpp"
#include "rap_route.hpp"
#include "rap_text.hpp"

#include <cassert>
#include <cstdint>

namespace rap {

class reader {
public:
    reader(const rap_frame* f)
        : frame_(f)
        , src_ptr_(f->payload())
        , src_end_(f->payload() + f->payload_size())
        , error_(rap_err_ok)
    {
    }

    char read_char()
    {
        assert(!error_);
        return *src_ptr_++;
    }

    unsigned char read_uchar()
    {
        assert(!error_);
        return static_cast<unsigned char>(*src_ptr_++);
    }

    record::tag read_tag() { return error_ ? record::tag_invalid : read_char(); }

    uint64_t read_uint64()
    {
        if (!error_) {
            uint64_t accum = 0;
            unsigned char s = 0;
            while (src_ptr_ < src_end_) {
                unsigned char uch = read_uchar();
                if (uch < 0x80) {
                    return accum | uint64_t(uch) << s;
                }
                accum |= uint64_t(uch & 0x7f) << s;
                s += 7;
            }
            set_error(rap_err_incomplete_number);
        }
        return 0;
    }

    int64_t read_int64()
    {
        uint64_t ux = read_uint64();
        int64_t ix = static_cast<int64_t>(ux >> 1);
        if (ux & 1)
            ix = ~ix;
        return ix;
    }

    size_t read_length()
    {
        if (!error_) {
            if (src_ptr_ + 1 <= src_end_) {
                size_t length = static_cast<size_t>(read_uchar());
                if (length < 0x80)
                    return length;
                if (src_ptr_ + 1 <= src_end_)
                    return ((length & 0x7f) << 8) | static_cast<size_t>(read_uchar());
            }
            set_error(rap_err_incomplete_length);
        }
        return 0;
    }

    text read_text()
    {
        if (!error_) {
            if (size_t length = read_length()) {
                if (src_ptr_ + length <= src_end_) {
                    const char* p = src_ptr_;
                    src_ptr_ += length;
                    return text(p, length);
                }
                set_error(rap_err_incomplete_string);
            } else if (!error_) {
                return text(read_uchar());
            }
        }
        return text();
    }

    bool read_string(string_t& out)
    {
        text txt = read_text();
        if (txt.is_null())
            return false;
        out.append(txt.data(), txt.size());
        return true;
    }

    string_t read_string()
    {
        string_t retv;
        read_string(retv);
        return retv;
    }

    route read_route()
    {
        if (!error_) {
            if (size_t length = read_length()) {
				// TODO: routes
                return route();
            } else if (!error_) {
                return route(read_text());
            }
        }
        return route();
    }

    void consume(size_t n)
    {
        assert(src_ptr_ + n <= src_end_);
        src_ptr_ += n;
    }

    void consume() { src_ptr_ = src_end_; }

    const rap_frame* frame() const { return frame_; }
    uint16_t id() const { return frame_->header().id(); }
    bool eof() const { return error_ || src_ptr_ >= src_end_; }
    rap::error error() const { return error_; }
    const char* data() const { return src_ptr_; }
    size_t size() const { return src_end_ - src_ptr_; }

private:
    const rap_frame* frame_;
    const char* src_ptr_;
    const char* src_end_;
    rap::error error_;

    void set_error(rap::error e)
    {
#ifndef NDEBUG
        fprintf(stderr, "rap::reader::set_error(%d)\n", e);
#endif
        error_ = e;
    }
};

} // namespace rap

#endif // RAP_READER_HPP
