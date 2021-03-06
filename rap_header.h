#ifndef RAP_HEADER_H
#define RAP_HEADER_H

#include <assert.h>
#include <stdint.h>

#include "rap_constants.h"

struct rap_header {
    // these masks apply to buf_[2]
    static const unsigned char mask_head = 0x20;
    static const unsigned char mask_body = 0x40;
    static const unsigned char mask_flow = 0x80;
    static const unsigned char mask_all = (mask_head | mask_body | mask_flow);
    static const unsigned char mask_id = (rap_muxer_conn_id >> 8);

    rap_header()
    {
        buf_[0] = 0;
        buf_[1] = 0;
        buf_[2] = 0;
        buf_[3] = 0;
    }

    explicit rap_header(rap_conn_id id)
    {
        buf_[0] = 0;
        buf_[1] = 0;
        buf_[2] = static_cast<unsigned char>((id >> 8) & mask_id);
        buf_[3] = static_cast<unsigned char>(id);
    }

    const char* data() const { return reinterpret_cast<const char*>(this); }
    char* data() { return reinterpret_cast<char*>(this); }
    size_t size() const { return rap_frame_header_size + payload_size(); }

    bool has_flow() const { return buf_[2] & mask_flow; }
    bool has_body() const { return buf_[2] & mask_body; }
    bool has_head() const { return buf_[2] & mask_head; }
    bool has_body_or_head() const { return buf_[2] & (mask_head|mask_body); }

    bool has_payload() const { return !has_flow() && has_body_or_head(); }
    size_t payload_size() const { return has_payload() ? size_value() : 0; }

    size_t size_value() const
    {
        return static_cast<size_t>(buf_[0]) << 8 | buf_[1];
    }
    void set_size_value(size_t n)
    {
        buf_[0] = static_cast<unsigned char>(n >> 8);
        buf_[1] = static_cast<unsigned char>(n);
    }

    uint16_t id() const
    {
        return static_cast<uint16_t>((buf_[2] & mask_id) << 8 | buf_[3]);
    }
    void set_id(uint16_t id)
    {
        buf_[2] = static_cast<unsigned char>(id >> 8) & mask_id;
        buf_[3] = static_cast<unsigned char>(id);
    }

    bool is_flow() const { return (buf_[2] & mask_flow) == mask_flow; }
    bool is_ack() const { return (buf_[2] & mask_all) == mask_flow; }
    bool is_conn_control() const { return buf_[2] == mask_id && buf_[3] == 0xff; }
    bool is_final() const { return (buf_[2] & (mask_flow|mask_body)) == (mask_flow|mask_body); }
    void set_final() { buf_[2] |= (mask_flow|mask_body); }
    void clr_final() { buf_[2] &= ~(mask_flow|mask_body); }
    void set_head() { buf_[2] |= mask_head; }
    void set_body() { buf_[2] |= mask_body; }

    unsigned char buf_[rap_frame_header_size];
};

#endif // RAP_HEADER_H
