<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# R3 physical acceptance endpoints

Iteration 5 prepares these endpoints; it does **not** authorize their execution.
Normal product `main/app_main()` continues into the product UI and only retains
R3 symbol references. The separate `r3_device` IDF app has its own `app_main` and
reuses project components through `EXTRA_COMPONENT_DIRS`; no product compile macro.
The `r3_mac` executable is test-only. CI/build gates never execute it or flash.

## Build only

Activate the repository-supported temporary ESP-IDF 5.5.3 environment. Run the
normal `./tools/validate.sh --static` and `./tools/validate.sh --firmware` gates.
Then provide a **fresh** explicit temporary build directory as
`R3_ACCEPTANCE_DEVICE_BUILD` to `./tools/check_r3_acceptance_device.sh`. This builds
ESP32-C3/8MB/no-PSRAM, merges and verifies the shared default partition layout,
and records SHA256/size for image, ELF, MAP, sdkconfig and component images in
`r3-acceptance-manifest.json`. It does not flash. Main task stack is8192 bytes;
LVGL is pinned9.5.0 to match the normal target. The normal product build/archive
and acceptance build/artifact identities must be reported separately.

`./tools/check_r3_acceptance_mac.sh` compiles and **links** the entire test
executable with Apple frameworks and warnings-as-errors; it never runs it.
An explicit `R3_ACCEPTANCE_MAC_OUTPUT` preserves the executable at that path.
No machine installation, login item or persistent service is added.

## Behavior after explicit physical authorization

Device identity opens before any BSP/RF/ADC/audio operation. The acceptance app
then initializes its own simple fingerprint/passkey label and BSP buttons, and
starts NimBLE. It does not use the product navigation or demo screens. OK click
arms one pairing for10 seconds; the nonblocking NimBLE callback atomically
consumes that prior approval and enqueues a display event (depth4, zero wait).
Queue failure denies pairing. Display/storage/session/crypto work runs in the
worker, never in BLE/button callbacks. UP long invokes ordinary local UNPAIR
only when the operator is explicitly authorized; identity is retained.

The worker refreshes actual link facts before RX dispatch. All Passport BLE
policy fields are required; app auth adds no invented BLE facts. Each50ms wake
handles at most4 UI events and4 bounded129-byte RX reads, then one output/ACK
flush. Authentication/full-snapshot timeout10s fails closed until a new link;
no infinite handshake retry. Repeated resource reports include disconnected
baseline. Crypto identity stays open throughout BSP/BLE coexistence; early
entropy is never reopened after ADC/RF initialization. DRBG exhaustion fails
closed, requiring an explicitly authorized reboot, not weak reseeding.

Mac CLI shape (do **not** run until authorized):

```text
r3-acceptance PERIPHERAL_UUID PIN_PATH GENERATION_PATH CYCLES [EXPECTED_24_HEX_FINGERPRINT]
```

The UUID selects exactly one CoreBluetooth peripheral, whose identity must be
established through an authorized discovery step if not known beforehand. Paths
must be explicit with existing private parents; retain the pin and generation
files and their lock files across runs. Do not delete/reset them to retry.
For first enrollment compare the displayed/serial public fingerprint independently
and supply it before starting the executable. The delegate approval only compares
that preconfirmed value; it never prompts, waits for input or silently pins.
Existing pins cannot be replaced, even with a new CLI fingerprint.

The runner owns a serial CBCentralManager queue, frozen service-filter scan,
selected-peripheral connect/disconnect, real transport bind after unbound core
begin, wrapper lifetime, and timer/signal shutdown. Cycles are1–20, attempts at
most cycles+2, each phase deadline30s. It waits for snapshot ACK, records increasing
generation, disconnects and securely reconnects for each requested cycle.
R2 uses `CODEX_SOURCE_DISABLED`: status/quota are truthfully unknown, not fabricated
live Codex data. Timer pump provides bounded retry; no prompts/transcripts/tools
or raw session/turn IDs are transmitted. SIGINT closes ownership and stores.

## Prepared physical matrix (NOT RUN)

| Action / observation | Required evidence |
| --- | --- |
| Authorize exact C3 image, compatible flash/NVS policy and selected device | Image/ELF/MAP hashes; flash success alone is not acceptance. Preserve identity/bonds; no erase implied |
| Boot separate app | Identity/fingerprint before BSP then BLE; no entropy/RF overlap failure |
| First pairing, prior OK arm | Passkey label, SC/MITM/encryption/bond/accepted-peer observations; expired/no arm denied |
| Explicit fingerprint enrollment | Match accepted once; wrong fingerprint fails; no silent TOFU |
| Application auth ordering | Fresh Mac challenge and Passport nonce per connection; confirmation before R1 HELLO; neither clock supplies uniqueness |
| Full handshake/snapshot ACK | Auth complete → HELLO reply → full current snapshot → ACK → notices allowed |
| Reconnect, restart, bond persistence | Fresh app-auth nonces, increasing Companion durable generation and full snapshot resync; no old session reuse; stable Passport identity/pin |
| Failure/security/subscription loss | No product truth mutation; wrong peer/key, replay, interrupted pairing, lost subscription and malformed/incomplete frames fail closed |
| State changes while disconnected | Full latest snapshot after reconnect; notices never precede snapshot/ACK (host fault suites provide synthetic coverage; live source integration requires its own authorized producer) |
| Authorized local unpair/retry | Removes peer trust/bond, preserves application identity; never factory reset or Codex action |
| Repeated cycles and teardown | Internal free/minimum/largest block; worker/TX/host stack minimum-free bytes; lifetime RX byte/TX slot high water; no growth/leak trend |
| ATT/RF behavior | Negotiated MTU,95-byte fragmentation, notify/backpressure/loss; observed RF delivery and ordering |

The Mac runner retains only fixed SHA256 digests of the last accepted challenge
and nonce, rejects either digest repeating on the next accepted reconnect, and
logs an enumerated fresh-nonce result without raw bytes/digests. Device per-byte
auth dispatch logs confirmation receipt while R1 generation is still zero, then
logs later HELLO generation, providing the required physical ordering observation
when executed. These compiled diagnostics are not yet physical evidence.
BLE metrics use public FreeRTOS APIs, expose no private NimBLE structures and
require ownership serialized against stop. Stack units are ESP-IDF bytes;
NimBLE mbuf occupancy is **UNAVAILABLE**, not zero. Runtime heap/stack/queue/RF,
identity NVS persistence, pairing and interoperability all remain **UNVERIFIED**
until this matrix is authorized and performed. Diagnostics contain phases,
public fingerprint, generation/cycle/resource numbers and enumerated failures.

R4 receives the completed secure transport; these test endpoints add no R4 UX.
No hardware, Bluetooth permission, scan/connect/pair/unpair/erase, commit or push
is performed by iteration5 compile gates.

Acceptance-only controls after explicit authorization: DOWN click sends one
bounded129-byte deliberately malformed frame after current app auth, for maximum
transport pressure/rejection observation; it contains only zeros and a terminator.
DOWN long closes auth and stops BLE, logs post-teardown heap/minimum/largest/worker
stack, and retains the worker for BSP callback lifetime. Stop failure retains
owner and another DOWN long retries once; successful stop requires reboot to
re-enter. No automatic retry or entropy reopening. Static validation also links
the Mac executable (Darwin) and never runs it.

The CLI embeds a Bluetooth purpose string in its Mach-O `__info_plist` section
([Apple usage-description documentation](https://developer.apple.com/documentation/bundleresources/information-property-list/nsbluetoothalwaysusagedescription)).
Link/section inspection does not request permission; actual OS consent and CLI
permission attribution remain physical acceptance facts, not compile evidence.

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
