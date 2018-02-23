#ifndef RAP_CALLBACKS_H
#define RAP_CALLBACKS_H

#include "rap_frame.h"

#ifndef RAP_EXCHANGE_DEFINED
#define RAP_EXCHANGE_DEFINED 1
typedef void rap_exchange;
#endif

/*
    The write network data callback.
    Always called with complete frames in the buffer.
    The buffer contents must be copied or sent before the call returns.
    A nonzero return value indicates the network socket has been
    closed and connection should terminate.
*/
typedef int (*rap_write_cb_t)(void* write_cb_param, const char* p, int n);

/*
    int rap_exchange_cb(
        void* exchange_cb_param,
        rap_exchange* exch,
        const rap_frame* f,
        int n)

    The frame callback is invoked when a new frame is received for
    an exchange.
    A nonzero return value indicates the connection should terminate.
*/
typedef int (*rap_exchange_cb_t)(void* exchange_cb_param, rap_exchange* exch,
                                 const rap_frame* f, int n);

#endif /* RAP_CALLBACKS_H */
