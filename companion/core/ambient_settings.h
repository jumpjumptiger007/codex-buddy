#pragma once
#include <stdbool.h>
#include <stdint.h>
#define AMBIENT_SETTINGS_VERSION 1U
/* R1 reserves no product preference keys. Unknown future schema/key fails closed. */
typedef struct { uint8_t version; uint32_t keys; } ambient_settings_t;
typedef enum { AMBIENT_LOCAL_UNPAIR = 1, AMBIENT_LOCAL_FACTORY_RESET } ambient_local_action_t;
typedef struct {
    bool clear_bond, clear_link_session, clear_product_settings;
    /* Host quota/history and Codex actions are deliberately absent. */
} ambient_local_effects_t;
bool ambient_settings_accept(const ambient_settings_t *candidate,
                             ambient_settings_t *current);
/* Pure policy only: caller performs authorized local effects and acknowledges
 * success only after all requested effects complete. No remote action opcode.
 * Any supplied effects object is cleared before validating a request. */
bool ambient_local_action_effects(ambient_local_action_t action,
                                 bool explicit_local_request,
                                 ambient_local_effects_t *effects);
typedef enum { AMBIENT_LOCAL_ACK_SUCCESS = 1, AMBIENT_LOCAL_ACK_FAILED,
    AMBIENT_LOCAL_ACK_UNAVAILABLE } ambient_local_ack_t;
bool ambient_local_ack_valid(ambient_local_ack_t ack);
