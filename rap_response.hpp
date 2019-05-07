#ifndef RAP_RESPONSE_HPP
#define RAP_RESPONSE_HPP

#include <ostream>

#include "rap.hpp"
#include "rap_kvv.hpp"
#include "rap_reader.hpp"
#include "rap_record.hpp"
#include "rap_text.hpp"
#include "rap_writer.hpp"

namespace rap {

class response : public record {
public:
    response(reader& r)
        : record(r.frame())
        , code_(static_cast<uint16_t>(r.read_length()))
        , headers_(r)
        , content_length_(-1)
    {
    }

    response(uint16_t code = 200, int64_t content_length = -1)
        : record(nullptr)
        , code_(code)
        , content_length_(content_length)
    {
    }

    void render(string_t& out) const
    {
        char buf[64];
        int n = sprintf(buf, "%03d", code());
        if (n > 0)
            out.append(buf, static_cast<size_t>(n));
        out += ' ';
        status().render(out);
        out += '\n';
        headers().render(out);
        if (content_length() >= 0) {
            n = sprintf(buf, "%lld", static_cast<long long>(content_length()));
            if (n > 0) {
                out += "Content-Length: ";
                out.append(buf, static_cast<size_t>(n));
                out += '\n';
            }
        }
        return;
    }

    uint16_t code() const { return code_; }
    void set_code(uint16_t code) { code_ = code; }
    const rap::headers& headers() const { return headers_; }
    text status() const
    {
        size_t i = headers_.find("Status");
        if (i == 0)
            return text();
        return headers_.at(i);
    }
    void set_status(text txt)
    {
        size_t i = headers_.find("Status");
        if (i != 0)
            headers_.at(i) = txt;
    }
    int64_t content_length() const { return content_length_; }
    void set_content_length(int64_t n) { content_length_ = n; }

    const rap::writer& operator>>(const rap::writer& w) const
    {
        w << static_cast<char>(rap::record::tag_http_response) << code()
          << headers() << content_length();
        return w;
    }

private:
    uint16_t code_;
    rap::headers headers_;
    int64_t content_length_;
};

inline const rap::writer& operator<<(const rap::writer& w,
    const rap::response& res)
{
    return res >> w;
}

} // namespace rap

#endif // RAP_RESPONSE_HPP
