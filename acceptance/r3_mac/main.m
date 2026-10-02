/* Explicitly run only after physical/Bluetooth authorization. Compile gates never run it. */
#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>
#include <signal.h>
#include <string.h>
#include "fingerprint.h"
#include "companion_corebluetooth.h"
#include "companion_generation_store.h"

@interface R3AcceptanceOwner : NSObject <CBCentralManagerDelegate>
- (instancetype)initWithPeripheral:(NSUUID *)identifier pin:(NSString *)pin
    generation:(NSString *)generation fingerprint:(NSString *)fingerprint cycles:(unsigned)cycles;
- (void)start;
@end
@implementation R3AcceptanceOwner {
    CBCentralManager *_manager;
    CBPeripheral *_peripheral;
    CompanionCoreBluetooth *_wrapper;
    dispatch_queue_t _queue;
    dispatch_source_t _timer, _interrupt;
    NSUUID *_selected;
    NSString *_pinPath, *_generationPath, *_expected;
    companion_ble_central_t _core;
    companion_runtime_t _runtime;
    companion_runtime_lease_t _lease;
    companion_generation_store_t _generation;
    companion_pin_store_t _pin;
    companion_pin_auth_owner_t _pinOwner;
    unsigned _cycles, _completed, _attempts;
    BOOL _connecting, _finished, _measured;
    uint64_t _peer, _previousGeneration;
    unsigned _seconds;
    uint8_t _lastChallengeHash[32], _lastNonceHash[32];
}
static bool approve_pin(void *context,const uint8_t key[65],const char *fingerprint) {
    (void)key;R3AcceptanceOwner *owner=(__bridge R3AcceptanceOwner *)context;
    return [owner matchesFingerprint:fingerprint];
}
- (BOOL)matchesFingerprint:(const char *)fingerprint {
    return _expected && [_expected isEqualToString:[NSString stringWithUTF8String:fingerprint]];
}
- (instancetype)initWithPeripheral:(NSUUID *)identifier pin:(NSString *)pin
    generation:(NSString *)generation fingerprint:(NSString *)fingerprint cycles:(unsigned)cycles {
    if((self=[super init])){_selected=identifier;_pinPath=pin;_generationPath=generation;
        _expected=fingerprint;_cycles=cycles;_queue=dispatch_queue_create("r3.acceptance",DISPATCH_QUEUE_SERIAL);}
    return self;
}
- (void)finish:(int)code {
    if(_finished)return;_finished=YES;
    [_manager stopScan];[_wrapper invalidate];_wrapper=nil;
    if(_peripheral)[_manager cancelPeripheralConnection:_peripheral];
    companion_ble_central_teardown(&_core);companion_runtime_stop(&_runtime);
    companion_pin_store_close(&_pin);companion_generation_store_close(&_generation);
    if(_timer)dispatch_source_cancel(_timer);if(_interrupt)dispatch_source_cancel(_interrupt);
    fprintf(stderr,"phase=STOP result=%d\n",code);exit(code);
}
- (void)scan {
    if(_finished||_connecting||_manager.state!=CBManagerStatePoweredOn)return;
    if(++_attempts>_cycles+2){[self finish:2];return;}
    _seconds=0;fprintf(stderr,"phase=SCAN attempt=%u\n",_attempts);
    [_manager scanForPeripheralsWithServices:@[[CBUUID UUIDWithString:@COMPANION_BLE_SERVICE_UUID]]
        options:@{CBCentralManagerScanOptionAllowDuplicatesKey:@NO}];
}
- (void)start {
    dispatch_async(_queue,^{
        if(!companion_pin_store_open(&self->_pin,self->_pinPath.fileSystemRepresentation,NULL) ||
            !companion_generation_store_open(&self->_generation,self->_generationPath.fileSystemRepresentation,NULL)){
            [self finish:2];return;
        }
        uint8_t key[65];
        if(!companion_pin_store_key(&self->_pin,key) && !self->_expected){[self finish:2];return;}
        self->_pinOwner=(companion_pin_auth_owner_t){&self->_pin,approve_pin,(__bridge void *)self};
        companion_runtime_options_t options={.lease=&self->_lease,.epoch=1,.freshness_ms=30000,
            .done_hold_ms=1000,.profile=CODEX_SOURCE_DISABLED};
        if(!companion_runtime_start(&self->_runtime,&options)){[self finish:2];return;}
        /* Only this explicitly invoked test executable creates a manager. */
        self->_manager=[[CBCentralManager alloc] initWithDelegate:self queue:self->_queue
            options:@{CBCentralManagerOptionShowPowerAlertKey:@NO}];
        self->_timer=dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,0,0,self->_queue);
        dispatch_source_set_timer(self->_timer,dispatch_time(DISPATCH_TIME_NOW,NSEC_PER_SEC),NSEC_PER_SEC,10000000);
        dispatch_source_set_event_handler(self->_timer,^{[self tick];});dispatch_resume(self->_timer);
        signal(SIGINT,SIG_IGN);
        self->_interrupt=dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL,SIGINT,0,self->_queue);
        dispatch_source_set_event_handler(self->_interrupt,^{[self finish:0];});dispatch_resume(self->_interrupt);
    });
}
- (void)tick {
    if(_finished)return;
    if(++_seconds>30){fprintf(stderr,"failure=DEADLINE\n");[self finish:2];return;}
    if(_wrapper){
        [_wrapper pumpPendingBytes];
        if(!_core.alive){[_manager cancelPeripheralConnection:_peripheral];return;}
        if(!_measured && _runtime.wire.ready && _runtime.wire.emitted_revision &&
            _runtime.wire.emitted_revision==_runtime.wire.acknowledged_revision){
            if(_core.last_frame_bytes!=95 || _core.last_frame_chunks!=5){
                fprintf(stderr,"failure=SNAPSHOT_CHUNKS\n");[self finish:2];return;}
            fprintf(stderr,"phase=SNAPSHOT_WRITES bytes=%zu chunks=%zu writes=%llu protocol_ack=1\n",
                _core.last_frame_bytes,_core.last_frame_chunks,(unsigned long long)_core.platform_writes);
            if(_core.bridge.generation<=_previousGeneration){[self finish:2];return;}
            uint8_t challengeHash[32],nonceHash[32];
            if(!_core.auth.crypto.hash(_core.auth.crypto.context,_core.auth.challenge,32,challengeHash) ||
                !_core.auth.crypto.hash(_core.auth.crypto.context,_core.auth.nonce,32,nonceHash) ||
                (_completed && (!memcmp(challengeHash,_lastChallengeHash,32) || !memcmp(nonceHash,_lastNonceHash,32)))){
                fprintf(stderr,"failure=NONCE_REUSE_OR_HASH\n");[self finish:2];return;
            }
            memcpy(_lastChallengeHash,challengeHash,32);memcpy(_lastNonceHash,nonceHash,32);
            fprintf(stderr,"phase=FRESH_APP_AUTH_NONCES\n");
            _previousGeneration=_core.bridge.generation;_measured=YES;_completed++;
            fprintf(stderr,"phase=SNAPSHOT_ACK generation=%llu reconnect=%u rx=%zu tx=%zu\n",
                (unsigned long long)_previousGeneration,_completed,_core.rx.length,_core.tx.length);
            [_manager cancelPeripheralConnection:_peripheral];
        }
    }
}
- (void)centralManagerDidUpdateState:(CBCentralManager *)manager {
    if(manager!=_manager||_finished)return;
    if(manager.state==CBManagerStatePoweredOn)[self scan];
    else if(manager.state==CBManagerStateUnauthorized||manager.state==CBManagerStateUnsupported){[self finish:2];}
}
- (void)centralManager:(CBCentralManager *)manager didDiscoverPeripheral:(CBPeripheral *)peripheral
    advertisementData:(NSDictionary<NSString *,id> *)data RSSI:(NSNumber *)rssi {
    (void)data;(void)rssi;
    if(manager!=_manager||_finished||_connecting||![peripheral.identifier isEqual:_selected])return;
    [_manager stopScan];_peripheral=peripheral;_connecting=YES;_seconds=0;
    [_manager connectPeripheral:peripheral options:nil];
}
- (void)centralManager:(CBCentralManager *)manager didConnectPeripheral:(CBPeripheral *)peripheral {
    if(manager!=_manager||_finished||peripheral!=_peripheral)return;
    _seconds=0;_measured=NO;
    if(_peer==UINT64_MAX || !companion_ble_central_begin(&_core,++_peer,&_runtime,&_generation.backend,NULL)){
        [self finish:2];return;
    }
    _wrapper=[[CompanionCoreBluetooth alloc] initWithCore:&_core peripheral:peripheral];
    if(!_wrapper||![_wrapper configureAuthenticationWithOwner:&_pinOwner explicitEnrollment:_expected!=nil]){
        [self finish:2];return;
    }
    fprintf(stderr,"phase=CONNECTED incarnation=%llu\n",(unsigned long long)_core.link.link_incarnation);
    [_wrapper discoverAuthorizedPeripheral];
}
- (void)disconnected:(CBPeripheral *)peripheral {
    if(_finished||peripheral!=_peripheral)return;
    [_wrapper invalidate];_wrapper=nil;companion_ble_central_teardown(&_core);
    _peripheral=nil;_connecting=NO;
    if(_completed>=_cycles)[self finish:0];else [self scan];
}
- (void)centralManager:(CBCentralManager *)manager didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    (void)error;if(manager==_manager)[self disconnected:peripheral];
}
- (void)centralManager:(CBCentralManager *)manager didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    (void)error;if(manager==_manager)[self disconnected:peripheral];
}
@end
int main(int argc,const char **argv) {
    @autoreleasepool {
        /* Expected fingerprint is supplied BEFORE BLE delegate/manager creation. */
        if(argc<5||argc>6){fprintf(stderr,"usage: r3-acceptance PERIPHERAL_UUID PIN_PATH GENERATION_PATH CYCLES [EXPECTED_24_HEX_FINGERPRINT]\n");return 2;}
        NSUUID *selected=[[NSUUID alloc] initWithUUIDString:[NSString stringWithUTF8String:argv[1]]];
        char *end=NULL;unsigned long cycles=strtoul(argv[4],&end,10);
        NSString *fingerprint=argc==6?[NSString stringWithUTF8String:argv[5]]:nil;
        if(!selected||!cycles||cycles>20||!end||*end)return 2;
        if(fingerprint){char normalized[25];
            if(!r3_fingerprint_normalize(argv[5],normalized))return 2;
            fingerprint=[NSString stringWithUTF8String:normalized];}
        R3AcceptanceOwner *owner=[[R3AcceptanceOwner alloc] initWithPeripheral:selected
            pin:[NSString stringWithUTF8String:argv[2]] generation:[NSString stringWithUTF8String:argv[3]]
            fingerprint:fingerprint cycles:(unsigned)cycles];
        [owner start];dispatch_main();
    }
}
