#ifndef RAP_EXCHANGE_HPP
#define RAP_EXCHANGE_HPP

#include <cstdint>

#include "rap.hpp"
#include "rap_callbacks.h"
#include "rap_constants.h"
#include "rap_frame.h"

#include "rap_link.hpp"

namespace rap {

class exchange {
public:
    explicit exchange()
        : link_(nullptr)
        , exchange_cb_(nullptr)
        , exchange_cb_param_(nullptr)
        , queue_(nullptr)
        , id_(rap_muxer_exchange_id)
        , send_window_(0)
    {
    }

    int init(rap::link* link, rap_exch_id id, int send_window,
        rap_exchange_cb_t exchange_cb, void* exchange_cb_param)
    {
        if (!link || id > rap_max_exchange_id)
            return -1;
        link_ = link;
        exchange_cb_ = exchange_cb;
        exchange_cb_param_ = exchange_cb_param;
        queue_ = nullptr;
        id_ = id;
        send_window_ = static_cast<int16_t>(send_window);

        ack_[0] = '\0';
        ack_[1] = '\0';
        ack_[2] = static_cast<char>(id_ >> 8);
        ack_[3] = static_cast<char>(id_);

        return 0;
    }

    virtual ~exchange()
    {
        while (queue_)
            framelink::dequeue(&queue_);
        id_ = rap_muxer_exchange_id;
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

    error write_frame(const rap_frame* f)
    {
        if (error e = write_queue())
            return e;
        if (send_window_ < 1) {
#ifndef NDEBUG
            fprintf(stderr, "exchange %04x waiting for ack\n", id_);
#endif
            framelink::enqueue(&queue_, f);
            return rap_err_ok;
        }
        return send_frame(f);
    }

    bool process_frame(const rap_frame* f, int len, error& ec)
    {
        if (!f->header().has_payload()) {
            ++send_window_;
            ec = write_queue();
            return false;
        }
        if (!f->header().is_final()) {
            if ((ec = send_ack()))
                return false;
        }

        bool had_head = f->header().has_head();
        if (exchange_cb_)
            exchange_cb_(exchange_cb_param_, this, f, len);
        return had_head;
    }

    rap_exch_id id() const { return id_; }
    int16_t send_window() const { return send_window_; }
    int write(const char* p, int n) const { return link_->write(p, n); }

private:
    rap::link* link_;
    rap_exchange_cb_t exchange_cb_;
    void* exchange_cb_param_;
    framelink* queue_;
    rap_exch_id id_;
    int16_t send_window_;
    char ack_[4];

    error write_queue()
    {
        while (queue_ != nullptr) {
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
            assert("rap::exchange::send_frame(): muxer_.write() failed" == nullptr);
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
