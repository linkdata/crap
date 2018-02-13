
#include <cassert>
#include <cstdlib>

#include "rap_conn.hpp"
#include "rap_frame.hpp"

#define RAP_CONN_DEFINED 1
typedef rap::conn rap_conn;

#define RAP_FRAME_DEFINED 1
typedef rap::frame rap_frame;

#include "crap.h"

extern "C" rap_conn* rap_conn_create(void* cb_param, rap_conn_write_cb_t write_cb, rap_conn_frame_cb_t frame_cb) {
  return new rap::conn(cb_param, write_cb, frame_cb);
}

extern "C" void rap_conn_destroy(rap_conn* conn) {
  if (conn) delete conn;
}

extern "C" int rap_conn_recv(rap_conn* conn, const char* buf,
                             int bytes_transferred) {
  if (!conn)
    return -1;
  return conn->read_stream(buf, bytes_transferred);
}

int rap_conn_lock(rap_conn* conn);
int rap_conn_unlock(rap_conn* conn);

rap_frame* rap_frame_create(int payload_max_size);
void rap_frame_destroy(rap_frame* f) {
    if (f)
        delete reinterpret_cast<std::vector<char>*>(f);
}
size_t rap_frame_needed_bytes(const char*);
int rap_frame_payload_max_size(const rap_frame*);
const char* rap_frame_payload_start(const rap_frame*);
char* rap_frame_payload_current(const rap_frame*);
const char* rap_frame_payload_limit(const rap_frame*);
int rap_frame_copy(rap_frame* dst, const rap_frame* src);
int rap_frame_error(const rap_frame*);

