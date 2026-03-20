#ifndef EV_RTC_ACTOR_H
#define EV_RTC_ACTOR_H

#include <stdint.h>

#include "ev/compiler.h"
#include "ev/delivery.h"
#include "ev/msg.h"
#include "ev/port_i2c.h"
#include "ev/result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EV_RTC_DEFAULT_ADDR_7BIT 0x68U

/**
 * @brief Inline payload published when the hardware clock has been sampled.
 */
typedef struct {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
} ev_time_payload_t;

EV_STATIC_ASSERT(sizeof(ev_time_payload_t) <= EV_MSG_INLINE_CAPACITY,
                 "RTC time payload must fit into one inline event payload");

/**
 * @brief Actor-local RTC state and injected dependencies.
 */
typedef struct {
    ev_i2c_port_t *i2c_port;
    ev_i2c_port_num_t port_num;
    uint8_t device_address_7bit;
    ev_delivery_fn_t deliver;
    void *deliver_context;
} ev_rtc_actor_ctx_t;

/**
 * @brief Initialize one RTC actor context.
 *
 * The actor performs no hardware I/O during initialization. It stores the
 * injected I2C contract and the delivery contract used later to publish
 * EV_TIME_UPDATED whenever EV_TICK_1S is observed.
 *
 * @param ctx Context to initialize.
 * @param i2c_port Injected platform I2C contract.
 * @param port_num Logical I2C controller number.
 * @param device_address_7bit Target 7-bit RTC I2C address.
 * @param deliver Delivery callback used by ev_publish().
 * @param deliver_context Caller-owned context bound to @p deliver.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_rtc_actor_init(ev_rtc_actor_ctx_t *ctx,
                              ev_i2c_port_t *i2c_port,
                              ev_i2c_port_num_t port_num,
                              uint8_t device_address_7bit,
                              ev_delivery_fn_t deliver,
                              void *deliver_context);

/**
 * @brief Default actor handler for one RTC runtime instance.
 *
 * Supported events:
 * - EV_TICK_1S
 *
 * @param actor_context Pointer to ev_rtc_actor_ctx_t.
 * @param msg Runtime envelope delivered to the actor.
 * @return EV_OK on success or an error code when the message contract is invalid.
 */
ev_result_t ev_rtc_actor_handle(void *actor_context, const ev_msg_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* EV_RTC_ACTOR_H */
