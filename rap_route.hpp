#ifndef RAP_ROUTE_HPP
#define RAP_ROUTE_HPP

#include "rap.hpp"
#include "rap_text.hpp"

namespace rap {

class route {
public:
    route()
        : index_(0)
    {
    }

    route(const route& other)
        : text_(other.text_)
        , index_(other.index_)
    {
    }

    explicit route(const text& txt)
        : text_(txt)
        , index_(0)
    {
    }

    explicit route(uint16_t map_index)
        : index_(map_index)
    {
    }

    route& operator=(const route& other)
    {
        index_ = other.index_;
        text_ = other.text_;
        return *this;
    }

    void render(string_t& out) const
    {
        if (index_ == 0)
            text_.render(out);
        else
            ; // TODO
    }

    string_t str() const
    {
        string_t retv;
        render(retv);
        return retv;
    }

    bool is_null() const { return index_ == 0 && text_.is_null(); }
    bool empty() const { return index_ == 0 && text_.empty(); }

private:
    text text_;
    uint16_t index_;
};

} // namespace rap

#endif // RAP_ROUTE_HPP
