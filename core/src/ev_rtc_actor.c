#include "ev/rtc_actor.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ev/dispose.h"
#include "ev/msg.h"
#include "ev/publish.h"

#define EV_RTC_TIME_REG_SECONDS 0x00U
#define EV_RTC_TIME_BYTES 3U
#define EV_RTC_SECONDS_MASK 0x7FU
#define EV_RTC_MINUTES_MASK 0x7FU
#define EV_RTC_HOURS_MASK 0x3FU

static uint8_t ev_rtc_actor_bcd_to_decimal(uint8_t bcd)
{
    return (uint8_t)(((uint8_t)(bcd >> 4U) * 10U) + (bcd & 0x0FU));
}

static ev_result_t ev_rtc_actor_publish_time_update(ev_rtc_actor_ctx_t *ctx, const ev_time_payload_t *payload)
{
    ev_msg_t msg = {0};
    ev_result_t rc;
    ev_result_t dispose_rc;

    if ((ctx == NULL) || (payload == NULL) || (ctx->deliver == NULL) || (ctx->deliver_context == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    rc = ev_msg_init_publish(&msg, EV_TIME_UPDATED, ACT_RTC);
    if (rc == EV_OK) {
        rc = ev_msg_set_inline_payload(&msg, payload, sizeof(*payload));
    }
    if (rc == EV_OK) {
        rc = ev_publish(&msg, ctx->deliver, ctx->deliver_context, NULL);
    }

    dispose_rc = ev_msg_dispose(&msg);
    if ((rc == EV_OK) && (dispose_rc != EV_OK)) {
        rc = dispose_rc;
    }

    return rc;
}

static ev_result_t ev_rtc_actor_handle_tick(ev_rtc_actor_ctx_t *ctx)
{
    uint8_t raw_time[EV_RTC_TIME_BYTES] = {0};
    ev_time_payload_t payload;
    ev_i2c_status_t status;

    if ((ctx == NULL) || (ctx->i2c_port == NULL) || (ctx->i2c_port->read_regs == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    status = ctx->i2c_port->read_regs(ctx->i2c_port->ctx,
                                      ctx->port_num,
                                      ctx->device_address_7bit,
                                      EV_RTC_TIME_REG_SECONDS,
                                      raw_time,
                                      EV_RTC_TIME_BYTES);
    if (status != EV_I2C_OK) {
        return EV_OK;
    }

    payload.seconds = ev_rtc_actor_bcd_to_decimal((uint8_t)(raw_time[0] & EV_RTC_SECONDS_MASK));
    payload.minutes = ev_rtc_actor_bcd_to_decimal((uint8_t)(raw_time[1] & EV_RTC_MINUTES_MASK));
    payload.hours = ev_rtc_actor_bcd_to_decimal((uint8_t)(raw_time[2] & EV_RTC_HOURS_MASK));

    return ev_rtc_actor_publish_time_update(ctx, &payload);
}

ev_result_t ev_rtc_actor_init(ev_rtc_actor_ctx_t *ctx,
                              ev_i2c_port_t *i2c_port,
                              ev_i2c_port_num_t port_num,
                              uint8_t device_address_7bit,
                              ev_delivery_fn_t deliver,
                              void *deliver_context)
{
    if ((ctx == NULL) || (i2c_port == NULL) || (i2c_port->read_regs == NULL) || (deliver == NULL) ||
        (deliver_context == NULL) || (device_address_7bit > 0x7FU)) {
        return EV_ERR_INVALID_ARG;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->i2c_port = i2c_port;
    ctx->port_num = port_num;
    ctx->device_address_7bit = device_address_7bit;
    ctx->deliver = deliver;
    ctx->deliver_context = deliver_context;
    return EV_OK;
}

ev_result_t ev_rtc_actor_handle(void *actor_context, const ev_msg_t *msg)
{
    ev_rtc_actor_ctx_t *ctx = (ev_rtc_actor_ctx_t *)actor_context;

    if ((ctx == NULL) || (msg == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    switch (msg->event_id) {
    case EV_TICK_1S:
        return ev_rtc_actor_handle_tick(ctx);

    default:
        return EV_ERR_CONTRACT;
    }
}
