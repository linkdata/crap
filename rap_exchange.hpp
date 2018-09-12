#ifndef RAP_EXCHANGE_HPP
#define RAP_EXCHANGE_HPP

#include <cstdint>

#include "rap.hpp"
#include "rap_callbacks.h"
#include "rap_constants.h"
#include "rap_frame.h"


class rap::conn;

// #include "rap_conn.hpp"

namespace rap {

class exchange {
public:
    explicit exchange()
        : conn_(0)
        , exchange_cb_(0)
        , exchange_cb_param_(0)
        , queue_(NULL)
        , id_(rap_conn_exchange_id)
        , send_window_(0)
    {
    }

    int init(rap_conn* conn, rap_exch_id id, int send_window, rap_exchange_cb_t exchange_cb, void* exchange_cb_param);

    virtual ~exchange()
    {
        while (queue_)
            framelink::dequeue(&queue_);
        id_ = rap_conn_exchange_id;
    }

    int set_callback(rap_exchange_cb_t exchange_cb, void* exchange_cb_param)
    {
        exchange_cb_ = exchange_cb;
        exchange_cb_param_ = exchange_cb_param;
        return 0;
    }

    int get_callback(rap_exchange_cb_t* p_exchange_cb,
        void** p_exchange_cb_param) const
    {
        *p_exchange_cb = exchange_cb_;
        *p_exchange_cb_param = exchange_cb_param_;
        return 0;
    }

    error write_frame(const rap_frame* f);

    rap_exch_id id() const { return id_; }
    int16_t send_window() const { return send_window_; }
    int write(const char* p, int n) const;
    bool process_frame(const rap_frame* f, int len, error& ec);

private:
    rap_conn* conn_;
    rap_exchange_cb_t exchange_cb_;
    void* exchange_cb_param_;
    framelink* queue_;
    rap_exch_id id_;
    int16_t send_window_;
    char ack_[4];

    error write_queue()
    {
        while (queue_ != NULL) {
            rap_frame* f = reinterpret_cast<rap_frame*>(queue_ + 1);
            if (!f->header().is_final() && send_window_ < 1)
                return rap_err_ok;
            if (error e = send_frame(f))
                return e;
            framelink::dequeue(&queue_);
        }
        return rap_err_ok;
    }

    error send_frame(const rap_frame* f)
    {
        if (write(f->data(), static_cast<int>(f->size()))) {
            assert(!"rap::exchange::send_frame(): conn_.write() failed");
            return rap_err_output_buffer_too_small;
        }
        if (!f->header().is_final())
            --send_window_;
        return rap_err_ok;
    }

    error send_ack()
    {
        return write(ack_, sizeof(ack_)) ? rap_err_output_buffer_too_small
                                         : rap_err_ok;
    }
};

} // namespace rap

#endif // RAP_EXCHANGE_HPP
