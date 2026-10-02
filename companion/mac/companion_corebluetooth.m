#import "companion_corebluetooth.h"
#include <time.h>

@implementation CompanionCoreBluetooth {
    companion_ble_central_t *_core;
    CBPeripheral *_peripheral;
    CBCharacteristic *_rx;
    CBCharacteristic *_tx;
    uint64_t _incarnation, _peer;
    companion_pin_auth_owner_t *_pinOwner;
    BOOL _enrollment;
}
static ambient_transport_result_t platform_write(void *context, const uint8_t *bytes, size_t n)
{
    CompanionCoreBluetooth *wrapper = (__bridge CompanionCoreBluetooth *)context;
    return [wrapper writeBytes:bytes length:n];
}
- (ambient_transport_result_t)writeBytes:(const uint8_t *)bytes length:(size_t)n {
    if (![self current:_peripheral] || _peripheral.state != CBPeripheralStateConnected || !_rx)
        return AMBIENT_TRANSPORT_DISCONNECTED;
    if (!(_rx.properties & CBCharacteristicPropertyWriteWithoutResponse)) return AMBIENT_TRANSPORT_INVALID;
    if (!_peripheral.canSendWriteWithoutResponse) return AMBIENT_TRANSPORT_WOULD_BLOCK;
    if (n > [_peripheral maximumWriteValueLengthForType:CBCharacteristicWriteWithoutResponse])
        return AMBIENT_TRANSPORT_TOO_LARGE;
    [_peripheral writeValue:[NSData dataWithBytes:bytes length:n]
         forCharacteristic:_rx type:CBCharacteristicWriteWithoutResponse];
    return AMBIENT_TRANSPORT_OK;
}
- (BOOL)current:(CBPeripheral *)peripheral {
    return _core && _core->alive && peripheral == _peripheral &&
        _core->link.link_incarnation == _incarnation && _core->peer_token == _peer;
}
- (instancetype)initWithCore:(companion_ble_central_t *)core peripheral:(CBPeripheral *)peripheral {
    self = [super init];
    if (self) {
        _core = core; _peripheral = peripheral;
        _incarnation = core ? core->link.link_incarnation : 0;
        _peer = core ? core->peer_token : 0;
        /* CB facts are transport-only. Application auth is configured by the
         * caller with its explicit durable pin store; no BLE security guesses. */
        if ([self current:peripheral]) {
            companion_ble_central_platform_t platform = {platform_write, (__bridge void *)self, 20};
            if (!companion_ble_central_bind(core, _incarnation, _peer, &platform)) { _core=NULL; return nil; }
            peripheral.delegate = self;
        }
    }
    return self;
}
- (void)pumpPendingBytes {
    if (![self current:_peripheral]) return;
    struct timespec monotonic;
    if (clock_gettime(CLOCK_MONOTONIC, &monotonic) != 0) { [self invalidate]; return; }
    uint64_t now = (uint64_t)monotonic.tv_sec * 1000U + (uint64_t)monotonic.tv_nsec / 1000000U;
    /* Wall time only supplies R2 freshness; neither clock allocates identity. */
    ambient_transport_result_t result = companion_ble_central_pump(_core, _incarnation, _peer, now, (int64_t)time(NULL));
    if (result != AMBIENT_TRANSPORT_OK && result != AMBIENT_TRANSPORT_WOULD_BLOCK) [self invalidate];
}
- (void)peripheralIsReadyToSendWriteWithoutResponse:(CBPeripheral *)peripheral {
    if ([self current:peripheral]) [self pumpPendingBytes];
}
- (BOOL)startAuthentication {
    if (![self current:_peripheral] || !_pinOwner || !_pinOwner->store ||
        !_pinOwner->store->open || _pinOwner->store->poisoned ||
        _core->auth.state != AMBIENT_AUTH_CLOSED) return NO;
    uint8_t key[65];BOOL pinned=companion_pin_store_key(_pinOwner->store,key);
    if (!pinned && (!_enrollment || !_pinOwner->approve)) return NO;
    ambient_auth_crypto_t crypto=companion_pin_auth_crypto(_pinOwner);
    BOOL started = companion_ble_central_authenticate(_core,_incarnation,_peer,pinned?key:NULL,_enrollment,&crypto);
    if (started) [self pumpPendingBytes];
    return started && [self current:_peripheral];
}
- (BOOL)configureAuthenticationWithOwner:(companion_pin_auth_owner_t *)owner explicitEnrollment:(BOOL)enrollment {
    if (![self current:_peripheral] || !owner || !owner->store || !owner->store->open || owner->store->poisoned) return NO;
    _pinOwner=owner;_enrollment=enrollment;
    if (_core->link.rx_notify_subscribed) return [self startAuthentication];
    return YES;
}
- (void)discoverAuthorizedPeripheral {
    if ([self current:_peripheral] && _peripheral.state == CBPeripheralStateConnected)
        [_peripheral discoverServices:@[[CBUUID UUIDWithString:@COMPANION_BLE_SERVICE_UUID]]];
}
- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error {
    if (![self current:peripheral]) return;
    if (error) { [self invalidate]; return; }
    for (CBService *service in peripheral.services) {
        if ([service.UUID isEqual:[CBUUID UUIDWithString:@COMPANION_BLE_SERVICE_UUID]]) {
            (void)companion_ble_central_service(_core, _incarnation, _peer, COMPANION_BLE_SERVICE_UUID);
            [peripheral discoverCharacteristics:@[[CBUUID UUIDWithString:@COMPANION_BLE_RX_UUID],
                [CBUUID UUIDWithString:@COMPANION_BLE_TX_UUID]] forService:service];
        }
    }
}
- (void)peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error {
    if (![self current:peripheral]) return;
    if (error || ![service.UUID isEqual:[CBUUID UUIDWithString:@COMPANION_BLE_SERVICE_UUID]]) { [self invalidate]; return; }
    for (CBCharacteristic *characteristic in service.characteristics) {
        if ([characteristic.UUID isEqual:[CBUUID UUIDWithString:@COMPANION_BLE_RX_UUID]]) {
            if (companion_ble_central_characteristic(_core, _incarnation, _peer,
                COMPANION_BLE_SERVICE_UUID, COMPANION_BLE_RX_UUID,
                (characteristic.properties & CBCharacteristicPropertyWriteWithoutResponse) != 0, false)) _rx = characteristic;
        } else if ([characteristic.UUID isEqual:[CBUUID UUIDWithString:@COMPANION_BLE_TX_UUID]]) {
            if (companion_ble_central_characteristic(_core, _incarnation, _peer,
                COMPANION_BLE_SERVICE_UUID, COMPANION_BLE_TX_UUID, false,
                (characteristic.properties & CBCharacteristicPropertyNotify) != 0)) _tx = characteristic;
        }
    }
    if (_rx && _tx) [peripheral setNotifyValue:YES forCharacteristic:_tx];
}
- (void)peripheral:(CBPeripheral *)peripheral didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
    if (![self current:peripheral] || characteristic != _tx) return;
    (void)companion_ble_central_subscription(_core, _incarnation, _peer, !error && characteristic.isNotifying);
    if (!error && characteristic.isNotifying) {
        if (![self startAuthentication]) [self pumpPendingBytes];
    }
}
- (void)peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
    if (![self current:peripheral] || characteristic != _tx) return;
    if (error) { [self invalidate]; return; }
    ambient_transport_result_t result = companion_ble_central_notification(_core, _incarnation, _peer,
        characteristic.value.bytes, characteristic.value.length);
    if (result != AMBIENT_TRANSPORT_OK) { [self invalidate]; return; }
    [self pumpPendingBytes];
}
- (void)dealloc { [self invalidate]; }
- (void)invalidate {
    if ([self current:_peripheral]) companion_ble_central_disconnect(_core, _incarnation, _peer);
    if (_peripheral.delegate == self) _peripheral.delegate = nil;
    _core = NULL; _rx = nil; _tx = nil; _pinOwner=NULL;
}
@end
