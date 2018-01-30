/**
 * @brief REST Aggregation Protocol test server
 * @author Johan Lindh <johan@linkdata.se>
 * @note Copyright (c)2015-2017 Johan Lindh
 */

#include <iostream>
#include <sstream>

#include <boost/asio.hpp>

#include "rap_exchange.hpp"
#include "rap_reader.hpp"
#include "rap_request.hpp"
#include "rap_response.hpp"
#include "rap_writer.hpp"

// #include "rapper_conn.hpp"
#include "rapper_server.hpp"

int main(int, char* []) {
  assert(sizeof(rap::text) == sizeof(const char*) + sizeof(size_t));
  assert(sizeof(rap::header) == 4);

  auto x = new rap::server();

  return 0;
}
