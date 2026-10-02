<p align="right">
  <a href="codex-r3-secure-bridge.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# R3 Secure Bridge and Session Layer

Baseline: reviewed R2 `dd873ec53d543f1a41a9f47ddc932d2fbdaf6b49`, main,
ahead 8 / behind 0. Iteration 1 Companion-generation architecture and iteration 2
autonomous implementation passed independent review. Iteration 3 security architecture
and cryptographic implementation passed independent review. Iteration 4 connects
production CoreBluetooth events to the bounded central pump and passed review. Iteration 5 prepares isolated runnable acceptance endpoints. Iteration 6 fingerprint and write instrumentation passed review. Iteration 7 deferred disconnect passed review. Iteration 8 TX timeout/stale-owner correction passed review. Iteration 9 published-owner termination lifecycle passed review. Iteration 10 closes provisional physical connection ownership; its initial DONE was withdrawn after supplementary review. Iteration 11 corrects observed-disconnect retirement; independent review is pending. R3 is not DONE. No R4, commit, push, or physical BLE operation.

## Boundary and donor extraction

`R2 Companion truth → R1 wire → authenticated session → bounded byte transport →
NimBLE/GATT/GAP → Passport`. The pre-R1 authentication multiplexer owns only identity
and handshake records. BLE never interprets Codex lifecycle or quota semantics.
No prompts, transcripts, tool/assistant content, commands, credentials, raw session
identifiers, approvals, or automatic turn submission enter the link.

Espressif `esp-desktop-buddy` S5 commit
`b6bac05db208717676e70180e5269d79f32b2d68`, Apache-2.0: adapted
`components/esp_desktop_buddy_transport_ble/buddy_transport_ble.c`,
`buddy_transport_ble_gap.c`, `buddy_transport_ble_gatt.c`, `buddy_transport_ble_tx.c`,
its public transport header and private state header. Project replacements in
`components/ambient_ble` own API, UUIDs, RX queue, TX ownership, security observations
and lifecycle. LICENSE and provenance NOTICE are retained; pinned donor has no NOTICE.
No donor semantic core, content model, permission model or build dependency remains.
The source boundary test and actual firmware compile check this extraction.

## Directional trust and authorization

Passport authenticates the Mac using the existing NimBLE predicate: connection,
TX subscription, encryption, authenticated MITM, persisted bond, LE Secure Connections,
valid locally accepted central identity, and current nonzero link incarnation.
New peer enrollment requires an explicit local pairing approval delegate; the current
product main supplies none. A bond alone, donor tx_ready, or encryption alone fails.
Accepted central identity persists as a seven-byte project NVS record. Queue/store
failure denies trust. No whole-NVS erase is a recovery action.

Mac authenticates Passport using its explicitly pinned P-256 public key and fresh
application challenge-response. Its revised predicate requires current connection,
expected service/characteristics, notification subscription, nonzero incarnation,
pinned Passport identity, completed application auth, and matching auth incarnation.
`companion_secure_bridge_open` cannot allocate generation or send HELLO otherwise.

CoreBluetooth public APIs do not expose per-link bond/encryption/MITM/SC facts.
`CBPeripheral.identifier` and the opaque peer token are local transport lookup keys;
OS Bluetooth authorization is a privacy permission. None is cryptographic identity.
The Mac predicate no longer claims these missing observations. Application auth proves
Passport application identity; it does **not** prove that macOS used SC/MITM.
Passport independently enforces those BLE properties. The public transcript hash
confirmation is an ordering/binding confirmation, not a second Mac identity proof.
This follows the approved asymmetric authority, with no guessed BLE facts.

## Persistent Passport identity

`ambient_identity` separates bounded lifecycle policy from the ESP crypto/NVS backend.
`ambient_identity_esp` generates P-256 with IDF-supported mbedTLS, exposes only canonical
SEC1 uncompressed public key (65 bytes, `04 || X32 || Y32`) and digest signing.
Private scalar never crosses the public identity API. There is no identity reset API.

Namespace `passport_id` holds `created=1` and a versioned 134-byte `PID1` record:
magic4, scalar32, public65, version1, SHA25632. The creation marker is durably committed
before key creation. A partial marker/record, invalid length/version/checksum/scalar,
or mismatch between derived and retained public key fails closed. An existing corrupt
identity never triggers regeneration. Store creation/write/commit failure fails closed.
Ordinary local UNPAIR uses only `ambient_r3` accepted-peer trust and NimBLE bond storage;
it preserves Passport application identity and unrelated settings.

The singleton ESP factory must run **before BSP, RF or ADC initialization**. It enables
the supported bootloader entropy source only while seeding mbedTLS CTR_DRBG, then disables
it before other peripherals start. Random nonces and ECDSA blinding use that DRBG.
Automatic reseed after the configured finite request budget fails closed because the
entropy source is no longer owned; no weak fallback or unbounded retry. Re-entry requires
safe early-startup entropy ownership (normally reboot). Calls must be serialized.
This is application identity, not hardware attestation. Flash extraction resistance,
secure boot and NVS encryption are not established by this implementation.

## Mac pin persistence and explicit enrollment

`companion_pin_store` takes an explicit caller-supplied path, installs no default user
configuration, and holds a lifetime nonblocking exclusive flock. Owner UID, regular-file
format, no final symlink, and private permissions are required. Its bounded 70-byte
`PIN1` record contains presence byte and the complete canonical public key. A fresh lock
marker permits creation of an empty store; a missing/corrupt existing record fails closed.
Pin replacement writes a private temp file, fsync/F_FULLFSYNC, atomic rename, then parent
fsync. Uncertain durability poisons the owner and denies key access until close/reopen.
Deletion or rollback of the store/lock is outside the persistence guarantee.

First pin requires an explicit caller approval callback before durable persistence.
There is no silent TOFU. Existing pins cannot be overwritten by enrollment or reconnect;
a mismatch requires a separately authorized trust reset/re-enrollment outside this API.
The SHA256-derived 24-hex-character (96-bit) fingerprint is for independent human
comparison only. Full public key signature verification remains the authority.
R4 will provide the final comparison UX; this iteration supplies the callback seam.

## Pre-R1 protocol and ordering

The unchanged GATT byte channel carries bounded binary records only before R1:
header `50 41 01 TYPE LENGTH_BE16` (`PA`, version 1); fixed payload sizes below.
No auth payload is passed to `ambient_wire` and no R1 kind or field changes.

| Type | Payload | Total bytes |
| --- | --- | --- |
| 1 enrollment request | empty | 6 |
| 2 enrollment public key | canonical public65 | 71 |
| 3 challenge | Mac CSPRNG32 | 38 |
| 4 response | Passport CSPRNG nonce32 + ECDSA raw r32/s32 | 102 |
| 5 confirmation | transcript SHA25632 | 38 |

The signed hash is SHA256 of exact ASCII domain (without NUL)
`yliu.tech/Passport-auth/v1:Mac-challenge:Passport-proof:Mac-confirm`, followed by
Mac challenge32 and Passport nonce32. Domain/version and role/direction are bound.
Existing pin → fresh challenge → fresh Passport nonce/signature → pinned-key verification
→ same-transcript confirmation → application auth complete → fresh durable Companion
generation and HELLO → Passport capability negotiation/reply → full snapshot → ACK → notices.
Enrollment adds explicit request/key presentation and approval/persistence before challenge.
Passport permits key presentation only after its BLE policy and explicit enrollment gate.

Host auth completes only after its confirmation is admitted to the bounded ordered TX
queue. Confirmation precedes HELLO in that queue. Passport opens its R1 responder only
on valid current-link confirmation. The byte multiplexer handles a confirmation and HELLO
coalesced within one ATT chunk in this order. Failed/incomplete auth never opens R1 or
changes product truth. Duplicate/out-of-order auth, wrong signatures/nonces, bad lengths,
stale incarnation, security/subscription loss, and disconnect clear auth/session ownership.
Auth records after R1 starts are rejected; no re-authentication within an active R1 session.
Reconnect repeats the protocol with fresh nonces and fresh generation.

## Generation and R1 resynchronization

Companion owns `ambient_generation`: durable epoch32 + host CSPRNG32 salt, strictly
increasing/nonzero even for repeated salt. Read/write/RNG failure or epoch exhaustion
prevents HELLO. Production `companion_generation_store` uses explicit path, exclusive
owner lock, atomic durable replacement and OS randomness. Existing missing/corrupt state
fails closed; deletion/backup rollback is unsupported. Restart tests retain the store.

Passport has no generation allocator or gen_epoch. After application auth it enters
WAIT_HELLO, binds the first legal fresh Companion HELLO, negotiates capabilities and replies
with the same generation. Companion accepts only that reply before unchanged R2 link_ready
and full snapshot publication. Premature ACK/snapshot/notice, mismatched/stale/second HELLO
are rejected. Reply failure consumes the RAM replay marker and requires a fresh session.
Link incarnation and semantic generation remain separate. Snapshot ACK gates notices;
latest snapshot wins, and disconnected changes appear in the next full snapshot.

## Transport, lifecycle, and bounds

| Attribute | Frozen UUID / direction |
| --- | --- |
| Service | `0913bf62-0732-53ce-b7c7-c90db3db3881` |
| RX | `3b9da8eb-4a29-53bb-a219-d4ad4a6e1c11`, central writes; encrypted/authenticated GATT |
| TX | `90127ebc-fe65-5b74-8e8a-41f58a8c27af`, Passport notification; subscription required |

UUIDv5 namespace: `https://yliu.tech/ai-passport/ambient-ble/v1/`. No extra characteristic.
MTU preference 23 → 20-byte values; a 95-byte snapshot uses five central writes, a 129-byte
central frame seven writes, and the 102-byte Passport auth response six notifications. This is byte-stream fragmentation with
no new per-fragment header. R1 limits remain snapshot95, line128, frame129, notices4, sessions8.
Passport RX ring516; TX one slot ≤160 bytes. Mac central RX/TX queues516 each.
Auth staging input102/output102 each; public65/challenge32/nonce32/hash32 are fixed arrays.
These are source/host bounds, not measured NimBLE heap or stack high-water values.
TX task4096; NimBLE host default4096; no PSRAM is assumed.

Central owns serialized discovery/subscription, bounded writes/backpressure, token/incarnation,
auth multiplexer and bridge. A tick does finite work; stale callbacks cannot reopen state.
The CoreBluetooth wrapper uses an already connected authorized peripheral and explicit pin
owner, public Security verification/SecRandomCopyBytes/CommonCrypto SHA256. It creates no
CBCentralManager and performs no scanning/connecting in these gates. Caller manages serial
queue and manager connect/disconnect notifications. Queue acceptance is not an RF delivery ACK.

Iteration 4 drives the same portable pump on auth startup (including configuration
after subscription), subscription-ready, incoming values and the public
`peripheralIsReadyToSendWriteWithoutResponse:` callback. Each invocation performs
at most 64 ticks, each with one write (production chunk20) and at most129 RX bytes.
This covers the bounded queues plus auth/HELLO output at the production chunk size.
WOULD_BLOCK retains TX and returns immediately; write-ready resumes. Stale
peripheral/incarnation callbacks cannot dispatch; The host-tested notification admission entry invalidates
the current link on empty/oversized/full RX; stale notifications cannot tear down a newer link. Caller uses `pumpPendingBytes` on the same serial queue after external
bridge publication creates sendable work. No timer or unbounded retry is installed.
The wrapper uses CLOCK_MONOTONIC milliseconds and Unix time only for R2 freshness;
these clocks never allocate generation or crypto identity. Portable helper tests
prove challenge output, fragmented102 response, confirmation-before-HELLO,
backpressure/resume, stale/teardown rejection and the64-tick cap; wrapper source
wiring checks and actual Objective-C compilation are separate evidence. Neither
proves real CoreBluetooth delegate execution or RF delivery.

Production peripheral failure paths run under a small host fake platform backend, compiling
the actual source: allocation/mutex/queue/NVS/init/GATT/task failures, partial rollback,
stop/deinit timeout/retry, trust commit, unpair/bond removal failure, queue full, mbuf/notify
failure, subscription/security loss, stale worker items and 50 start/stop cycles.
Stop failure retains recoverable ownership; successful stop consumes the pointer. Unpair's
atomic denial latch precedes fallible locks/stores and persists through failure until retry.
Notification failure clears matching link ownership before termination; no infinite retry.

Current uncommitted change set: 94 paths; execution_summary/audit records the exact manifest.

## Validation and evidence

| Evidence class | Status / limit |
| --- | --- |
| Portable host | Auth/parser/lifecycle/identity policy, central/bridge/session/generation/store, ring/boundary, all R1/R2/existing suites; deterministic crypto doubles are explicitly synthetic |
| Production Mac crypto | Public Apple Security P-256 verify, SHA256 and secure RNG; RFC6979 A.2.5 known-good/bad vector, wrong key, real pin files/lock/restart and write/sync/replace faults; warnings-as-errors compile, no Bluetooth execution |
| Production ESP crypto on host | Actual `ambient_identity_esp.c` with pinned IDF mbedTLS library, real P-256 generation/signing and Apple verification, cross-backend auth/fingerprint; NVS and entropy APIs stubbed; not ESP hardware entropy/NVS evidence |
| Firmware | Actual ESP-IDF 5.5.3 / ESP32-C3 build and archive verification; identity/auth/session entry points linked by no-start seam; final measurements recorded in execution_output |
| Device | UNVERIFIED; no scan/connect/permission/pair/flash/erase/unpair/device operation |
| Runtime resources | UNVERIFIED: ATT/security/RF interoperability, internal heap/largest block, mbuf footprint, task high-water, repeated physical reconnect leakage, early entropy/BSP coexistence |

Final iteration-4 firmware: **PASS**, ESP-IDF5.5.3/C3, app967504 bytes, merged1033040 bytes.
Verified archive: `build/firmware/a558e561674db44b23200189c45d1f13a8054901f420802fc8688db76f8da992`.
The linked seam never invokes identity creation, BLE, or device actions.

The published signature vector is [RFC6979 Appendix A.2.5](https://www.rfc-editor.org/rfc/rfc6979#appendix-A.2.5).
Crypto host gate: build the pinned IDF `components/mbedtls/mbedtls` in a temporary CMake
directory with tests/programs disabled, then set `R3_MBEDTLS_SOURCE` and
`R3_MBEDTLS_HOST_BUILD` for `tools/check_r3_crypto.sh`. `tools/check_mac_ble_central.sh`
is compile-only. `./tools/validate.sh --static` includes portable auth and production
peripheral fault suites. Actual firmware uses the repository `--firmware` gate.
No global installation or persistent machine configuration is required.

## R4 input contract and remaining acceptance

R4 consumes the R3 authenticated transport and R1 session. It supplies final local pairing
approval and independent fingerprint comparison/enrollment UX, navigation and rendering;
it does not create Mac BLE transport or weaken auth. Product main remains a no-start link
seam, without final device integration. R4 has not started.

After independent architecture/code review, physical acceptance requires explicit authorization
for C3 flashing, OS Bluetooth permission, secure pairing, reconnect/wrong-peer/unpair tests,
identity persistence across reboot/unpair, pre-R1 auth and fragmentation/backpressure, and
heap/mbuf/stack/reconnect resource measurements. No such authorization or action occurred here.

## Iteration 5 isolated acceptance preparation

[Acceptance endpoints and physical matrix](../../acceptance/README.md) describe
the separate IDF app and explicitly invoked Mac CLI. Normal product startup
remains a no-start symbol seam. `companion_ble_central_begin(NULL platform)` owns
an unbound incarnation, then the wrapper binds the actual CoreBluetooth writer
exactly once before discovery/auth. No dummy writer. Portable tests cover stale,
double bind and unavailable I/O before binding.

Device startup opens the opaque ESP identity before BSP display/ADC buttons and
BLE. Nonblocking prior pairing approval uses an atomic one-shot latch and a
bounded display-event queue. Current security facts gate the bounded acceptance
worker and authenticated session. The test display is separate from product UI.
Mac runner owns manager/filter/selection/connect/disconnect/serialized lifetime;
preconfirmed fingerprint comparison never prompts on the delegate queue.
Cycles1–20, at most cycles+2 attempts and30s deadlines prevent unbounded retries.
R2 source is explicitly disabled, preserving unknown truth for transport testing.

Read-only BLE metrics expose lifetime RX byte/TX slot high-water and public
FreeRTOS task minimum-free stack bytes, serialized against stop. Internal heap,
minimum-ever and largest block are observed in the worker, including disconnected
baseline; mbuf occupancy is UNAVAILABLE. Device reports confirmation received
before HELLO binding. Mac compares bounded SHA256 digests of challenge/nonce
across accepted cycles and rejects reuse, logging only an enumerated result.
These are prepared diagnostics, not claimed runtime measurements.

Host/static isolation, real-facts mapping, metrics/fault suites, Mac executable
warnings-as-errors link, actual normal product and separate IDF5.5.3/C3 builds
are separately recorded in iteration5 execution outputs. No executable is run
by compile gates. Device/security/RF/resources remain UNVERIFIED; no Bluetooth
permission/scan/connect/pair/flash/unpair/erase, R4, commit or push occurred.

Final iteration5 builds PASS: normal product app967568 / merged1033104 bytes,
archive `build/firmware/b47c4269cf0a5fbf6588efa5cfe6e4a4592d5c6eb1ef313eb49c937e2a55e054`.
Separate acceptance app962176 / merged1027712 bytes, retained at
`/private/tmp/codex-r3-i5-acceptance-final`; full SHA256
`700cbfa2e0ece38026d75d4a6febd29b7fa194bb4ec3e52fbc99ebf276acbaad`,
ELF SHA256 `531a038fd3a614e0da13632cef96932116987fa6fd80929ee1fdeab1567094f5`.
Its generated manifest binds all artifacts/configuration. Linked Mac executable
is `/private/tmp/codex-buddy-r3-i5-mac-acceptance` (NOT RUN). Initial acceptance
configure/build failed on its inherited relative partition filename; acceptance
SDK defaults now explicitly resolve the shared root layout, and final fresh gate
passed. No root partition or dependency lock was changed.

## Iteration 6 corrections and evidence

Enrollment accepts exactly 24 ASCII hex characters in either case and canonicalizes
uppercase before exact comparison with the actual Passport fingerprint. Host tests
cover the real fingerprint, lowercase input, mismatch, malformed and oversized input;
the production Apple/ESP crypto gate also checks actual fingerprint normalization.

Forced disconnect retains the exact handle and connection incarnation before clearing
link facts. Its matching disconnect event owns a one-shot advertising restart only
when unpair has completed, no newer incarnation owns the link, and shutdown has not
started. ENOTCONN follows the same path. Failed trust-store cleanup stays fail closed;
retry can consume an already observed disconnect. Production-source fake-backend tests
cover unpair, notification failure, duplicate/stale events, shutdown, ENOTCONN and retry.
Advertising recovery never restores authorization or an old authenticated session.

The central byte boundary measures isolated whole-frame length and successful platform
write chunks, preserving WOULD_BLOCK without counting it. Coalesced queued frames are
not misreported as one measured frame. The runner requires the first acknowledged
snapshot to measure 95 bytes / five writes at a 20-byte payload. These counts mean
CoreBluetooth write acceptance; Passport's subsequent R1 ACK is separate protocol
evidence. AUTH_OPEN logs actual ATT MTU. RF delivery and device observations remain
UNVERIFIED until explicitly authorized physical acceptance.

Iteration 6 validation: full static/R1/R2/R3 host suites, CoreBluetooth object compile,
Mac acceptance executable warnings-as-errors link (NOT RUN), and production Apple/ESP
crypto gate PASS. Actual ESP-IDF 5.5.3 / ESP32-C3 normal product build PASS:
app 967968 bytes, merged 1033504 bytes, archive
`build/firmware/8c777691518457a24d938dde164894f85c7101f84fec64b8d7eb90f5e2a5af08`.
Fresh independent acceptance build PASS at `/private/tmp/codex-r3-i6-acceptance`:
app 962624 bytes, merged 1028160 bytes, merged SHA256
`9b2cd12015f095d3d8846614f8e88f75b18c8ec8ab93338c494aa98bb3b92eb6`.
Historical iteration 5 artifacts above remain historical evidence.

Device tests and runtime resource measurements: UNVERIFIED; NimBLE mbuf measurement:
UNAVAILABLE. No scan/connect, permission, pairing, flash, erase, real unpair, R4,
commit or push. R3 is EXECUTED pending independent review, not DONE.

## Iteration 7 deferred disconnect ownership

A state-mutex timeout no longer consumes an unrecorded disconnect. A single fixed
pending slot retains handle and published 64-bit link incarnation under a short C3
FreeRTOS critical section (numeric publication only, no allocation or NimBLE calls).
A safely published event may be acknowledged by the GAP callback even when immediate
cleanup fails. The existing TX worker makes one deferred processing attempt per loop,
with its existing 100ms idle wait and a maximum 20ms state-lock attempt. This adds no
queue capacity, task, busy retry loop, or R1 content interpretation.

While pending, link facts deny peer acceptance, send/read and GATT ingress reject
product bytes, and peer approval persistence/worker commit is denied. Under the state
lock, matching cleanup resets RX and drains at most the existing one TX slot. Forced
ownership survives unpair and ENOTCONN; unpair retries also recover a disconnect that
arrived before the initial failed unpair lock could establish its marker. A newer
published disconnect supersedes a stale slot; processing an older incarnation cannot
clear a newer link. Shutdown prevents restart. Advertising restart and ownership
consumption occur in the same acquired state-lock transaction, eliminating a second
fallible lock that could strand the event. A critical-section publication during
processing retains the pending latch for the next worker pass.

Production-source fake API tests cover ordinary/unpair/notify disconnect first-lock
failure, failed forced-resume lock, ENOTCONN with both immediate/resume lock failures,
exactly-once restart and duplicates, stale pending/newer links and newer disconnect
supersession, shutdown, and send/read denial before worker cleanup. Host fake critical
sections are serialized no-ops; real C3 scheduling/critical-section behavior remains
Device UNVERIFIED. No new physical authorization is implied.

Iteration 7 Host validation: full static/R1/R2/R3 production BLE lifecycle/fault suites,
Apple/ESP crypto gate, CoreBluetooth object compile and Mac acceptance warnings-as-errors
LINK-only gate PASS. Device endpoint remains NOT RUN. Final fresh IDF5.5.3/C3 acceptance app
963088 bytes / merged1028624 bytes, retained `/private/tmp/codex-r3-i7-acceptance-final`; merged
SHA256 `b590f1818a58437a5ec6cea9c610598f3882ffe5d9641db2c39fd2b58b7a37fc`.
Physical interoperability/security and runtime resources remain UNVERIFIED.

Final iteration 7 normal product IDF5.5.3/C3 build PASS: app968368 bytes, merged1033904 bytes; archive `build/firmware/7823d0ae0ace85a093873dae6693435f4dec6c02434e1e0dc6ca0558756de137`. Normal product still does not start R3.

## Iteration 8 TX uncertainty and recovery

The production TX snapshot now preserves ESP_ERR_TIMEOUT instead of classifying it
as proven stale ESP_ERR_INVALID_STATE. A queued frame whose ownership cannot be
established is not silently discarded on an otherwise live authorized link: its
handle/incarnation requests fail-closed lifecycle recovery through the existing fixed
slot. The short critical section compares the item's exact incarnation to the current
published owner before setting denial; an older item cannot request termination of a
newer connection, even when the state mutex is unavailable.

The same slot distinguishes a requested forced termination from an observed disconnect.
The worker establishes forced ownership, resets authorization/RX/TX, and requests
termination once after acquiring the state mutex. Successful termination awaits the
matching disconnect; ENOTCONN records completion in the same transaction and restores
advertising once. An actual disconnect supersedes a queued termination request.
Generic TX recovery never sets or clears the explicit local-unpair latch. Other
termination errors retain fail-closed ownership until a real disconnect or explicit
local lifecycle retry; they do not trigger an unbounded termination retry loop.

Tests against actual production sources cover notify failure plus recovery-lock
failure, success/later callback and ENOTCONN, a current queued frame's snapshot-lock
timeout, stale queued timeout against a newer incarnation, send/read/GATT/trust-store
denial while pending, duplicate requests/callbacks and all iteration 7 regressions.
Two intermediate host assertions were corrected during implementation (new-owner
pending ownership and the fake worker's next-loop retry); final full static gate PASS.
No queue/task/allocation increase or security/R1 policy change. Device/runtime remain
UNVERIFIED; Mac acceptance is compile/link ONLY. No hardware, permission, R4, commit/push.

Iteration 8 final validation: full static/R1/R2/R3/BLE faults and Apple/ESP crypto PASS;
CoreBluetooth compile and Mac acceptance warnings-as-errors LINK PASS (NOT RUN).
Normal product IDF5.5.3/C3 build PASS: app968544 / merged1034080 bytes; archive
`build/firmware/31bdac1ec8fe159fdb220183c352db2fba22b3bb1a95d87eedc6d0e6fb6a6c95`.
Fresh isolated acceptance build PASS: app963280 / merged1028816 bytes at
`/private/tmp/codex-r3-i8-acceptance`; merged SHA256
`065cb48a0fb95434603319791f52c573e1a632154d872f36ec50c579c09a1806`.
Device/runtime UNVERIFIED; mbuf UNAVAILABLE. R3 awaits independent review, not DONE.

## Iteration 9 bounded termination lifecycle (current)

All project-initiated rejection/recovery of a published current connection uses the
same fixed owner slot: TX uncertainty/notify failure, security initiation failure,
failed/insufficient ENC_CHANGE (including observation timeout), missing local peer
approval, rejected passkey or injection failure, repeat pairing denial, and trust
queue-full rejection. Explicit local unpair and shutdown remain separate. The legacy
direct termination at incarnation exhaustion rejects an unpublished connection; it
cannot authorize a frame or allocate a wrapped incarnation.

The termination budget is exactly **3 attempts total per owner**, with **at least
100ms between attempts**, measured by wrap-safe FreeRTOS tick subtraction. Only the
existing TX worker performs retries; initial dispatch may make the first attempt.
Duplicate requests cannot reset or accelerate the budget. Return0 waits for a matching
disconnect; ENOTCONN completes immediately through the existing one-shot cleanup.
Other errors retain denial and finite worker retry ownership. A matching disconnect
cancels retries even after exhaustion; a newer incarnation invalidates old retries;
shutdown suppresses both retry and advertising.

After the third error, retries stop and product traffic remains denied. The component
emits the content-free enum `terminal lifecycle fault=TERMINATE` and exposes current
`termination_attempts` / `termination_fault` through its read-only metrics API; the
acceptance worker logs those numeric fields. It does not silently restore authorization,
rotate identity, erase bonds or reboot. Explicit component stop/re-entry remains the
terminal recovery boundary; physical use requires separate authorization. This replaces
the iteration 8 single-attempt error handling described above.

Production-source fault tests cover error→success/callback, error→ENOTCONN, exactly-three
exhaustion, no repeat attempts/reset from duplicate requests, real disconnect between
attempts, stale owner/new connection, shutdown, tick rollover, each security/pairing/
trust-queue rejection path, and all iteration 7/8 regressions. Initial legacy fixtures
were corrected to reconnect after newly fail-closed pairing rejection before testing
subsequent operations; no security predicate was weakened. Device behavior and runtime
resources remain UNVERIFIED; no scan/connect/permission/flash/pair/unpair/R4/commit/push.

Iteration 9 final gates PASS: full static/R1/R2/R3/BLE lifecycle faults; actual Apple/ESP
crypto host gate; CoreBluetooth compile and Mac acceptance warnings-as-errors LINK only;
check_repo and diff-check. Normal IDF5.5.3/C3 product app969056 / merged1034592 bytes;
archive `build/firmware/cb4f2e27bf318eca1a21649c57618ebb6e42be221ccdb43fe44276ebf42a29bc`.
Fresh separate acceptance app963840 / merged1029376 bytes at
`/private/tmp/codex-r3-i9-acceptance`; merged SHA256
`1574fcd0d03ce5456df083337cdbacedc0bc817386fcb03433c2d2bf48a32e91`.
Device/runtime UNVERIFIED, mbuf UNAVAILABLE. Independent review pending; not R3 DONE.

## Iteration 10 provisional physical ownership (current)

A successful NimBLE CONNECT first records one fixed provisional handle and a monotonic
lifetime physical-admission serial under the short critical section, before taking the
fallible state mutex. This is cleanup ownership only: it supplies no authenticated link
incarnation, app-auth result, R1 generation or accepted bytes. It is not persisted or
transmitted. Neither the admission serial nor the normal link-incarnation allocator
wraps; exhaustion rejects admission instead of creating a session.

Link publication now returns an explicit result. Security initiation runs only after
successful publication. State-lock timeout or UINT64_MAX incarnation exhaustion rejects
the provisional owner through the same 3-attempt / >=100ms worker termination policy.
A matching provisional disconnect or ENOTCONN clears it and restarts advertising once;
errors retain finite retry ownership; exhaustion exposes the existing TERMINATE fault.
A newer published owner invalidates stale provisional work. One fixed retired numeric
marker rejects a delayed old provisional callback, including controller-handle reuse;
no queued bytes or authority are retained there. Shutdown suppresses retry/restart.
NimBLE GAP callback ordering and actual hardware scheduling remain Device UNVERIFIED.

Every published CONNECT allocates a fresh link incarnation, even if a controller handle
is reused before old project facts are cleared. RX, the one TX slot and pending pairing
facts are cleared inside the publication state lock. Failed publication never starts
security or authenticates/R1. Neighboring subscription/ENC/MTU/identity observation
updates also reject the current owner when ownership cannot be established, rather
than retaining stale authorization observations. GAP now contains only the unified
bounded termination call; explicit stop/unpair stay separate.

Production-source tests cover CONNECT's first state-lock failure, success/later callback,
error/retry, ENOTCONN, three-error terminal fault, disconnect between attempts, normal
incarnation exhaustion without wrap, physical-admission serial exhaustion, stale
provisional callback and deferred work against a newer published link, reused handle,
no security initiation on failed publication, shutdown, observation-lock failures, and
all iteration 7/8/9 regressions. One intermediate fixture assertion was corrected to
check internal trust-store denial only while recovery is pending; post-ENOTCONN is
already disconnected and cannot admit product bytes. Security predicates were unchanged.
Device/runtime remain UNVERIFIED; mbuf UNAVAILABLE. No hardware/R4/commit/push.

Iteration 10 final gates PASS: full static/R1/R2/R3/BLE production fault suites;
Apple/ESP actual crypto host gate; CoreBluetooth compile; Mac acceptance warnings-as-errors
LINK only; repository/diff audit. Product IDF5.5.3/C3 app970016 / merged1035552 bytes;
archive `build/firmware/2397969791ddb2b84da481f98cf531cd18234f65ca3757d0b0721ec8f338731e`.
Fresh isolated acceptance app964800 / merged1030336 bytes retained at
`/private/tmp/codex-r3-i10-acceptance`; merged SHA256
`399c9b73c94aca3d04345c0ab002dc515cb2826df7e05db9bf9587b876fe0537`.
Device/runtime UNVERIFIED; mbuf UNAVAILABLE. Independent review pending; not R3 DONE.

## Iteration 11 — observed provisional disconnect retirement

The supplementary independent review withdrew iteration 10 DONE after confirming an
omitted callback ordering. A provisional DISCONNECT can be recorded but its state-lock
processing can fail; a newer same-handle CONNECT can publish before worker cleanup.
The fixed retired marker now clears, under the existing critical section, when consuming
an observed provisional slot with the exact retired handle and serial. A pending
termination request does not clear it: the delayed old physical callback is still
outstanding. No allocation, queue/task growth, security-policy or wire changes.

The new production-source regression failed before the correction and passed after it:
observed old disconnect → lock failure → same-handle new publication → stale observed
slot consumed → matching tombstone cleared → genuine current disconnect clears truth
and restarts advertising exactly once. Its complementary unobserved termination case
retains the tombstone, ignores the first delayed old callback, then processes the genuine
current callback normally. Duplicate callbacks do not restart advertising again; all
previous shutdown, stale-owner, bounded termination and iteration 7–10 tests remain.

Independent iteration 11 review is pending. Device interoperability and runtime resource
measurements remain UNVERIFIED; NimBLE mbuf accessor remains UNAVAILABLE. No physical
actions, Bluetooth runner execution, R4, commit or push.

Iteration 11 final validation: static/R1/R2/R3/production BLE faults, real Apple/ESP
crypto, CoreBluetooth compile, Mac warnings-as-errors LINK-only and repository/diff
audit PASS. Product actual IDF5.5.3/C3 app970080 / merged1035616 bytes, archive
`build/firmware/903ce368779b3e27417dcde12a43edee57189d96905a67c1109b71e913d23f6c`.
Fresh acceptance app964848 / merged1030384 bytes, retained at
`/private/tmp/codex-r3-i11-acceptance`, merged SHA256
`d77b148d779af9b3d5a4341bca56b26f4c6e7347ee7880ba54bc6b56b29d0e72`.
Independent review pending; physical acceptance remains UNVERIFIED.
