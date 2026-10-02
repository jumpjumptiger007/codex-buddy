"""Compile-only acceptance separation; this script never runs endpoint binaries."""
from pathlib import Path
import plistlib
product = Path('main/main.c').read_text()
seam = Path('main/ambient_r3_build_seam.c').read_text()
assert 'ambient_ble_start(' not in product and 'ambient_identity_esp_open(' not in product
assert 's_ble_start_reference = ambient_ble_start;' in seam and 'ambient_ble_start(' not in seam
app = Path('acceptance/r3_device/main/r3_acceptance.c').read_text()
assert app.index('ambient_identity_esp_open(') < app.index('bsp_display_init(') < app.index('bsp_button_init(') < app.index('ambient_ble_start(')
assert 'ambient_ble_internal.h' not in app
assert 'phase=PRE_BLE' in app and 'phase=POST_TEARDOWN' in app and 'phase=MAX_FRAME_FAULT' in app
assert 'ambient_ble_stop(ble)' in app and 'stopped=true' in app
assert 'r3_acceptance_link(&current)' in app and 'ambient_ble_get_metrics' in app
pair = app[app.index('static bool pairing('):app.index('static void fatal_wait(')]
assert 'atomic_exchange' in pair and 'xQueueSend(events,&e,0)' in pair
assert all(s not in pair for s in ('lv_', 'nvs_', 'while', 'ulTaskNotifyTake'))
runner = Path('acceptance/r3_mac/main.m').read_text()
assert 'CBCentralManager alloc' in runner and 'DISPATCH_QUEUE_SERIAL' in runner
assert 'COMPANION_BLE_SERVICE_UUID' in runner and 'peripheral.identifier isEqual:_selected' in runner
assert 'companion_ble_central_begin(&_core,++_peer,&_runtime,&_generation.backend,NULL)' in runner
assert 'companion_ble_central_bind(core, _incarnation, _peer, &platform)' in Path('companion/mac/companion_corebluetooth.m').read_text()
assert 'dummy' not in runner and 'readLine' not in runner
for file in ('tools/validate.sh', 'tools/check_r3_acceptance_mac.sh', 'tools/check_r3_acceptance_device.sh'):
    gate = Path(file).read_text()
    assert '"$output/r3-acceptance"\n' not in gate.replace('-o "$output/r3-acceptance"\n','')
    assert 'idf.py flash' not in gate and 'scanForPeripherals' not in gate
assert 'EXTRA_COMPONENT_DIRS' in Path('acceptance/r3_device/CMakeLists.txt').read_text()
assert 'r3_acceptance.c' not in Path('main/CMakeLists.txt').read_text()
print('R3 isolated acceptance entry/entropy/callback/compile-only boundaries: PASS (static)')

metadata=plistlib.loads(Path('acceptance/r3_mac/Info.plist').read_bytes())
assert metadata['NSBluetoothAlwaysUsageDescription']
assert '-sectcreate,__TEXT,__info_plist' in Path('tools/check_r3_acceptance_mac.sh').read_text()
