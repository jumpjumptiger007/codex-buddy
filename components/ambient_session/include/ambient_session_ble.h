#pragma once

#include "ambient_ble.h"
#include "ambient_transport.h"
#include "ambient_wire.h"

bool ambient_session_ble_transport_init(ambient_ble_t *ble,
                                        ambient_transport_t *transport);
