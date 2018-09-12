#include "rap_conn.hpp"
#include "rap_exchange.hpp"

namespace rap {

conn::conn(void* conn_user_data,
    rap_conn_write_cb_t conn_write_cb,
    rap_conn_exch_init_cb_t conn_exch_init_cb)
    : conn_user_data_(conn_user_data)
    , conn_write_cb_(conn_write_cb)
    , frame_ptr_(frame_buf_)
    , exchanges_(rap_max_exchange_id + 1)
{
    // assert correctly initialized exchange vector
    assert(exchanges_.size() == rap_max_exchange_id + 1);
    for (rap_exch_id id = 0; id < exchanges_.size(); ++id) {
        exchanges_[id].init(
            static_cast<rap_conn*>(this), id, max_send_window, 0, 0);
        assert(exchanges_[id].id() == id);
        if (conn_exch_init_cb != 0)
            conn_exch_init_cb(conn_user_data, id, &exchanges_[id]);
    }
}

// consume up to 'len' bytes of data from 'p',
// returns the number of bytes consumed, or less if there is an error.
int conn::recv(const char* src_buf, int src_len)
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
        } else if (id < exchanges_.size()) {
            error ec = rap_err_ok;
            exchanges_[id].process_frame(
                f, static_cast<int>(frame_ptr_ - frame_buf_), ec);
        } else {
        // exchange id out of range
#ifndef NDEBUG
            fprintf(stderr,
                "rap::conn::read_stream_ok(): exchange id %04x out of range\n",
                id);
#endif
            frame_ptr_ = frame_buf_;
            return (int)(src_ptr - src_buf);
        }
        frame_ptr_ = frame_buf_;
    }

    assert(src_ptr == src_end);
    return (int)(src_ptr - src_buf);
}

} // namespace rap
