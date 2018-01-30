#ifndef RAPPER_HPP
#define RAPPER_HPP

#include "rap.hpp"

#include <boost/shared_ptr.hpp>

namespace rapper {

class server;
class conn;

template <typename exchange_t>
struct config {
  typedef boost::shared_ptr<rapper::server> server_ptr;
  typedef boost::shared_ptr<rapper::conn> conn_ptr;
};

}  // namespace rapper

#endif  // RAPPER_HPP
