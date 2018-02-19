
#include <cassert>
#include <cstdlib>

#include "rap_frame.h"
#include "rap_conn.hpp"

#define RAP_CONN_DEFINED 1
typedef rap::conn rap_conn;

#define RAP_EXCHANGE_DEFINED 1
typedef rap::exchange rap_exchange;

#include "crap.h"

extern "C" rap_conn* rap_conn_create(rap_conn_write_cb_t write_cb,
                                     void* write_cb_param,
                                     rap_conn_frame_cb_t frame_cb,
                                     void* frame_cb_param) {
    return new rap::conn(write_cb, write_cb_param, (rap::connbase::frame_cb_t)frame_cb, frame_cb_param);
}

extern "C" int rap_conn_recv(rap_conn* conn, const char* buf,
                             int bytes_transferred) {
    return conn->read_stream(buf, bytes_transferred);
}

extern "C" void rap_conn_destroy(rap_conn* conn) {
    if (conn) delete conn;
}

/*
 * Exchange level API
 */

extern "C" int rap_exch_get_id(const rap_exchange* exch){
    return exch->id();
}

extern "C" void* rap_exch_get_userdata(const rap_exchange* exch){
    return NULL;
}

extern "C" void rap_exch_set_userdata(rap_exchange* exch, void* userdata){
    return;
}

int rap_exch_write_frame(rap_exchange* exch, const rap_frame* f) {
    return exch->write_frame(f);
}

rap_frame* rap_frame_create(int payload_max_size);
void rap_frame_destroy(rap_frame* f) {
  if (f) delete reinterpret_cast<std::vector<char>*>(f);
}
int rap_frame_id(const rap_frame* f) {
  return f->header().id();
}
size_t rap_frame_needed_bytes(const char*);
int rap_frame_payload_max_size(const rap_frame*);
const char* rap_frame_payload_start(const rap_frame*);
char* rap_frame_payload_current(const rap_frame*);
const char* rap_frame_payload_limit(const rap_frame*);
int rap_frame_copy(rap_frame* dst, const rap_frame* src);
int rap_frame_error(const rap_frame*);
