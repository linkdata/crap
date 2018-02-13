
#include <cassert>
#include <cstdlib>

#include "rap_conn.hpp"
#include "rap_frame.hpp"

#define RAP_CONN_DEFINED 1
typedef rap::conn rap_conn;

#define RAP_FRAME_DEFINED 1
typedef rap::frame rap_frame;

#include "crap.h"

extern "C" rap_conn* rap_conn_create(rap_conn_write_cb_t cb, void* cb_param) {
  return new rap::conn(cb, cb_param);
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
