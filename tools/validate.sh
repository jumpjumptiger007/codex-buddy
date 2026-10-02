#!/usr/bin/env bash
set -euo pipefail

mode="${1:---all}"
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    echo "Usage: $0 [--all|--static|--firmware]" >&2
}

run_static_checks() {
    local actionlint_bin
    local test_dir
    local gc_sections_flag="-Wl,--gc-sections"

    if [[ "$(uname -s)" == "Darwin" ]]; then
        gc_sections_flag="-Wl,-dead_strip"
    fi

    python3 tools/check_repo.py
    python3 tests/test_r3_acceptance_isolation.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_ambient_ble_boundary.py

    actionlint_bin="${ACTIONLINT_BIN:-}"
    if [[ -z "${actionlint_bin}" ]]; then
        actionlint_bin="$(command -v actionlint || true)"
    fi
    if [[ -z "${actionlint_bin}" || ! -x "${actionlint_bin}" ]]; then
        actionlint_bin="$(./tools/install-actionlint.sh)"
    fi
    "${actionlint_bin}" -color .github/workflows/*.yml

    test_dir="$(mktemp -d /tmp/ai-passport-host-tests.XXXXXX)"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icompanion/core \
        tests/test_ambient_reducer.c companion/core/ambient_reducer.c \
        -o "${test_dir}/test_ambient_reducer"
    "${test_dir}/test_ambient_reducer"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icompanion/core \
        tests/test_ambient_quota.c companion/core/ambient_quota.c \
        -o "${test_dir}/test_ambient_quota"
    "${test_dir}/test_ambient_quota"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icompanion/core \
        tests/test_ambient_protocol.c companion/core/ambient_protocol.c \
        -o "${test_dir}/test_ambient_protocol"
    "${test_dir}/test_ambient_protocol"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icompanion/core -Icompanion/mac \
        tests/test_codex_hook_contract.c companion/mac/codex_hook_contract.c \
        companion/core/ambient_reducer.c -o "${test_dir}/test_codex_hook_contract"
    "${test_dir}/test_codex_hook_contract"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icompanion/core -Itests \
        tests/test_ambient_wire.c companion/core/ambient_wire.c \
        companion/core/ambient_protocol.c companion/core/ambient_transport.c \
        tests/ambient_fake_transport.c -o "${test_dir}/test_ambient_wire"
    "${test_dir}/test_ambient_wire"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icompanion/core \
        tests/test_ambient_settings.c companion/core/ambient_settings.c \
        -o "${test_dir}/test_ambient_settings"
    "${test_dir}/test_ambient_settings"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Icompanion/core -Itests \
        tests/test_ambient_transport.c tests/ambient_fake_transport.c \
        companion/core/ambient_transport.c \
        -o "${test_dir}/test_ambient_transport"
    "${test_dir}/test_ambient_transport"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Icompanion/core -Icompanion/mac \
        tests/test_rollout_watcher.c companion/mac/rollout_watcher.c \
        -o "${test_dir}/test_rollout_watcher"
    "${test_dir}/test_rollout_watcher"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Icompanion/core -Icompanion/mac \
        tests/test_rollout_quota_source.c \
        companion/mac/rollout_quota_source.c \
        companion/mac/app_server_quota_source.c \
        companion/core/ambient_quota.c \
        -o "${test_dir}/test_rollout_quota_source"
    "${test_dir}/test_rollout_quota_source"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Icompanion/core -Icompanion/mac \
        tests/test_quota_reset_state_store.c \
        companion/mac/quota_reset_state_store.c \
        companion/mac/rollout_quota_source.c \
        companion/core/ambient_quota.c \
        -o "${test_dir}/test_quota_reset_state_store"
    "${test_dir}/test_quota_reset_state_store"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Icompanion/core -Icompanion/mac -Itests \
        tests/test_companion_core.c tests/ambient_fake_transport.c \
        companion/mac/companion_core.c \
        companion/mac/rollout_watcher.c \
        companion/mac/rollout_quota_source.c \
        companion/mac/quota_reset_state_store.c \
        companion/core/ambient_reducer.c \
        companion/core/ambient_protocol.c companion/core/ambient_wire.c \
        companion/core/ambient_dedup.c \
        companion/core/ambient_quota.c \
        companion/core/ambient_transport.c \
        -o "${test_dir}/test_companion_core"
    "${test_dir}/test_companion_core"
    "${CC:-cc}" -D_GNU_SOURCE -DAMBIENT_BLE_HOST_TEST -std=c11 -Wall -Wextra -Werror \
        -Itests/ambient_ble_stubs -Icomponents/ambient_ble/include -Icomponents/ambient_ble/src \
        tests/test_ambient_ble_runtime.c tests/ambient_ble_stubs/ambient_ble_platform_fake.c \
        components/ambient_ble/src/ambient_ble.c components/ambient_ble/src/ambient_ble_gap.c \
        components/ambient_ble/src/ambient_ble_gatt.c components/ambient_ble/src/ambient_ble_tx.c \
        components/ambient_ble/src/ambient_ble_peer_nvs.c components/ambient_ble/src/ambient_ble_ring.c \
        -o "${test_dir}/test_ambient_ble_runtime"
    "${test_dir}/test_ambient_ble_runtime"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/ambient_ble_stubs -Icomponents/ambient_ble/include \
        -Icompanion/core -Icomponents/ambient_session/include -Iacceptance/r3_device/main \
        tests/test_r3_acceptance_policy.c -o "${test_dir}/test_r3_acceptance_policy"
    "${test_dir}/test_r3_acceptance_policy"
    for r2_suite in identifier_registry codex_source_adapter companion_ingestion companion_diagnostics companion_runtime companion_r2_integration companion_secure_bridge companion_ble_central; do
        "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icompanion/core -Icompanion/mac -Itests \
            "tests/test_${r2_suite}.c" companion/mac/identifier_registry.c \
            companion/mac/codex_source_adapter.c companion/mac/codex_hook_contract.c \
            companion/mac/companion_ingestion.c companion/mac/companion_diagnostics.c \
            companion/mac/companion_runtime.c companion/mac/companion_core.c \
            companion/mac/companion_secure_bridge.c companion/mac/companion_ble_central.c components/ambient_auth/src/ambient_auth.c \
            components/ambient_generation/src/ambient_generation.c \
            -Icomponents/ambient_generation/include -Icomponents/ambient_session/include -Icomponents/ambient_auth/include \
            components/ambient_session/src/ambient_session.c \
            companion/mac/rollout_watcher.c companion/mac/rollout_quota_source.c \
            companion/mac/quota_reset_state_store.c companion/core/ambient_reducer.c \
            companion/core/ambient_dedup.c companion/core/ambient_quota.c \
            companion/core/ambient_protocol.c companion/core/ambient_wire.c \
            companion/core/ambient_transport.c tests/ambient_fake_transport.c \
            -o "${test_dir}/test_${r2_suite}"
        "${test_dir}/test_${r2_suite}"
    done
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icompanion/mac \
        tests/test_permission_observer.c companion/mac/permission_observer.c \
        -o "${test_dir}/test_permission_observer"
    "${test_dir}/test_permission_observer"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icompanion/mac \
        tests/test_stt_backend.c companion/mac/stt_backend.c \
        -o "${test_dir}/test_stt_backend"
    "${test_dir}/test_stt_backend"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icompanion/mac \
        tests/test_composer_injection.c companion/mac/composer_injection.c \
        -o "${test_dir}/test_composer_injection"
    "${test_dir}/test_composer_injection"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Icompanion/core -Icompanion/mac \
        tests/test_gate2_integration.c \
        companion/mac/companion_core.c \
        companion/mac/permission_observer.c \
        companion/mac/stt_backend.c \
        companion/mac/composer_injection.c \
        companion/mac/rollout_watcher.c \
        companion/mac/rollout_quota_source.c \
        companion/mac/quota_reset_state_store.c \
        companion/core/ambient_reducer.c \
        companion/core/ambient_protocol.c companion/core/ambient_wire.c \
        companion/core/ambient_dedup.c \
        companion/core/ambient_quota.c \
        companion/core/ambient_transport.c \
        -o "${test_dir}/test_gate2_integration"
    "${test_dir}/test_gate2_integration"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icompanion/core \
        tests/test_ambient_dedup.c companion/core/ambient_dedup.c \
        -o "${test_dir}/test_ambient_dedup"
    "${test_dir}/test_ambient_dedup"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_ui_pixel_math.c main/ui_pixel_math.c \
        -o "${test_dir}/test_ui_pixel_math"
    "${test_dir}/test_ui_pixel_math"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_passport_ui_model.c main/passport_ui_model.c \
        -o "${test_dir}/test_passport_ui_model"
    "${test_dir}/test_passport_ui_model"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_passport_ui_presenter.c main/passport_ui_presenter.c \
        -o "${test_dir}/test_passport_ui_presenter"
    "${test_dir}/test_passport_ui_presenter"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/passport_ui_stubs -Imain \
        tests/test_passport_ui_shell.c main/passport_ui_shell.c \
        main/passport_ui_model.c main/passport_ui_presenter.c \
        -o "${test_dir}/test_passport_ui_shell"
    "${test_dir}/test_passport_ui_shell"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_demo_navigation.c main/demo_navigation.c \
        -o "${test_dir}/test_demo_navigation"
    "${test_dir}/test_demo_navigation"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icomponents/bsp/src \
        tests/test_bsp_display_rounding.c components/bsp/src/bsp_display_rounding.c \
        -o "${test_dir}/test_bsp_display_rounding"
    "${test_dir}/test_bsp_display_rounding"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icomponents/bsp/src \
        tests/test_bsp_es8311_sleep_check.c components/bsp/src/bsp_es8311_sleep_check.c \
        -o "${test_dir}/test_bsp_es8311_sleep_check"
    "${test_dir}/test_bsp_es8311_sleep_check"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/bsp_stubs -Icomponents/bsp/include \
        tests/test_bsp_button.c -o "${test_dir}/test_bsp_button"
    "${test_dir}/test_bsp_button"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/bsp_stubs -Icomponents/bsp/include \
        tests/test_bsp_lvgl_init.c components/bsp/src/bsp_display_rounding.c \
        -o "${test_dir}/test_bsp_lvgl_init"
    "${test_dir}/test_bsp_lvgl_init"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/audio_stubs -Icomponents/bsp/include -Icomponents/bsp/src \
        tests/test_bsp_audio_recovery.c components/bsp/src/bsp_es8311_sleep_check.c \
        -o "${test_dir}/test_bsp_audio_recovery"
    "${test_dir}/test_bsp_audio_recovery"
    for demo in audio low_power ble wifi; do
        "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
            -ffunction-sections -fdata-sections -Itests/demo_stubs -Imain \
            "tests/test_demo_${demo}_runtime.c" "$gc_sections_flag" \
            -o "${test_dir}/test_demo_${demo}_runtime"
        "${test_dir}/test_demo_${demo}_runtime"
    done
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Icomponents/ambient_ble/src \
        tests/test_ambient_ble_ring.c components/ambient_ble/src/ambient_ble_ring.c \
        -o "${test_dir}/test_ambient_ble_ring"
    "${test_dir}/test_ambient_ble_ring"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Icomponents/ambient_generation/include \
        tests/test_ambient_generation.c components/ambient_generation/src/ambient_generation.c \
        -o "${test_dir}/test_ambient_generation"
    "${test_dir}/test_ambient_generation"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests -Icomponents/ambient_auth/include \
        tests/test_ambient_auth.c components/ambient_auth/src/ambient_auth.c components/ambient_auth/src/ambient_identity.c \
        -o "${test_dir}/test_ambient_auth"
    "${test_dir}/test_ambient_auth"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Icompanion/mac -Icomponents/ambient_generation/include \
        tests/test_companion_generation_store.c companion/mac/companion_generation_store.c \
        components/ambient_generation/src/ambient_generation.c \
        -o "${test_dir}/test_companion_generation_store"
    "${test_dir}/test_companion_generation_store"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Icomponents/ambient_session/include -Icomponents/ambient_auth/include -Icomponents/ambient_generation/include \
        -Icompanion/core -Itests \
        tests/test_ambient_session.c components/ambient_session/src/ambient_session.c \
        components/ambient_generation/src/ambient_generation.c \
        companion/core/ambient_wire.c companion/core/ambient_protocol.c \
        companion/core/ambient_transport.c tests/ambient_fake_transport.c \
        -o "${test_dir}/test_ambient_session"
    "${test_dir}/test_ambient_session"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests -Icompanion/core \
        -Icomponents/ambient_auth/include -Icomponents/ambient_session/include \
        tests/test_ambient_authenticated_session.c components/ambient_auth/src/ambient_auth.c \
        components/ambient_session/src/ambient_authenticated_session.c components/ambient_session/src/ambient_session.c \
        companion/core/ambient_wire.c companion/core/ambient_protocol.c companion/core/ambient_transport.c \
        tests/ambient_fake_transport.c -o "${test_dir}/test_ambient_authenticated_session"
    "${test_dir}/test_ambient_authenticated_session"
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_deep_sleep_contract.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_passport_ui_startup.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_check_repo.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_verify_firmware.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_archive_firmware.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_install_passport_skills.py
    rm -rf "${test_dir}"
    ./tools/check_r3_acceptance_mac.sh
    echo "Host tests: PASS"
}

run_firmware_checks() (
    # Subshell owns this variable through its EXIT trap (not a function local).

    if ! command -v idf.py >/dev/null 2>&1; then
        echo "ERROR: idf.py is not available; activate ESP-IDF 5.5.3 first." >&2
        return 1
    fi

    validation_build_dir="$(mktemp -d /tmp/ai-passport-firmware.XXXXXX)"
    trap 'case "${validation_build_dir}" in /tmp/ai-passport-firmware.*) rm -rf -- "${validation_build_dir}" ;; esac' EXIT

    SDKCONFIG_DEFAULTS="${repo_root}/sdkconfig.defaults" \
        idf.py -B "${validation_build_dir}" \
        -D "SDKCONFIG=${validation_build_dir}/sdkconfig" build
    idf.py -B "${validation_build_dir}" merge-bin \
        -o "${validation_build_dir}/FoloToy-AI-Passport-full.bin"
    python3 tools/verify_firmware.py "${validation_build_dir}"
    PYTHONDONTWRITEBYTECODE=1 python3 tools/archive_firmware.py create \
        "${validation_build_dir}" --archive-root "${repo_root}/build/firmware"
    mkdir -p "${repo_root}/build"
    install -m 0644 \
        "${validation_build_dir}/FoloToy-AI-Passport-full.bin" \
        "${repo_root}/build/FoloToy-AI-Passport-full.bin"
    echo "Firmware build: PASS"
)

cd "${repo_root}"
case "${mode}" in
    --all)
        run_static_checks
        run_firmware_checks
        ;;
    --static)
        run_static_checks
        ;;
    --firmware)
        run_firmware_checks
        ;;
    *)
        usage
        exit 2
        ;;
esac
