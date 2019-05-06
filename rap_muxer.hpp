#ifndef RAP_MUXER_HPP
#define RAP_MUXER_HPP

#include <cassert>
#include <cstdint>
#include <vector>

#include "rap.hpp"
#include "rap_callbacks.h"
#include "rap_constants.h"
#include "rap_frame.h"
#include "rap_text.hpp"

#include "rap_conn.hpp"
#include "rap_link.hpp"

namespace rap {

class muxer : public link {
public:
    explicit muxer(void* muxer_user_data,
        rap_muxer_write_cb_t muxer_write_cb,
        rap_muxer_conn_init_cb_t muxer_conn_init_cb)
        : link(muxer_user_data, muxer_write_cb)
        , conns_(rap_max_conn_id + 1)
    {
        // assert correctly initialized conn vector
        assert(conns_.size() == rap_max_conn_id + 1);
        for (rap_conn_id id = 0; id < conns_.size(); ++id) {
            conns_[id].init(
                static_cast<rap_muxer*>(this), id, rap_max_send_window, nullptr, nullptr);
            assert(conns_[id].id() == id);
            if (muxer_conn_init_cb != nullptr)
                muxer_conn_init_cb(this->muxer_user_data(), id, &conns_[id]);
        }
    }

    virtual ~muxer() {}

    /**
     * @brief Get the connection object identified by it's ID
     * 
     * @param id the id number of the connection
     * @return rap::conn* the connetion, or NULL if it was not found
     */
    rap::conn* get_conn(rap_conn_id id)
    {
        if (id >= conns_.size())
            return nullptr;
        return &conns_[id];
    }

private:
    std::vector<rap::conn> conns_;

    void process_muxer(const rap_frame* /*f*/) { assert(false); /* TODO */ }

    bool process_frame(rap_conn_id id, const rap_frame* f, int len, rap::error& ec)
    {
        if (id < conns_.size()) {
            return conns_[id].process_frame(f, len, ec);
        } else {
            ec = rap_err_invalid_conn_id;
#ifndef NDEBUG
            fprintf(stderr,
                "rap::muxer::process_frame(): conn id %04x out of range\n",
                id);
#endif
            return false;
        }
    }
};

} // namespace rap

#endif // RAP_MUXER_HPP
