#pragma once
#include "ambient_auth.h"
/* Public Apple Security + CommonCrypto backend, no private APIs/Bluetooth. */
ambient_auth_crypto_t companion_auth_crypto(void);
bool companion_auth_public_key_valid(const uint8_t[65]);
