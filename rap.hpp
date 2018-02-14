#ifndef RAP_HPP
#define RAP_HPP

#include <cstdint>
#include <string>

#include "rap_header.h"

namespace rap {

typedef enum {
  rap_err_ok = 0,
  rap_err_null_string = 1,
  rap_err_invalid_parameter = 2,
  rap_err_payload_too_big = 3,
  rap_err_unknown_frame_type = 4,
  rap_err_output_buffer_too_small = 5,
  rap_err_incomplete_length = 6,
  rap_err_incomplete_string = 7,
  rap_err_string_index_unknown = 9,
  rap_err_incomplete_lookup = 10,
  rap_err_string_too_long = 11,
  rap_err_invalid_exchange_id = 12,
  rap_err_incomplete_number = 13,
  rap_err_incomplete_body = 14
} error;

typedef enum {
  rap_frame_type_setup = 0x01,
  rap_frame_type_request = 0x02,
  rap_frame_type_response = 0x03,
  rap_frame_type_close = 0x04,
  rap_frame_type_body = 0xff,
} rap_frame_type;

typedef enum {
  rap_frame_flag_final = 0x80,
  rap_frame_flag_head = 0x40,
  rap_frame_flag_body = 0x20,
} rap_frame_flag;

typedef char record_type;
enum {
  RecordTypeHTTPRequest = record_type(0x01),
  RecordTypeHTTPResponse = record_type(0x02),
};

typedef std::string string_t;

class text;
class kvv;
class exchange;
class conn;

}  // namespace rap

#endif  // RAP_HPP
