
#include <cassert>
#include <cstdlib>

#include "rap.hpp"
#include "rap_conn.hpp"
#include "rap_exchange.hpp"
#include "rap_frame.h"

/* crap.h must be included after rap.hpp */
#include "crap.h"

extern "C" rap_conn* rap_conn_create(rap_write_cb_t write_cb,
                                     void* write_cb_param) {
  return new rap::conn(write_cb, write_cb_param);
}

extern "C" int rap_conn_recv(rap_conn* conn, const char* buf,
                             int bytes_transferred) {
  return conn->read_stream(buf, bytes_transferred);
}

extern "C" rap_exchange* rap_conn_get_exchange(rap_conn* conn, int id) {
  return conn->get_exchange(id);
}

extern "C" void rap_conn_destroy(rap_conn* conn) {
  if (conn) delete conn;
}

/*
 * Exchange level API
 */

extern "C" int rap_exch_get_id(const rap_exchange* exch) { return exch->id(); }

extern "C" int rap_exch_set_callback(rap_exchange* exch,
                                     rap_exchange_cb_t exchange_cb,
                                     void* exchange_cb_param) {
  return exch->set_callback(exchange_cb, exchange_cb_param);
}

extern "C" int rap_exch_get_callback(const rap_exchange* exch,
                                     rap_exchange_cb_t* p_exchange_cb,
                                     void** p_exchange_cb_param) {
  return exch->get_callback(p_exchange_cb, p_exchange_cb_param);
}

int rap_exch_write_frame(rap_exchange* exch, const rap_frame* f) {
  return exch->write_frame(f);
}

rap_frame* rap_frame_create(int payload_max_size);
void rap_frame_destroy(rap_frame* f) {
  if (f) delete reinterpret_cast<std::vector<char>*>(f);
}
int rap_frame_id(const rap_frame* f) { return f->header().id(); }
size_t rap_frame_needed_bytes(const char*);
int rap_frame_payload_max_size(const rap_frame*);
const char* rap_frame_payload_start(const rap_frame*);
char* rap_frame_payload_current(const rap_frame*);
const char* rap_frame_payload_limit(const rap_frame*);
int rap_frame_copy(rap_frame* dst, const rap_frame* src);
int rap_frame_error(const rap_frame*);
