#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Darwin ]]; then
    echo 'Production Apple crypto: UNVERIFIED (requires macOS)'
    exit 0
fi
: "${R3_MBEDTLS_SOURCE:?set to the pinned ESP-IDF mbedTLS source}"
: "${R3_MBEDTLS_HOST_BUILD:?set to its temporary host CMake build}"
output="$(mktemp -d "${TMPDIR:-/tmp}/codex-r3-crypto.XXXXXX")"
trap 'rm -rf "$output"' EXIT
includes=(-Icompanion/mac -Icomponents/ambient_auth/include -Icomponents/ambient_generation/include -Itests/identity_esp_stubs -I"$R3_MBEDTLS_SOURCE/include")
for source in components/ambient_auth/src/ambient_auth.c components/ambient_auth/src/ambient_identity.c components/ambient_auth/src/ambient_identity_esp.c companion/mac/companion_pin_store.c tests/identity_esp_stubs/platform.c; do
    xcrun clang -mmacosx-version-min=15.7 -std=c11 -Wall -Wextra -Werror "${includes[@]}" -c "$source" -o "$output/$(basename "$source").o"
done
xcrun clang -mmacosx-version-min=15.7 -Wl,-fatal_warnings -fobjc-arc -Wall -Wextra -Werror "${includes[@]}" \
    tests/test_r3_production_crypto.m companion/mac/companion_auth_crypto.m "$output/"*.o \
    "$R3_MBEDTLS_HOST_BUILD/library/libmbedcrypto.a" -framework Security -framework Foundation \
    -o "$output/test_crypto"
"$output/test_crypto"
