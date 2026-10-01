#include "ambient_settings.h"
bool ambient_settings_accept(const ambient_settings_t *candidate, ambient_settings_t *current)
{
    if (!candidate || !current || candidate->version != AMBIENT_SETTINGS_VERSION
        || candidate->keys) return false;
    *current = *candidate; return true;
}
bool ambient_local_action_effects(ambient_local_action_t action, bool local,
                                 ambient_local_effects_t *effects)
{
    if (effects) *effects = (ambient_local_effects_t){0};
    if (!effects || !local || (action != AMBIENT_LOCAL_UNPAIR
        && action != AMBIENT_LOCAL_FACTORY_RESET)) return false;
    *effects = (ambient_local_effects_t){.clear_bond = true, .clear_link_session = true,
        .clear_product_settings = action == AMBIENT_LOCAL_FACTORY_RESET}; return true;
}
bool ambient_local_ack_valid(ambient_local_ack_t ack)
{
    return ack >= AMBIENT_LOCAL_ACK_SUCCESS && ack <= AMBIENT_LOCAL_ACK_UNAVAILABLE;
}
