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

#include "rap_exchange.hpp"
#include "rap_link.hpp"

namespace rap {

class muxer : public link {
public:
    explicit muxer(void* muxer_user_data,
        rap_muxer_write_cb_t muxer_write_cb,
        rap_muxer_exch_init_cb_t muxer_exch_init_cb)
        : link(muxer_user_data, muxer_write_cb)
        , exchanges_(rap_max_exchange_id + 1)
    {
        // assert correctly initialized exchange vector
        assert(exchanges_.size() == rap_max_exchange_id + 1);
        for (rap_exch_id id = 0; id < exchanges_.size(); ++id) {
            exchanges_[id].init(
                static_cast<rap_muxer*>(this), id, rap_max_send_window, 0, 0);
            assert(exchanges_[id].id() == id);
            if (muxer_exch_init_cb != 0)
                muxer_exch_init_cb(this->muxer_user_data(), id, &exchanges_[id]);
        }
    }

    virtual ~muxer() {}

    /**
     * @brief Get the exchange object identified by it's ID
     * 
     * @param id the id number of the exchange
     * @return rap::exchange* the exchange, or NULL if it was not found
     */
    rap::exchange* get_exchange(rap_exch_id id)
    {
        if (id < 0 || id >= exchanges_.size())
            return 0;
        return &exchanges_[id];
    }

private:
    std::vector<rap::exchange> exchanges_;

    void process_muxer(const rap_frame* f) { assert(!"TODO!"); }

    bool process_frame(rap_exch_id id, const rap_frame* f, int len, rap::error& ec)
    {
        if (id < exchanges_.size()) {
            return exchanges_[id].process_frame(f, len, ec);
        } else {
            ec = rap_err_invalid_exchange_id;
#ifndef NDEBUG
            fprintf(stderr,
                "rap::muxer::process_frame(): exchange id %04x out of range\n",
                id);
#endif
            return false;
        }
    }
};

} // namespace rap

#endif // RAP_MUXER_HPP
