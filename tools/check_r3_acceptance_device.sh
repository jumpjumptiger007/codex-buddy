#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo"
python3 tests/test_r3_acceptance_isolation.py
idf.py --version | grep -F 'v5.5.3'
# Build output is explicitly supplied and must be fresh. Preserve acceptance artifacts.
: "${R3_ACCEPTANCE_DEVICE_BUILD:?supply a fresh explicit build directory}"
if [[ -e "$R3_ACCEPTANCE_DEVICE_BUILD" ]]; then echo 'ERROR: acceptance build directory must be fresh' >&2; exit 2; fi
mkdir -p "$R3_ACCEPTANCE_DEVICE_BUILD"
SDKCONFIG_DEFAULTS="$repo/sdkconfig.defaults;$repo/acceptance/r3_device/sdkconfig.defaults" \
    idf.py -C acceptance/r3_device -B "$R3_ACCEPTANCE_DEVICE_BUILD" \
    -D "SDKCONFIG=$R3_ACCEPTANCE_DEVICE_BUILD/sdkconfig" -D IDF_TARGET=esp32c3 build
idf.py -C acceptance/r3_device -B "$R3_ACCEPTANCE_DEVICE_BUILD" merge-bin \
    -o "$R3_ACCEPTANCE_DEVICE_BUILD/R3-Acceptance-full.bin"
python3 tools/verify_r3_acceptance.py "$R3_ACCEPTANCE_DEVICE_BUILD"
echo "Acceptance artifacts retained: $R3_ACCEPTANCE_DEVICE_BUILD (no flash)"
