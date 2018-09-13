#ifndef RAP_LINK_HPP
#define RAP_LINK_HPP

#include <cassert>
#include <cstdint>

#include "rap.hpp"
#include "rap_callbacks.h"
#include "rap_frame.h"
#include "rap_text.hpp"

namespace rap {

/**
 * @brief link maintains the per-network-link state and functionality to
 *        serve both the #conn and #exchange classes.
 * 
 */
class link {
public:
    explicit link(void* conn_user_data, rap_conn_write_cb_t conn_write_cb)
        : conn_user_data_(conn_user_data)
        , conn_write_cb_(conn_write_cb)
        , frame_ptr_(frame_buf_)
    {
    }

    virtual ~link() {}

    /**
     * @brief write() calls the #conn_write_cb callback function, 
     * writing @a src_len bytes from @a src_buf to the network.
     * 
     * @param src_buf the bytes to write, must not be NULL
     * @param src_len the number of bytes to write
     * @return int return value from #conn_write_cb
     */
    int write(const char* src_buf, int src_len) const
    {
        return conn_write_cb_(conn_user_data(), src_buf, src_len);
    }

    /**
     * @brief consume up to @a src_len bytes of data from @a src_buf
     * 
     * @param src_buf the bytes to read, must not be NULL
     * @param src_len number of bytes to read
     * @return int number of bytes consumed, or less if there is an error.
     */
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

protected:
    void* conn_user_data() const { return conn_user_data_; }
    virtual void process_conn(const rap_frame* f) = 0;
    virtual bool process_frame(rap_exch_id id, const rap_frame* f, int len, rap::error& ec) = 0;

private:
    void* conn_user_data_;
    rap_conn_write_cb_t conn_write_cb_;
    char frame_buf_[rap_frame_max_size];
    char* frame_ptr_;
};

} // namespace rap

#endif // RAP_LINK_HPP