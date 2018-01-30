/**
 * @brief REST Aggregation Protocol test server
 * @author Johan Lindh <johan@linkdata.se>
 * @note Copyright (c)2015-2017 Johan Lindh
 */

#include <iostream>
#include <sstream>

#include <boost/asio.hpp>

#include "crap.h"

extern "C" int main(int argc, char** argv) {
  auto x = rap_conn_create();
  rap_conn_destroy(x);
  return 0;
}
