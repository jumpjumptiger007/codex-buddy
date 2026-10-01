#include <assert.h>
#include <stddef.h>
#include "ambient_settings.h"
int main(void)
{
    ambient_settings_t current = {1,0}, candidate = {2,0};
    assert(!ambient_settings_accept(&candidate, &current) && current.version == 1);
    candidate = (ambient_settings_t){1,1};
    assert(!ambient_settings_accept(&candidate, &current) && current.keys == 0);
    candidate.keys = 0; assert(ambient_settings_accept(&candidate, &current));
    assert(!ambient_settings_accept(NULL, &current));
    ambient_local_effects_t effects = {0};
    assert(!ambient_local_action_effects(AMBIENT_LOCAL_UNPAIR, false, &effects));
    assert(!effects.clear_bond);
    assert(!ambient_local_action_effects((ambient_local_action_t)3, true, &effects));
    assert(ambient_local_action_effects(AMBIENT_LOCAL_UNPAIR, true, &effects));
    assert(effects.clear_bond && effects.clear_link_session && !effects.clear_product_settings);
    assert(!ambient_local_action_effects(AMBIENT_LOCAL_UNPAIR, false, &effects));
    assert(!effects.clear_bond && !effects.clear_link_session && !effects.clear_product_settings);
    assert(ambient_local_action_effects(AMBIENT_LOCAL_FACTORY_RESET, true, &effects));
    assert(effects.clear_product_settings);
    assert(!ambient_local_action_effects((ambient_local_action_t)99, true, &effects));
    assert(!effects.clear_bond && !effects.clear_link_session && !effects.clear_product_settings);
    assert(!ambient_local_action_effects(AMBIENT_LOCAL_UNPAIR, true, NULL));
    assert(!ambient_local_ack_valid(0) && !ambient_local_ack_valid(4));
    assert(ambient_local_ack_valid(AMBIENT_LOCAL_ACK_FAILED));
    return 0;
}
