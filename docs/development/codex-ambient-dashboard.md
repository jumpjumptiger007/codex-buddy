<p align="right">
  <a href="codex-ambient-dashboard.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Codex Ambient Dashboard: vNext Architecture and R0–R7 Roadmap

This is the authoritative architecture and delivery roadmap for the Codex Ambient Dashboard. It supersedes the former Phase 1, Phase 2, U0–U3, and G2.1–G2.5 roadmaps. The source-level evidence and module decisions are in the paired [R0 reuse audit](codex-buddy-reuse-audit.md).

R0 is an architecture, source-audit, licensing, and documentation gate. It does not add production BLE, Codex Hooks, device services, final UI, or character assets. Work stops after R0 review; a separate Control Room decision is required before R1.

## 1. Scope and verified baseline

The current workspace baseline audited for R0 is codex-buddy at commit f12a6e0 on main, with a clean working tree before documentation edits. Donor refs and full commit SHAs are pinned in the audit appendix.

The product target is FoloToy AI Passport: ESP32-C3, 8 MB Flash, no PSRAM, and ESP-IDF 5.5.3. The current BSP, pin header, and hardware guide remain the hardware sources of truth.

Current implementation evidence:

- companion/core contains host-testable lifecycle reduction, quota normalization, message validation and line framing, transport callbacks, and bounded notification deduplication.
- companion/mac contains a persisted-rollout watcher, rollout quota source, reset-state store, Companion orchestration, and interfaces for permission observation, STT, and composer insertion. These are host-side modules and boundaries; they do not establish a production Codex Hook, App Server, Accessibility, or STT adapter.
- The current rollout watcher accepts only the lifecycle form that was verified in its recorded probe. Unsupported or malformed input fails closed. Quota comes from source-provided 300-minute and 10080-minute rate-limit records; token counts are not a quota source.
- main/main.c starts the persistent Passport product shell from the transport-neutral UI model. The main product target does not start the hardware-demo pages. demo_ble.c is a non-connectable demo advertisement, not the production device link.
- Host tests exercise pure logic and fakes. They do not prove a real macOS integration, BLE pairing, RF behavior, or hardware acceptance.

The existing synthetic Passport model still has project and activity text fields. They are not part of the vNext wire or product contract and are scheduled for removal in the product-integration gate. The exact U3 card layout is not frozen. The purple geometric character remains a placeholder; this roadmap does not start the old Character Engine work or define a final mascot.

Earlier environment-probe notes in the former architecture document are historical observations. Recheck them in the gate that needs them; do not treat an old local probe as current platform truth.

## 2. Decision vocabulary and R0 result

Every module decision in the audit matrix is exactly one of:

- **KEEP OURS** — retain the current project implementation or contract as the vNext basis.
- **REUSE DONOR** — incorporate a donor implementation substantially as-is after license, provenance, and integration checks.
- **ADAPT DONOR** — use a donor implementation as a starting point and change it to fit this product's contracts, security, hardware, bounds, or ownership.
- **DROP** — exclude the donor implementation or data from vNext.

The audited matrix contains 21 KEEP OURS, 12 ADAPT DONOR, 8 DROP, and no REUSE DONOR decisions. No donor module was suitable for direct, as-is integration: the useful candidates either carry incompatible content or approval semantics, depend on another semantic core, or need changes for this board and the project's bounded contracts.

The architecture keeps the current Companion truth core, multi-session reducer, outcome and freshness behavior, quota source and reset persistence, notification/snapshot split, transport-neutral UI model, BSP boundary, and fail-closed host interfaces. It adapts a licensed Codex Hook and desktop-bridge path, selected ESP-IDF NimBLE transport and bond-management patterns, small settings/persistence patterns, support tooling, and a simulator test seam. It drops device-side approvals, the Espressif Buddy semantic core and its tx_ready predicate as product authorization, the Codex Buddy security helper as policy, content-bearing Buddy fields, the old project/activity projection, and unproven character or media assets. The audit appendix explains each module decision and its evidence.

## 3. vNext component ownership and data flow

The target data flow is:

Codex lifecycle Hook and verified quota source → Mac Companion adapters → per-session lifecycle and quota truth → bounded, content-free snapshot and one-shot event contract → secure BLE transport → Passport parser/model/presenter/persistent UI.

For voice, a deliberate Passport push-to-talk action starts bounded audio capture → secure BLE transports audio to Mac Companion → local STT returns text → the Companion inserts text only into one verified Codex composer target. The user reviews and submits the Codex turn. No component presses Enter or otherwise submits automatically.

### Mac Companion

The Companion owns volatile Codex and macOS integrations, official Hook installation and lifecycle mapping, multi-session aggregation, source-provided quota acquisition and reset detection, desktop startup/recovery, notification generation, local STT, composer verification/insertion, and BLE connection orchestration.

The Companion is the only source of aggregate lifecycle truth. Session and turn identifiers remain local and map to opaque bounded state. Unknown events, stale sources, unsupported permission signals, and malformed records fail closed. Failed and aborted turns remain distinguishable from successful completion.

### Passport and BSP

Passport owns the display, product interaction, buttons, microphone and speaker hardware access, local settings, bounded BLE parsing and transport events, and small deterministic projection logic. It displays the already-normalized snapshot; it does not aggregate Codex sessions or infer quota.

The BSP remains the owner of board-specific pins, display, buttons, battery and audio interfaces. Product behavior stays in application code. LVGL access outside its task holds bsp_lvgl_lock(). Button callbacks enqueue bounded work and never perform slow storage, networking, audio, or UI operations. Producers stop before their UI objects are destroyed.

### Content-free protocol and security

The vNext semantic protocol is project-owned. Its allowlisted fields are limited to protocol version/capabilities, link and freshness state, enumerated lifecycle/outcome values, bounded aggregate counts, source-provided quota values and reset markers, and enumerated one-shot notice codes. PROJECT and ACTIVITY are excluded. The wire carries no prompts, transcripts, commands, diffs, tool output, assistant content, tool previews, approval requests, authentication material, or secrets.

Persistent snapshot truth is separate from transient notices. A reconnect receives a complete current snapshot before relying on later events. R1 freezes semantic sequence/revision/ACK rules, protocol version/capabilities, field sizes and host admission bounds in [the R1 contract](codex-r1-contracts.md): a maximum95-byte legal state frame, 128-byte line/129-byte frame admission, four notices/516-byte fake-transport queue budget, and eight sessions/sixteen dedup entries. R3 still owns BLE MTU/fragmentation, authenticated generation establishment and physical queue/resource proof; measured limits may tighten but must not silently exceed R1 hard maxima. R5 owns audio framing/codec limits.

R0 selects Option B as the target architecture, not as a proven implementation: adapt Espressif's Apache-2.0 NimBLE transport while keeping the smaller project-owned semantic protocol. The published transport is source-coupled to esp_desktop_buddy through its required core pointer and GATT RX dispatch, so R3 begins with an extraction feasibility proof. RX/TX, GAP/GATT, pairing/bond lifecycle, reconnect, queues, and teardown must work behind the project-owned transport interface without importing Buddy message, entry, prompt, tool, hint, time-command, or permission-reply semantics. If that proof fails or requires retaining the Buddy semantic core, R3 stops for architecture review; it must not silently adopt Option A.

The donor transport makes bonding, MITM, and Secure Connections configurable, while its tx_ready predicate checks connection, subscription, and encryption only. That is transport readiness, not product authorization. R0 does not freeze the exact pairing policy; application data may flow only after a project-owned predicate enforces the policy selected for encryption, bonding, MITM/authentication, Secure Connections, peer identity, and subscription. R3 must test that a link accepted by donor tx_ready is still rejected when it fails the project policy. Donor security settings, UUIDs, MTU assumptions, frame sizes, retry intervals, and RF claims are not current-product facts. The Codex Buddy demo's inspected helper requires encryption, bonding, and a 16-byte key but ignores the authenticated flag; it is DROP as policy evidence.

Passport is never a controller for repository, agent, shell/tool, deployment, or other privileged approvals. A permission-related observation may produce an ATTENTION notice directing the user to act on the Mac. The device cannot grant or deny the operation.

## 4. Resource and failure rules

Keep buffers, queues, retained identifiers, and decoded messages bounded. Reject oversized, malformed, unsupported, stale, replayed, or out-of-order data before it enters product state. Do not raise resource limits based on donor measurements. The ESP32-C3 has no PSRAM; R3 and R4 must measure internal heap, largest contiguous block, task stacks, audio DMA, display DMA, and worst-case frame pressure on the current board.

If the Companion source becomes unavailable or stale, the Passport shows OFFLINE. A failed or aborted turn never becomes DONE. A disconnect clears link-dependent readiness, and a successful secure reconnect triggers full snapshot resynchronization before events are treated as current. Quota is unavailable at its source reset boundary and is never estimated from token totals.

Unpair and reset clear local bond state and any bounded link state through an explicit user action. Logs omit prompts, transcripts, commands, payload content, credentials, and raw authorization data.

## 5. R0–R7 delivery gates

Complete each gate through its own Control Room review. A gate's exit evidence does not authorize the next gate.

### R0 — Reuse Architecture Rebaseline

- **Entry:** current workspace and Git baseline verified; mandatory source set pinned.
- **Scope and donor inputs:** source-level license/provenance audit, module matrix, vNext architecture, and R0–R7 roadmap. Keep current core; adapt only named licensed candidates; exclude the unsafe and unlicensed candidates.
- **Out of scope:** production code, installed hooks, dependencies, firmware changes, device writes, commits, and pushes.
- **Exit evidence:** aligned English/Chinese documents, pinned source evidence, every required module classified, static/document checks pass, and independent Control Room review returns DONE.
- **Stop for human action:** only if access, licensing, or a material product decision cannot be resolved safely inside R0. Stop here after review.

### R1 — Contract and Protocol Convergence

- **Entry:** R0 review is DONE and the Control Room authorizes R1.
- **Scope and donor inputs:** freeze the allowlisted Hook event model, per-session ordering/outcomes, multi-session aggregation inputs, quota/reset semantics, snapshot-versus-notice rules, settings/unpair contract, protocol versioning, and testable interfaces. Adapt only the sanitized Hook mapping pattern from the licensed Codex Buddy source.
- **Out of scope:** user-level Hook installation, production BLE, UI redesign, voice transport, and final character work.
- **Required evidence:** host tests for valid/unknown Hook events, concurrent sessions, replay/order rejection, failed/aborted outcomes, quota windows/resets, content rejection, framing bounds, and version/capability behavior.
- **Stop for human action:** any required change to user Codex configuration, permission, trust, or product-visible approval behavior.
- **Exit evidence:** reviewed contracts, host tests, field/byte/queue limits with rationale, and an approved R2 test plan.

- **Frozen host contract:** [R1 contracts and R2 entry plan](codex-r1-contracts.md) defines the normalized boundary, wire v1, limits, recovery and source-evidence requirements.

### R2 — Companion Production Truth

- **Entry:** R1 contracts and the Control Room authorization for R2.
- **Scope and donor inputs:** implement the production Companion Hook/lifecycle/quota/reset/startup/diagnostic adapters justified by R1. Adapt the licensed desktop bridge and installer patterns while preserving source truth and fail-closed behavior.
- **Out of scope:** Passport BLE and firmware integration, automatic turn submission, device approvals, and unsupported App Server or Accessibility behavior.
- **Required evidence:** real Hook-to-Companion event flow where authorized, multi-session integration, quota/reset persistence, stale/offline recovery, sanitized diagnostics, and host validation.
- **Stop for human action:** macOS privacy/trust dialogs, user-wide Hook configuration consent, credentials, or any unsupported permission boundary.
- **Exit evidence:** independent source inspection, repeatable Companion tests, documented limitations, and a reviewed secure-link input contract for R3.

### R3 — Secure Bridge and Session Layer

R3 iterations 1/2 passed independent review. Iteration 3 implements the approved directional trust model: Passport enforces NimBLE security, while Companion verifies a pinned P-256 Passport identity through fresh pre-R1 application authentication. CoreBluetooth BLE security facts are not claimed. Iteration 3 security architecture and crypto passed independent review; iteration 4 bounded CoreBluetooth scheduling/RX-loss correction passed review. Iteration 5 prepares isolated acceptance endpoints and awaits review. Physical central/peripheral acceptance and runtime resource measurements remain UNVERIFIED. R4 remains unauthorized until R3 review is DONE.

- **Entry:** R2 review is DONE and R1 protocol, content, and security requirements are frozen enough to evaluate a transport.
- **First activity / feasibility gate:** extract the Espressif NimBLE transport behind a project-owned interface. Prove that no esp_desktop_buddy semantic-core dependency or forbidden content/permission semantics remain, and prove an ESP32-C3 / ESP-IDF 5.5.3 build before product integration.
- **Scope and donor inputs:** adapt only GAP/GATT/byte notification mechanics from pinned S5; use project UUIDv5 service/RX/TX identifiers, MTU 23, bounded byte queues, project-owned bonding/peer acceptance, a Companion-owned durable epoch plus host CSPRNG generation and Companion-first HELLO, teardown, and full-snapshot resynchronization. `tx_ready` is never product authorization.
- **Out of scope:** Passport product navigation, final layout, character engine, PTT/STT, and automatic approvals.
- **Required evidence:** ESP-IDF 5.5.3 C3 build, paired central/peripheral interoperability, and independent enforcement of the selected encryption/bonding/MITM-authentication/Secure-Connections/peer policy. Explicitly reject links that meet donor tx_ready but fail product policy. Also test retry/unpair, bounded queues, disconnect/reconnect snapshot recovery, teardown, and measured resource use.
- **Stop for human action:** pairing trust prompts, physical device access, a material protocol/security decision, or unacceptable measured resource/RF behavior. If clean extraction, resource limits, IDF compatibility, or required security enforcement cannot be demonstrated, stop R3 for architecture review; do not adopt the Espressif semantic core as fallback.
- **Exit evidence:** transport/core decoupling, project security-policy enforcement, reconnect/resync, bounded queues, teardown, and measured resource use pass independent review.

[R3 secure bridge evidence](codex-r3-secure-bridge.md) records the exact extraction, GATT policy, implementation boundaries, build/host results, and remaining device-only evidence.

### R4 — Passport Product Integration

- **Entry:** R3 secure session passes review and the protocol is stable.
- **Scope and donor inputs:** bind snapshots and one-shot events to the persistent product UI; implement bounded button/navigation, settings, battery, time presentation if required, sound, persistence, and explicit unpair behavior through current BSP interfaces. Adapt only small validated service patterns.
- **Out of scope:** Codex aggregation on-device, project/activity fields, device approvals, final mascot, and voice capture.
- **Required evidence:** host mapping tests plus current-board display, button callback, LVGL locking, storage corruption/recovery, link-loss, and secure resynchronization acceptance.
- **Stop for human action:** physical hardware operation, product navigation choice that changes scope, or destructive reset of real user data.
- **Exit evidence:** persistent UI behavior matches the reviewed content-free contract and passes device acceptance.

### R5 — Voice Path

- **Entry:** R4 product integration passes review; audio transport and privacy limits are approved.
- **Scope and donor inputs:** implement push-to-talk capture, bounded audio framing/transport, Mac-local STT, verified single-target composer insertion, and clear failure/recovery behavior. Use current BSP audio hardware; evaluate small, licensed audio patterns only where measured.
- **Out of scope:** always-on recording, wake-word service, cloud transcription, transcript display/history on Passport, and automatic Codex submission.
- **Required evidence:** capture and queue bounds, codec or PCM tradeoff, STT availability/latency, interruption and disconnect handling, target verification, and tests proving no submit action exists.
- **Stop for human action:** microphone/Accessibility permission, audio-data consent, or a material codec/privacy choice.
- **Exit evidence:** repeatable end-to-end voice insertion on Mac and device with no automatic submission.

### R6 — Hardening and Supportability

- **Entry:** R4 and R5 reviews are DONE.
- **Scope and donor inputs:** harden sleep/wake, RAM/task/audio resource budgets, startup/autostart, installer/uninstaller, diagnostics, simulator coverage, persistence migration, and recovery. Adapt licensed setup/support patterns and the simulator only for the behavior it actually models.
- **Out of scope:** new product features, new character assets, unbounded telemetry, and release publication.
- **Required evidence:** clean-install and upgrade checks, removal/unpair tests, restart and fault-injection tests, simulator coverage boundaries, sanitized support artifacts, and C3 resource results.
- **Stop for human action:** global/system configuration, signing credentials, destructive migration, or production distribution.
- **Exit evidence:** documented support and recovery procedures with all local automated checks passing.

### R7 — End-to-End Acceptance and Release Readiness

- **Entry:** R6 review is DONE and all acceptance hardware and authorized participants are available.
- **Scope and donor inputs:** validate real-device BLE security/reconnect, lifecycle and multi-session truth, quota/reset, UI/input, voice, sleep/resources, installer/startup, diagnostics, licensing, and simulator-versus-hardware boundaries.
- **Out of scope:** publishing, release, GitHub push, or deployment without explicit authorization.
- **Required evidence:** signed-off test matrix, logs without sensitive content, source/license inventory, reproducible build/package records, hardware results, and documented remaining limitations.
- **Stop for human action:** physical-device operation, account or signing access, publication, release, or any unresolved safety/privacy decision.
- **Exit evidence:** Control Room review confirms the goal's acceptance criteria are met; release actions remain separately authorized.

## 6. Open facts carried to later gates

During the pending R3 review, the following remain open: exact Hook payload/version semantics; production quota source transport; whether App Server quota can be read safely; real central compatibility and paired-device security; physical BLE queue/resource and RF acceptance within R1 hard maxima; audio framing/codec limits in R5; board-level security acceptance; settings migration format; whether the product needs a visible wall clock; STT model/runtime performance; and the exact verified macOS composer-selection mechanism. R3 selects UUID/GATT layout, MTU 23, Passport-side SC+MITM bonding policy, accepted-peer persistence, explicit Passport public-key pinning and pre-R1 authentication, and Companion-owned epoch-backed generation with Passport HELLO binding; see the R3 evidence document.

Each gate must gather the evidence its scope needs. A donor README, successful donor build, or simulator run does not establish current Passport behavior. No arbitrary prompt, tool, or assistant content is approved for the device.

## R2 autonomous evidence

[R2 production truth](codex-r2-companion-truth.md) records bounded source/identity/runtime infrastructure, host integration and the R3 input contract. Current production Hook ordering/outcome mapping and live quota paths remain unavailable where evidence is insufficient; no user Hook installation or device integration has occurred. That report records review status at its writing time; the current reviewed checkpoint marks R2 DONE and authorizes R3.

## R3 autonomous evidence

[R3 secure bridge](codex-r3-secure-bridge.md) records the pinned NimBLE transport extraction, project-owned byte boundary, security and generation policy, ESP32-C3 / ESP-IDF 5.5.3 firmware build, host fault tests, and device-only UNVERIFIED evidence. The implementation keeps R4 UI and voice integration out of scope and awaits independent Control Room review.
