#
# crapper - compact RAP echo server
#
# Requires C++11 and Boost 1.66 or higher.
#
# See the LICENCE file for licensing details.
# Copyright (c)2015-2018 Johan Lindh <johan at linkdata dot se>.
#

cmake_minimum_required(VERSION 3.10)

set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_CONFIGURATION_TYPES Debug Release)
set(CMAKE_CXX_STANDARD 11)

project(crap)
enable_testing()

link_directories(${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601 -D_CRT_SECURE_NO_WARNINGS)
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_ADDITIONAL_VERSIONS 1.68)
find_package(Boost REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})
if(Boost_FOUND)
    add_definitions(-DHAVE_BOOST)
endif()

add_subdirectory(test)

# build the sample echo server 'crapper'
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories($ENV{INCLUDE})
add_executable(crapper
  crapper.cpp
  crap.cpp
  crap.h

  rap_constants.h
  rap_callbacks.h
  rap_header.h
  rap_frame.h

  rap.hpp
  rap_link.hpp
  rap_conn.hpp
  rap_exchange.hpp
  rap_kvv.hpp
  rap_reader.hpp
  rap_record.hpp
  rap_request.hpp
  rap_response.hpp
  rap_stats.hpp
  rap_text.hpp
  rap_textmap.c
  rap_textmap.gperf
  rap_writer.hpp
)
target_link_libraries(crapper ${Boost_LIBRARIES})
