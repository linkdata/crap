#ifndef RAP_CALLBACKS_H
#define RAP_CALLBACKS_H

#include "rap_frame.h"

#ifndef RAP_EXCHANGE_DEFINED
#define RAP_EXCHANGE_DEFINED 1
typedef void rap_exchange;
#endif

/*
    The write network data callback.
    The buffer contents must be copied or fully sent before the call returns.
    A nonzero return value indicates the network socket has been
    closed and connection should terminate.
*/
typedef int (*rap_conn_write_cb_t)(void *conn_user_data, const char *p, int n);

/*
    One-time initialization of RAP exchanges. Called for an exchange before
    it is allowed to process data. Use it to create instances of your own 
    handler objects, and set the exchange's callback parameters.
*/
typedef void (*rap_conn_exch_init_cb_t)(void *conn_user_data, rap_exch_id id, rap_exchange *exch);

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
typedef int (*rap_exchange_cb_t)(void *exchange_cb_param, rap_exchange *exch,
                                 const rap_frame *f, int n);

#endif /* RAP_CALLBACKS_H */
