#ifndef RAP_CONN_IMPL_HPP
#define RAP_CONN_IMPL_HPP

#include <atomic>
#include <cassert>
#include <cstdint>
#include <vector>

#include "rap.hpp"
#include "rap_callbacks.h"
#include "rap_frame.h"
#include "rap_text.hpp"

#include "rap_conn.hpp"
#include "rap_exchange.hpp"

namespace rap {

class conn_impl : public conn {
public:
    explicit conn_impl(void* conn_user_data,
        rap_conn_write_cb_t conn_write_cb,
        rap_conn_exch_init_cb_t conn_exch_init_cb)
        : conn(conn_user_data, conn_write_cb)
        , exchanges_(rap_max_exchange_id + 1)
    {
        // assert correctly initialized exchange vector
        assert(exchanges_.size() == rap_max_exchange_id + 1);
        for (rap_exch_id id = 0; id < exchanges_.size(); ++id) {
            exchanges_[id].init(
                static_cast<rap_conn*>(this), id, max_send_window, 0, 0);
            assert(exchanges_[id].id() == id);
            if (conn_exch_init_cb != 0)
                conn_exch_init_cb(this->conn_user_data(), id, &exchanges_[id]);
        }
    }

    rap::exchange* get_exchange(rap_exch_id id)
    {
        if (id < 0 || id >= exchanges_.size())
            return 0;
        return &exchanges_[id];
    }

private:
    std::vector<rap::exchange> exchanges_;

    void process_conn(const rap_frame* f) { assert(!"TODO!"); }

    bool process_frame(rap_exch_id id, const rap_frame* f, int len, rap::error& ec)
    {
        if (id < exchanges_.size()) {
            return exchanges_[id].process_frame(f, len, ec);
        } else {
            ec = rap_err_invalid_exchange_id;
#ifndef NDEBUG
            fprintf(stderr,
                "rap::conn_impl::process_frame(): exchange id %04x out of range\n",
                id);
#endif
            return false;
        }
    }
};

} // namespace rap

#endif // RAP_CONN_IMPL_HPP
