#ifndef EV_PORT_LOG_H
#define EV_PORT_LOG_H

#include <stddef.h>

#include "ev/result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ev_log_level {
    EV_LOG_DEBUG = 0,
    EV_LOG_INFO = 1,
    EV_LOG_WARN = 2,
    EV_LOG_ERROR = 3,
    EV_LOG_FATAL = 4
} ev_log_level_t;

typedef struct ev_log_port {
    void *ctx;
    ev_result_t (*write)(void *ctx,
                         ev_log_level_t level,
                         const char *tag,
                         const char *message,
                         size_t message_len);
    ev_result_t (*flush)(void *ctx);
} ev_log_port_t;

#ifdef __cplusplus
}
#endif

#endif /* EV_PORT_LOG_H */
