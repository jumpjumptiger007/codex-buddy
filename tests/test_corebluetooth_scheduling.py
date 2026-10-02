"""Check the compiled wrapper wires actual delegate events to the host-tested pump.

This checks source wiring, not CoreBluetooth runtime behavior or radio delivery.
"""
from pathlib import Path
import re

source = Path('companion/mac/companion_corebluetooth.m').read_text()

def method(name):
    start = source.index(name)
    body = source.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[body:end]

pump = method('- (void)pumpPendingBytes')
assert 'current:_peripheral' in pump
assert 'companion_ble_central_pump(_core, _incarnation, _peer' in pump
assert 'CLOCK_MONOTONIC' in pump and 'time(NULL)' in pump
assert '[self invalidate]' in pump
assert '[self pumpPendingBytes]' in method('- (BOOL)startAuthentication')
assert '[self startAuthentication]' in method('- (BOOL)configureAuthenticationWithOwner:')
for name in ('didUpdateNotificationStateForCharacteristic:', 'didUpdateValueForCharacteristic:',
             '- (void)peripheralIsReadyToSendWriteWithoutResponse:'):
    body = method(name)
    assert 'current:peripheral' in body
    assert '[self pumpPendingBytes]' in body
rx = method('didUpdateValueForCharacteristic:')
assert rx.index('companion_ble_central_notification') < rx.index('[self pumpPendingBytes]')
assert 'result != AMBIENT_TRANSPORT_OK' in rx and '[self invalidate]' in rx
invalidate = method('- (void)invalidate')
assert 'companion_ble_central_disconnect' in invalidate and '_core = NULL' in invalidate
assert 'canSendWriteWithoutResponse' in method('- (ambient_transport_result_t)writeBytes:')
assert not re.search(r'\b(while|dispatch_async|dispatch_after)\s*\(', source)
assert 'CBCentralManager' not in source and 'scanForPeripherals' not in source
print('CoreBluetooth delegate → bounded production pump wiring: PASS (static; no Bluetooth execution)')
