#ifndef RAP_CONSTANTS_H
#define RAP_CONSTANTS_H

#include "rap_types.h"

enum {
    rap_muxer_conn_id = rap_conn_id(0x1fff), /**< ID used in frames for a muxer connection. */
    rap_max_conn_id = rap_muxer_conn_id - 1, /**< The highest allowed connection ID. */
    rap_frame_header_size = 4, /**< Number of octets in a rap frame header. */
    rap_max_send_window = 8 /**< maximum send window size */
};

#endif /* RAP_CONSTANTS_H */
