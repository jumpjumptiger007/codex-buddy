#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/test_corebluetooth_scheduling.py
if [[ "$(uname -s)" != Darwin ]]; then
    echo "macOS CoreBluetooth compile: UNVERIFIED (requires macOS SDK)"
    exit 0
fi
output="$(mktemp -d "${TMPDIR:-/tmp}/codex-buddy-mac-ble.XXXXXX")"
trap 'rm -rf "$output"' EXIT
xcrun clang -fobjc-arc -Wall -Wextra -Werror -c \
    -Icompanion/core -Icompanion/mac -Icomponents/ambient_generation/include -Icomponents/ambient_auth/include \
    companion/mac/companion_corebluetooth.m -o "$output/companion_corebluetooth.o"
xcrun clang -fobjc-arc -Wall -Wextra -Werror -c -Icompanion/mac -Icomponents/ambient_auth/include \
    companion/mac/companion_auth_crypto.m -o "$output/companion_auth_crypto.o"
echo "macOS CoreBluetooth object compile: PASS (no manager, scan, connection, or permission invoked)"
