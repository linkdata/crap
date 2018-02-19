#ifndef CRAP_H
#define CRAP_H 1

#include <stdint.h>

#include "rap_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
    crap.h
    Compact REST Aggregation Protocol library
*/

#ifndef RAP_CONN_DEFINED
#define RAP_CONN_DEFINED 1
typedef void rap_conn;
#endif

#ifndef RAP_EXCHANGE_DEFINED
#define RAP_EXCHANGE_DEFINED 1
typedef void rap_exchange;
#endif

#ifndef RAP_PARSER_DEFINED
#define RAP_PARSER_DEFINED 1
typedef void rap_parser;
#endif

typedef char rap_tag;
enum {
  rap_tag_set_string = rap_tag('\x01'),
  rap_tag_set_route = rap_tag('\x02'),
  rap_tag_http_request = rap_tag('\x03'),
  rap_tag_http_response = rap_tag('\x04'),
  rap_tag_service_pause = rap_tag('\x05'),
  rap_tag_service_resume = rap_tag('\x06'),
  rap_tag_user_first = rap_tag('\x80'),
  rap_tag_invalid = rap_tag(0)
};

/*
    The write network data callback.
    Always called with complete frames in the buffer.
    The buffer contents must be copied or sent before the call returns.
    A nonzero return value indicates the network socket has been
    closed and connection should terminate.
*/
typedef int (*rap_conn_write_cb_t)(void* write_cb_param, const char* p, int n);

/*
    rap_conn_frame_cb(void* frame_cb_param, rap_exchange* exch, const rap_frame*
   f, int n)

    The frame callback is invoked when a new frame is received for
    an exchange.
    A nonzero return value indicates the connection should terminate.
*/
typedef int (*rap_conn_frame_cb_t)(void* frame_cb_param, rap_exchange* exch,
                                   const rap_frame* f, int n);

/*
 * Network-side connection-level API
 *
 * For each listening port (usually on 10111), create a RAP
 * connection with callbacks for writing to the network and
 * handle incoming RAP frames.
 * Then repeatedly call rap_conn_recv() to submit incoming
 * network data. Once rap_conn_recv() returns a negative
 * value, close the connection and call rap_conn_destroy()
 * to free up the RAP connection resources.
 */

rap_conn* rap_conn_create(rap_conn_write_cb_t write_cb, void* write_cb_param,
                          rap_conn_frame_cb_t frame_cb, void* frame_cb_param);
int rap_conn_recv(rap_conn* conn, const char* buf, int len);
void rap_conn_destroy(rap_conn* conn);

/*
 * Exchange level API
 *
 * When the application gets a `frame_cb` callback, one of the
 * parameters is a `rap_exchange*`. That parameter may be used
 * with these APIs.
 */

int rap_exch_get_id(const rap_exchange* exch);
int rap_exch_write_frame(rap_exchange* exch, const rap_frame* f);

/*
    Application-side connection-level API

    Application runs a thread to recieve incoming
    RAP frames for every connection. This may be the
    same thread running rap_conn_recv(), as long as
    it doesn't block during handling of frames.

    Each frame dequeued is handled depending on it's
    tag (record type).
*/

rap_exchange* rap_conn_accept(rap_conn* conn);
rap_frame* rap_conn_frame_dequeue(rap_conn*);
int rap_conn_frame_enqueue(rap_conn*, rap_frame*);

/*
    Frame API
*/
rap_frame* rap_frame_create(int payload_max_size);
void rap_frame_destroy(rap_frame*);
int rap_frame_id(const rap_frame*);
size_t rap_frame_needed_bytes(const char*);
int rap_frame_payload_max_size(const rap_frame*);
const char* rap_frame_payload_start(const rap_frame*);
char* rap_frame_payload_current(const rap_frame*);
const char* rap_frame_payload_limit(const rap_frame*);
int rap_frame_copy(rap_frame* dst, const rap_frame* src);
int rap_frame_error(const rap_frame*);

/*
    These will return the number of characters written, or negative on error.
    They will advance the frame's current payload pointer on success.
*/
int rap_frame_write_tag(rap_frame*, rap_tag);
int rap_frame_write_uint64(rap_frame*, uint64_t);
int rap_frame_write_int64(rap_frame*, int64_t);
int rap_frame_write_length(rap_frame*, int);
int rap_frame_write_string(rap_frame*, const char*, int);

/*
    Frame parsing API
*/
int rap_parse_tag(const char**, rap_tag* result);
int rap_parse_uint64(const char**, uint64_t* result);
int rap_parse_int64(const char**, int64_t* result);
int rap_parse_length(const char**, int* result);
int rap_parse_string(const char**, const char** result, int* result_len);

#ifdef __cplusplus
}
#endif

#endif /* CRAP_H */
