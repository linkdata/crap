#ifndef RAP_HEAD_HPP
#define RAP_HEAD_HPP

#include "rap.hpp"
#include "rap_frame.h"
#include "rap_text.hpp"


#include <cassert>
#include <streambuf>

namespace rap {

class record {
public:
    typedef char tag;

    enum {
        tag_set_string = tag('\x01'),
        tag_set_route = tag('\x02'),
        tag_http_request = tag('\x03'),
        tag_http_response = tag('\x04'),
        tag_service_pause = tag('\x05'),
        tag_service_resume = tag('\x06'),
        tag_user_first = tag('\x80'),
        tag_invalid = tag(0)
    } tags;

    static void write(std::streambuf& sb, char ch) { sb.sputc(ch); }

    static void write(std::streambuf& sb, uint16_t n)
    {
        sb.sputc(static_cast<char>(n >> 8));
        sb.sputc(static_cast<char>(n));
    }

    explicit record(const rap_frame* f)
        : frame_(f)
    {
    }

protected:
    const rap_frame* frame_;
};

} // namespace rap

#endif // RAP_HEAD_HPP
