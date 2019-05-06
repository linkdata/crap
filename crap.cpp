
#include <cassert>
#include <cstdlib>

#include "rap.hpp"
#include "rap_exchange.hpp"
#include "rap_frame.h"
#include "rap_muxer.hpp"

/* crap.h must be included after rap.hpp */
#include "crap.h"

extern "C" rap_muxer* rap_muxer_create(void* muxer_user_data,
    rap_muxer_write_cb_t muxer_write_cb,
    rap_muxer_exch_init_cb_t muxer_exch_init_cb)
{
    return new rap::muxer(muxer_user_data, muxer_write_cb, muxer_exch_init_cb);
}

extern "C" int rap_muxer_recv(rap_muxer* muxer, const char* buf, int bytes_transferred)
{
    return muxer->recv(buf, bytes_transferred);
}

extern "C" rap_exchange* rap_muxer_get_exchange(rap_muxer* muxer, int id)
{
    return muxer->get_exchange(id);
}

extern "C" void rap_muxer_destroy(rap_muxer* muxer)
{
    if (muxer)
        delete muxer;
}

/*
 * Exchange level API
 */

extern "C" rap_exch_id rap_exch_get_id(const rap_exchange* exch)
{
    return exch->id();
}

extern "C" int rap_exch_set_callback(rap_exchange* exch,
    rap_exchange_cb_t exchange_cb,
    void* exchange_cb_param)
{
    return exch->set_callback(exchange_cb, exchange_cb_param);
}

extern "C" int rap_exch_get_callback(const rap_exchange* exch,
    rap_exchange_cb_t* p_exchange_cb,
    void** p_exchange_cb_param)
{
    return exch->get_callback(p_exchange_cb, p_exchange_cb_param);
}

int rap_exch_write_frame(rap_exchange* exch, const rap_frame* f)
{
    return exch->write_frame(f);
}

rap_frame* rap_frame_create(int payload_max_size);
void rap_frame_destroy(rap_frame* f)
{
    if (f)
        delete reinterpret_cast<std::vector<char>*>(f);
}
rap_exch_id rap_frame_id(const rap_frame* f)
{
    return f->header().id();
}
size_t rap_frame_needed_bytes(const char*);
int rap_frame_payload_max_size(const rap_frame*);
const char* rap_frame_payload_start(const rap_frame*);
char* rap_frame_payload_current(const rap_frame*);
const char* rap_frame_payload_limit(const rap_frame*);
int rap_frame_copy(rap_frame* dst, const rap_frame* src);
int rap_frame_error(const rap_frame*);
