#include "rap_exchange.hpp"
#include "rap_conn.hpp"

namespace rap
{

int exchange::init(rap_conn *conn, rap_exch_id id, int send_window,
                   rap_exchange_cb_t exchange_cb, void *exchange_cb_param)
{
  if (!conn || id > rap_max_exchange_id)
    return -1;
  conn_ = conn;
  exchange_cb_ = exchange_cb;
  exchange_cb_param_ = exchange_cb_param;
  queue_ = NULL;
  id_ = id;
  send_window_ = send_window;

  ack_[0] = '\0';
  ack_[1] = '\0';
  ack_[2] = static_cast<char>(id_ >> 8);
  ack_[3] = static_cast<char>(id_);

  return 0;
}

int exchange::write(const char *p, int n) const { return conn_->write(p, n); }

error exchange::write_frame(const rap_frame *f)
{
  if (error e = write_queue())
    return e;
  if (send_window_ < 1)
  {
#ifndef NDEBUG
    fprintf(stderr, "exchange %04x waiting for ack\n", id_);
#endif
    framelink::enqueue(&queue_, f);
    return rap_err_ok;
  }
  return send_frame(f);
}

bool exchange::process_frame(const rap_frame *f, int len, error &ec)
{
  if (!f->header().has_payload())
  {
    ++send_window_;
    ec = write_queue();
    return false;
  }
  if (!f->header().is_final())
  {
    if ((ec = send_ack()))
      return false;
  }

  bool had_head = f->header().has_head();
  if (exchange_cb_)
    exchange_cb_(exchange_cb_param_, this, f, len);
  return had_head;
}

} // namespace rap
