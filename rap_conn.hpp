#ifndef RAP_CONN_HPP
#define RAP_CONN_HPP

#include <atomic>
#include <cassert>
#include <cstdint>
#include <vector>

#include "rap.hpp"
#include "rap_callbacks.h"
#include "rap_frame.h"
#include "rap_text.hpp"

namespace rap {

class conn {
public:
    static const int16_t max_send_window = 8;

    explicit conn(void* conn_user_data, rap_conn_write_cb_t conn_write_cb)
        : conn_user_data_(conn_user_data)
        , conn_write_cb_(conn_write_cb)
        , frame_ptr_(frame_buf_)
    {
    }
    virtual ~conn() {}

    int write(const char* p, int n) const
    {
        return conn_write_cb_(conn_user_data_, p, n);
    }

    // consume up to 'len' bytes of data from 'p',
    // returns the number of bytes consumed, or less if there is an error.

    // consume up to 'len' bytes of data from 'p',
    // returns the number of bytes consumed, or less if there is an error.
    int recv(const char* src_buf, int src_len)
    {
        if (!src_buf || src_len < 0)
            return 0;

        const char* src_ptr = src_buf;
        const char* src_end = src_ptr + src_len;

        while (src_ptr < src_end) {
            // make sure we have header
            while (frame_ptr_ < frame_buf_ + rap_frame_header_size) {
                assert(src_ptr < src_end);
                *frame_ptr_++ = *src_ptr++;
                if (src_ptr >= src_end)
                    return (int)(src_ptr - src_buf);
            }

            // copy data until frame is complete
            size_t frame_len = rap_frame::needed_bytes(frame_buf_);
            assert(frame_len <= sizeof(frame_buf_));
            const char* frame_end = frame_buf_ + frame_len;
            assert(frame_end <= frame_buf_ + sizeof(frame_buf_));
            while (src_ptr < src_end && frame_ptr_ < frame_end) {
                *frame_ptr_++ = *src_ptr++;
            }

            if (frame_ptr_ < frame_end)
                return (int)(src_ptr - src_buf);

            // frame completed
            const rap_frame* f = reinterpret_cast<const rap_frame*>(frame_buf_);
            uint16_t id = f->header().id();
            if (id == rap_conn_exchange_id) {
                process_conn(f);
            } else {
                error ec = rap_err_ok;
                process_frame(id, f, static_cast<int>(frame_ptr_ - frame_buf_), ec);
            }
            frame_ptr_ = frame_buf_;
        }
        assert(src_ptr == src_end);
        return (int)(src_ptr - src_buf);
    }

    virtual rap::exchange* get_exchange(rap_exch_id id) = 0;

protected:
    virtual void process_conn(const rap_frame* f) = 0;
    virtual bool process_frame(rap_exch_id id, const rap_frame* f, int len, rap::error& ec) = 0;

private:
    void* conn_user_data_;
    rap_conn_write_cb_t conn_write_cb_;
    char frame_buf_[rap_frame_max_size];
    char* frame_ptr_;
};

} // namespace rap

#endif // RAP_CONN_IMPL_HPP
