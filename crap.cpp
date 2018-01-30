#define RAP_CONN_DEFINED 1
typedef rap::conn rap_conn;

#define RAP_FRAME_DEFINED 1
typedef rap::frame rap_frame;

#include "crap.h"
#include <cassert>
#include <cstdlib>

#include "rap_conn.hpp"
#include "rap_frame.hpp"

extern "C" rap_conn* rap_conn_create() { return new rap::conn(); }

extern "C" void rap_conn_destroy(rap_conn* conn) {
  if (conn) delete conn;
}

extern "C" int rap_conn_recv(rap_conn* conn, const char* buf,
                             int bytes_transferred) {
  if (!conn || !buf || bytes_transferred < 0) return -1;
  return 0;
}

int rap_conn_send(rap_conn* conn, char* buf, int max_len);
int rap_conn_lock(rap_conn* conn);
int rap_conn_unlock(rap_conn* conn);
