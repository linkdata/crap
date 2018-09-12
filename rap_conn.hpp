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

    explicit conn(void* conn_user_data, rap_conn_write_cb_t conn_write_cb, rap_conn_exch_init_cb_t conn_exch_init_cb);

    rap::exchange* get_exchange(int id)
    {
        if (id < 0 || id >= exchanges_.size())
            return 0;
        return &exchanges_[id];
    }

    void process_conn(const rap_frame* f) { assert(!"TODO!"); }

    int write(const char* p, int n) const
    {
        return conn_write_cb_(conn_user_data_, p, n);
    }

    // consume up to 'len' bytes of data from 'p',
    // returns the number of bytes consumed, or less if there is an error.
    int recv(const char* src_buf, int src_len);

private:
    void* conn_user_data_;
    rap_conn_write_cb_t conn_write_cb_;
    char frame_buf_[rap_frame_max_size];
    char* frame_ptr_;
    std::vector<rap::exchange> exchanges_;
};

} // namespace rap

#endif // RAP_CONN_HPP
