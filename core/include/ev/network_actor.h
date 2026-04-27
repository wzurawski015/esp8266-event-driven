#ifndef EV_NETWORK_ACTOR_H
#define EV_NETWORK_ACTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "ev/compiler.h"
#include "ev/msg.h"
#include "ev/port_net.h"
#include "ev/result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ev_network_state {
    EV_NETWORK_STATE_DISCONNECTED = 0,
    EV_NETWORK_STATE_WIFI_UP = 1,
    EV_NETWORK_STATE_MQTT_CONNECTED = 2,
    EV_NETWORK_STATE_DEGRADED = 3
} ev_network_state_t;

typedef struct ev_network_actor_stats {
    uint32_t wifi_up_events;
    uint32_t wifi_down_events;
    uint32_t mqtt_up_events;
    uint32_t mqtt_down_events;
    uint32_t mqtt_rx_events;
    uint32_t mqtt_rx_ignored_foundation;
    uint32_t tx_commands_seen;
    uint32_t tx_attempts;
    uint32_t tx_ok;
    uint32_t tx_failed;
    uint32_t tx_rejected_not_connected;
    uint32_t bad_payloads;
    uint32_t bad_events;
} ev_network_actor_stats_t;

typedef struct ev_network_actor_ctx {
    ev_net_port_t *net_port;
    ev_network_state_t state;
    ev_network_actor_stats_t stats;
} ev_network_actor_ctx_t;

EV_STATIC_ASSERT(sizeof(ev_net_ingress_event_t) <= EV_MSG_INLINE_CAPACITY,
                 "network ingress event must fit inline message payload");
EV_STATIC_ASSERT(sizeof(ev_net_mqtt_publish_cmd_t) <= EV_MSG_INLINE_CAPACITY,
                 "network tx command must fit inline message payload");
EV_STATIC_ASSERT((EV_NET_INGRESS_RING_CAPACITY & (EV_NET_INGRESS_RING_CAPACITY - 1U)) == 0U,
                 "network ingress ring capacity must be a power of two");

ev_result_t ev_network_actor_init(ev_network_actor_ctx_t *ctx, ev_net_port_t *net_port);
ev_result_t ev_network_actor_handle(void *actor_context, const ev_msg_t *msg);
const ev_network_actor_stats_t *ev_network_actor_stats(const ev_network_actor_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* EV_NETWORK_ACTOR_H */
