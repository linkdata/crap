#ifndef CRAP_H
#define CRAP_H 1

#include <stdint.h>

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

#ifndef RAP_FRAME_DEFINED
#define RAP_FRAME_DEFINED 1
typedef void rap_frame;
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
} rap_tags;

enum {
  rap_conn_exchange_id = 0x1fff,
  rap_max_exchange_id = rap_conn_exchange_id - 1
};

enum {
  rap_frame_header_size = 4,    /**< Number of octets in a rap frame header. */
  rap_frame_max_size = 0x10000, /**< Maximum frame size on the wire. */
  rap_frame_max_payload_size =
      rap_frame_max_size -
      rap_frame_header_size, /**< Maximum allowed frame payload size. */
};

/*
    Network-side connection-level API

    The application listens for TCP connections on a port (usually 10111)
    and for each one, starts two threads to service it, one thread for
    reading network data and forwarding it to the library using
    rap_conn_recv() and one for writing network data from the library
    using rap_conn_send().

    Once either of those functions return -1, both threads must terminate
    and rap_conn_destroy() must be called to clean up.
*/

rap_conn* rap_conn_create();
void rap_conn_destroy(rap_conn* conn);
int rap_conn_recv(rap_conn* conn, const char* buf, int len);
int rap_conn_send(rap_conn* conn, char* buf, int max_len);
int rap_conn_lock(rap_conn* conn);
int rap_conn_unlock(rap_conn* conn);

/*
    Application-side connection-level API

    Application runs a thread to recieve incoming
    RAP frames for every connection. This may be the
    same thread running rap_conn_recv(), as long as
    it doesn't block during handling of frames.

    Each frame dequeued is handled depending on it's
    tag (record type).
*/

rap_frame* rap_conn_frame_dequeue(rap_conn*);
int rap_conn_frame_enqueue(rap_conn*, rap_frame*);

/*
    Frame API
*/
rap_frame* rap_frame_create(int payload_max_size);
void rap_frame_destroy(rap_frame*);
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
