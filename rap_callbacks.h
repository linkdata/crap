#ifndef RAP_CALLBACKS_H
#define RAP_CALLBACKS_H

#include "rap_frame.h"

#ifndef RAP_CONN_DEFINED
#define RAP_CONN_DEFINED 1
typedef void rap_conn;
#endif

/*
    The write network data callback.
    The buffer contents must be copied or fully sent before the call returns.
    A nonzero return value indicates the network socket has been
    closed and connection should terminate.
*/
typedef int (*rap_muxer_write_cb_t)(void* muxer_user_data, const char* p, int n);

/*
    One-time initialization of RAP connections. Called for a connection before
    it is allowed to process data. Use it to create instances of your own 
    handler objects, and set the connections callback parameters.
*/
typedef void (*rap_muxer_conn_init_cb_t)(void* muxer_user_data, rap_conn_id id, rap_conn* conn);

/*
    int rap_conn_cb(
        void* conn_cb_param,
        rap_conn* conn,
        const rap_frame* f,
        int n)

    The frame callback is invoked when a new frame is received for
    a connection.
    A nonzero return value indicates the connection should terminate.
*/
typedef int (*rap_conn_cb_t)(void* conn_cb_param, rap_conn* conn,
    const rap_frame* f, int n);

#endif /* RAP_CALLBACKS_H */
