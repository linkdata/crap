/**
 * @brief REST Aggregation Protocol test server
 * @author Johan Lindh <johan@linkdata.se>
 * @note Copyright (c)2015-2017 Johan Lindh
 */

#include "rap_exchange.hpp"
#include "rap_reader.hpp"
#include "rap_request.hpp"
#include "rap_response.hpp"
#include "rap_writer.hpp"

// #include "rapper_conn.hpp"
#include "rapper_server.hpp"

#include <boost/asio.hpp>

#include <iostream>
#include <sstream>

int main(int, char *[]) {
  assert(sizeof(rap::text) == sizeof(const char *) + sizeof(size_t));
  assert(sizeof(rap::header) == 4);
  try {
    boost::asio::io_service io_service;
    rap_server::ptr s(new rap_server(io_service, 10111));
    s->start();
    io_service.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
