#ifndef RAP_CALLBACKS_H
#define RAP_CALLBACKS_H

#include "rap_frame.h"

#ifndef RAP_EXCHANGE_DEFINED
#define RAP_EXCHANGE_DEFINED 1
typedef void rap_exchange;
#endif

typedef int (*rap_write_cb_t)(void*, const char*, int);
typedef int (*rap_exchange_cb_t)(void*, rap_exchange*, const rap_frame*, int);

#endif /* RAP_CALLBACKS_H */
