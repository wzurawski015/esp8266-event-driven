#ifndef EV_RESULT_H
#define EV_RESULT_H

/**
 * @brief Result codes used by the framework core.
 */
typedef enum {
    EV_OK = 0,
    EV_ERR_INVALID_ARG = -1,
    EV_ERR_OUT_OF_RANGE = -2,
    EV_ERR_NOT_FOUND = -3,
    EV_ERR_STATE = -4,
    EV_ERR_CONTRACT = -5
} ev_result_t;

#endif /* EV_RESULT_H */
