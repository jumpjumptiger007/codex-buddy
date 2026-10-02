#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/test_r3_acceptance_isolation.py
if [[ "$(uname -s)" != Darwin ]]; then echo 'Mac acceptance link: UNVERIFIED (macOS required)'; exit 0; fi
output="$(mktemp -d "${TMPDIR:-/tmp}/codex-r3-mac-accept.XXXXXX")"
trap 'rm -rf "$output"' EXIT
includes=(-Icompanion/core -Icompanion/mac -Icomponents/ambient_generation/include -Icomponents/ambient_auth/include)
for source in companion/core/*.c companion/mac/*.c components/ambient_generation/src/ambient_generation.c components/ambient_auth/src/ambient_auth.c; do
    xcrun clang -std=c11 -Wall -Wextra -Werror "${includes[@]}" -c "$source" -o "$output/$(basename "$source").o"
done
xcrun clang -fobjc-arc -Wall -Wextra -Werror -Wl,-fatal_warnings "${includes[@]}" \
    acceptance/r3_mac/main.m companion/mac/companion_corebluetooth.m companion/mac/companion_auth_crypto.m \
    "$output/"*.o -Wl,-sectcreate,__TEXT,__info_plist,acceptance/r3_mac/Info.plist -framework Foundation -framework CoreBluetooth -framework Security \
    -o "$output/r3-acceptance"
xcrun otool -s __TEXT __info_plist "$output/r3-acceptance" >/dev/null
# Retain only with an explicit caller-selected build output, never run here.
if [[ -n "${R3_ACCEPTANCE_MAC_OUTPUT:-}" ]]; then install -m 0755 "$output/r3-acceptance" "$R3_ACCEPTANCE_MAC_OUTPUT"; fi
echo 'Mac acceptance executable warnings-as-errors link: PASS (NOT RUN)'
