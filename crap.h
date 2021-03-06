#ifndef CRAP_H
#define CRAP_H 1

#include <stdint.h>

#include "rap_callbacks.h"
#include "rap_frame.h"
#include "rap_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
    crap.h
    Compact REST Aggregation Protocol library
*/

#ifndef RAP_MUXER_DEFINED
#define RAP_MUXER_DEFINED 1
typedef void rap_muxer;
#endif

#ifndef RAP_CONN_DEFINED
#define RAP_CONN_DEFINED 1
typedef void rap_conn;
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
    rap_tag_hijack = rap_tag('\x07'),
    rap_tag_user_first = rap_tag('\x80'),
    rap_tag_invalid = rap_tag(0)
};

/*
* Muxer API
*
* For each connected RAP peer, create a `rap_muxer`
* with callbacks for writing to the network and to
* initialize connections.
* 
* Then repeatedly call `rap_muxer_recv()` to submit incoming
* network data. Once `rap_muxer_recv()` returns a negative
* value, close the connection and call `rap_muxer_destroy()`
* to free up the RAP connection resources.
* 
* The `muxer_conn_init_cb` callback will be called when a
* connection is set up for the first time. Use `rap_conn_set_callback()`
* to set it up to handle requests and responses. Once initialized,
* a connection object will never change it's ID, and won't be destroyed until
* it's associated connection is destroyed.
*/

rap_muxer* rap_muxer_create(void* muxer_user_data, rap_muxer_write_cb_t muxer_write_cb, rap_muxer_conn_init_cb_t muxer_conn_init_cb);
int rap_muxer_recv(rap_muxer* muxer, const char* buf, int len);
void rap_muxer_destroy(rap_muxer* muxer);

/*
* Connection API
*/

rap_conn_id rap_conn_get_id(const rap_conn* conn);
int rap_conn_set_callback(rap_conn* conn, rap_conn_cb_t conn_cb,
    void* conn_cb_param);
int rap_conn_get_callback(const rap_conn* conn,
    rap_conn_cb_t* p_conn_cb,
    void** p_conn_cb_param);
int rap_conn_write_frame(rap_conn* conn, const rap_frame* f);

/*
* Frame API
*/
rap_frame* rap_frame_create(int payload_max_size);
void rap_frame_destroy(rap_frame*);
rap_conn_id rap_frame_id(const rap_frame*);
size_t rap_frame_needed_bytes(const char*);
int rap_frame_payload_max_size(const rap_frame*);
const char* rap_frame_payload_start(const rap_frame*);
char* rap_frame_payload_current(const rap_frame*);
const char* rap_frame_payload_limit(const rap_frame*);
int rap_frame_copy(rap_frame* dst, const rap_frame* src);
int rap_frame_error(const rap_frame*);

/*
* These will return the number of characters written, or negative on error.
* They will advance the frame's current payload pointer on success.
*/
int rap_frame_write_tag(rap_frame*, rap_tag);
int rap_frame_write_uint64(rap_frame*, uint64_t);
int rap_frame_write_int64(rap_frame*, int64_t);
int rap_frame_write_length(rap_frame*, int);
int rap_frame_write_string(rap_frame*, const char*, int);

/*
* Frame parsing API
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
