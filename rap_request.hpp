#ifndef RAP_REQUEST_HPP
#define RAP_REQUEST_HPP

#include "rap.hpp"
#include "rap_kvv.hpp"
#include "rap_reader.hpp"
#include "rap_record.hpp"
#include "rap_text.hpp"

#include <cassert>
#include <cstring>

namespace rap {

class request : public record {
public:
    request(reader& r)
        : record(r.frame())
        , method_(r.read_text())
        , scheme_(r.read_text())
        , route_(r.read_route())
        , query_(r)
        , headers_(r)
        , host_(r.read_text())
        , content_length_(r.read_int64())
    {
        assert(!method_.empty());
        assert(!route_.empty());
        assert(content_length_ >= -1);
        assert(content_length_ < (int64_t(1) << 32));
    }

    text method() const { return method_; }
    text scheme() const { return scheme_; }
    rap::route route() const { return route_; }
    const rap::query& query() const { return query_; }
    const rap::headers& headers() const { return headers_; }
    text host() const { return host_; }
    int64_t content_length() const { return content_length_; }

    void render(string_t& out) const
    {
        if (method().is_null())
            return;
        assert(!route().is_null());
        assert(content_length() >= -1);
        method().render(out);
        out += ' ';
        route().render(out);
        query().render(out);
        out += '\n';
        headers().render(out);
        if (!host().empty()) {
            out += "Host: ";
            host().render(out);
            out += '\n';
        }
        if (content_length() >= 0) {
            char buf[64];
            int n = sprintf(buf, "%lld", static_cast<long long>(content_length()));
            if (n > 0) {
                out += "Content-Length: ";
                out.append(buf, static_cast<size_t>(n));
                out += '\n';
            }
        }
    }

private:
    text method_;
    text scheme_;
    rap::route route_;
    rap::query query_;
    rap::headers headers_;
    text host_;
    int64_t content_length_;
};

} // namespace rap

#endif // RAP_REQUEST_HPP
