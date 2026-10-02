#pragma once
#import <CoreBluetooth/CoreBluetooth.h>
#include "companion_ble_central.h"
#include "companion_pin_store.h"

/* Platform wrapper (validated by compile and portable pump tests). Caller supplies an already connected peripheral
 * and serial delegate queue after explicit OS/device authorization. Core must have
 * begun with NULL platform; construction binds its real writer once. This wrapper
 * must retain this wrapper until invalidate, and keep core alive until then. It
 * creates no CBCentralManager and performs no scan/connect/pairing. */
@interface CompanionCoreBluetooth : NSObject <CBPeripheralDelegate>
- (instancetype)initWithCore:(companion_ble_central_t *)core
                  peripheral:(CBPeripheral *)peripheral;
/* Pin owner must outlive wrapper and share its serial queue. Does not scan or
 * invoke UI; enrollment is allowed only by caller's explicit approval callback. */
- (BOOL)configureAuthenticationWithOwner:(companion_pin_auth_owner_t *)owner
                    explicitEnrollment:(BOOL)enrollment;
/* Same serial delegate queue: call after external bridge publish/pump adds TX.
 * Uses public monotonic/Unix clocks; does not scan/connect or allocate identity. */
- (void)pumpPendingBytes;
- (void)discoverAuthorizedPeripheral;
- (void)invalidate;
@end
