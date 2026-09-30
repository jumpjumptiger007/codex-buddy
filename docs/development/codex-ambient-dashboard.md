<p align="right">
  <a href="codex-ambient-dashboard.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Codex Ambient Dashboard: Architecture and Phase 1 Gates

This document records the frozen architecture and delivery gates for the Codex Ambient Dashboard. Phase 1 covers architecture, real-environment validation, host-testable core logic, and the Mac Companion truth layer. It ends before Passport product firmware or UI implementation.

## Product boundary and platform

The target is ESP32-C3 with 8 MB Flash, no PSRAM, and ESP-IDF 5.5.3.

- **Passport** owns the display and product UI, character animation, buttons, microphone capture, speaker cues, BLE transport, and small deterministic state/reducer logic.
- **Mac Companion** owns Codex integration, quota acquisition and reset detection, lifecycle interpretation, notifications, local speech-to-text (STT), safe text insertion into the verified Codex composer, and BLE reconnect/orchestration.
- Reuse the upstream BSP. Do not change BSP code for application convenience or reuse the baseline hardware-test menu or visual shell as the product UI.
- Passport is a product device, never an approval controller for repository, agent, deployment, or other privileged operations.

Keep state, reducer, protocol, and timing logic that can be tested without hardware independent of ESP-IDF and LVGL.

## State and truth

Codex state is `OFFLINE`, `IDLE`, `WORKING`, `ATTENTION`, or `DONE`. The voice overlay is `NONE`, `LISTENING`, `TRANSCRIBING`, `READY`, or `FAILED`.

- `OFFLINE` means there is no fresh Companion snapshot.
- `DONE` is a brief success indication of about three seconds. A failed or aborted turn must not be celebrated as `DONE`.
- A Companion snapshot is the source of current truth. Notifications are one-shot events and do not replace snapshot state.

Normalize persisted rollout `rate_limits.primary` and `rate_limits.secondary` by `window_minutes` (300 minutes and 10080 minutes), using only source-provided usage and reset data. A window is unavailable at or past its `reset_at` boundary; never estimate quota from token counts. Define an `AppServerQuotaSource` interface, but keep its real adapter unavailable until safe transport and request framing are verified. Persist only bounded reset-detector state with atomic replacement; missing or corrupt state must safely re-baseline.

## BLE, voice, and data boundaries

Use a project-owned 128-bit BLE service. `CONTROL` flows from Mac Companion to Passport; `EVENT` and `AUDIO` flow from Passport to Mac Companion. Control messages use bounded newline-delimited JSON; audio uses a bounded binary format. Exact UUID values, schemas, frame limits, and other wire details remain to be established from measured requirements before implementation.

The user holds OK to talk. Passport captures microphone audio but does not run STT. Mac Companion performs local STT and inserts recognized text into a verified Codex composer. It never presses Enter or otherwise submits the turn automatically. Composer selection and insertion behavior must be based on the Gate 0 Accessibility probe.

Do not send prompts, transcripts, commands, diffs, tool output, assistant content, authentication material, or secrets to Passport. Validate lengths and formats before parsing and bound all payloads, buffers, queues, and retained data. Send only fields needed by the active feature.

Public character engine/code and public example assets must remain distributable. Keep Spider-Man and all other private or non-distributable character sources out of public Git history and releases. Before introducing a private or local character source pack, protect its source path in `.gitignore`; this document does not choose that path. Generated public assets must never silently derive from private or non-distributable sources.

`demo/claude-buddy-port` is a donor and architecture reference only. Its BLE/protocol implementation and measurements are not current-product facts unless revalidated for this product and hardware.

## Gate 0 reviewed environment findings

These are observations of the local environment inspected for this phase; they are not new product invariants.

- **Codex Desktop identity:** the bundle display name was `ChatGPT`, bundle ID `com.openai.codex`, observed version `26.924.22138` (build `11645`), and executable architecture `arm64`. The framework/runtime version was not identifiable from the inspected bundle metadata.
- **Rollout lifecycle and concurrency:** the exact event names `SessionConfigured`, `TurnStarted`, and `TurnComplete` were not observed. A metadata-only follow-up verified 46 persisted `event_msg` records with `payload.type = turn_aborted` across 32 rollout files. Their envelope fields were `type`, `ordinal`, `timestamp`, and `payload`; their payload fields were `type`, `turn_id`, `reason`, `started_at`, `completed_at`, and `duration_ms`. Each inspected file had one `session_meta.payload.session_id`, and ordinals increased within each inspected file. G2.1 may emit only this verified abort form; `thread_settings_applied`, `task_started`, and `task_complete` remain non-equivalent and fail closed. Existing metadata showed task activity overlapping across distinct sessions.
- **Quota:** existing rollout `rate_limits` records contain short and long windows identified by duration (300 and 10080 minutes), with source-provided usage and reset fields. The App Server `account/rateLimits/read` comparison remains unavailable because no safely callable transport or request framing was established; no new request was sent. Rollout data is not cross-source validation, and no token-based estimate was used.
- **Permission and trust:** a metadata-only scan of existing rollout event/type labels found no matching permission, approval, trust, policy, sandbox, `allowed`, or `denied` signals. This is limited to the scanned corpus and does not prove that the application has no other permission behavior.
- **Composer Accessibility:** `NOT_INSPECTED`. The available CUA Accessibility hierarchy call has no metadata-only filter and can return text values, so the composer was not queried. No TCC prompt or setting change was triggered. The composer selector, editability, and insertion behavior remain unverified.
- **STT benchmark:** unavailable in the probed environment because no supported `whisper.cpp` runtime or pre-existing `base`/`small` model pair was found in the checked locations. No inference, download, installation, or user audio was involved; model performance remains unknown.

## Phase 1 host implementation status (G2.1–G2.5)

The current Gate 2 implementation is host-only and reuses the Gate 1 core:

- **G2.1 — RolloutWatcher:** tails bounded persisted rollout records and emits only the verified `turn_aborted` lifecycle form. Unknown or unverified lifecycle labels fail closed; partial, malformed, and oversized input is bounded and rejected safely.
- **G2.2 — Quota source and persistence:** `RolloutQuotaSource` reads persisted `rate_limits` and matches the 300-minute and 10080-minute windows by duration, using only source-provided usage and reset data. Expiry removes the affected window and clears only its reset-detector state. Reset-detector persistence is bounded and atomically replaced; missing or corrupt state re-baselines safely. `AppServerQuotaSource` remains an unavailable interface boundary.
- **G2.3 — CompanionCore:** aggregates host-injected per-session lifecycle events through the existing reducer, with priority `ATTENTION > WORKING > DONE > IDLE`. A fresh Companion snapshot with no fresh Codex session is `IDLE`. Lifecycle and confirmed quota-reset notifications are one-shot and bounded-deduplicated. Snapshot publication is exercised with fake transport only.
- **G2.4 — External boundaries:** `PermissionObserver` reports injected observation status without installing hooks or synthesizing events. STT has an injectable backend boundary with unavailable/failure behavior and no runtime or model. Composer insertion requires one verified opaque target and exposes text insertion only; no real Accessibility selector, retargeting, or submit operation is implemented.
- **G2.5 — Integration validation:** host tests exercise rollout-to-Companion quota/state/snapshot/fake-transport flow, reset persistence across restart, expiry and reappearance, and a synthetic fake-STT result flowing into a verified fake composer. They also verify that unavailable permission and STT results do not create attention or usable transcript input. These tests validate the host abstractions and fakes, not real macOS integrations.

The real App Server transport and request framing remain unavailable. No real permission-request or hook path has been established. The STT runtime and models remain unavailable. The real Codex Accessibility composer remains unverified and has no production insertion adapter. Host interfaces and fakes do not prove these integrations. Passport product firmware, product UI, and physical BLE implementation have not begun.

## Gate sequence

Complete the gates in order. Record evidence and limitations without persisting secrets. Pause when a gate needs a macOS permission dialog, hook trust, system/global installation or configuration change, GitHub origin setup, publication, or physical-device write that requires human action. Do not commit or push unless C2C Control Room explicitly authorizes it.

| Gate | Scope | Exit evidence |
| --- | --- | --- |
| Architecture | Persist this architecture and gate plan in paired English and Simplified Chinese documents. | Both documents agree on frozen decisions, unresolved details, and gate order; repository documentation validation passes. |
| 0 — Environment truth | Probe Codex Desktop identity/runtime/bundle; rollout `SessionConfigured`, `TurnStarted`, `TurnComplete`, and `TurnAborted`; concurrent-session behavior; App Server quota versus rollout fallback; `PermissionRequest` and trust behavior; actual Accessibility composer characteristics; and whisper.cpp `base` versus `small` performance when feasible. | Record observations, evidence source, method, and limitations for each probe. State explicitly when a probe or benchmark is unavailable. Do not persist secrets. |
| 1 — Host-testable core | Implement bounded protocol models and malformed-input tests, a multi-session state reducer, quota normalization/reset detection, freshness logic, notification deduplication, and fake/mock BLE transport. | Host tests cover valid and malformed messages, independent concurrent sessions, stale/fresh snapshots, success versus failed/aborted turns, quota windows/resets, notification deduplication, and bounded transport behavior. Independent C2C review passes. |
| 2.1–2.5 — Mac Companion truth layer | Implement the host-only rollout watcher, quota source and bounded reset persistence, CompanionCore, fail-closed external boundaries, and integration tests listed above. Reuse the Gate 1 reducer, protocol, deduplication, and transport. | Relevant host suites and static validation pass; unavailable real integrations remain explicit; independent C2C review passes. |
| Later phase — Passport product firmware/UI | Implement the product device experience only after this phase has passed review and a separate goal authorizes that work. | This phase does not begin Passport product firmware, UI, flashing, or device writes. |

## Details deliberately left open

This plan does not invent a BLE UUID value, characteristic schema, numeric payload caps, audio encoding, exact freshness thresholds, macOS permission workaround, Accessibility element selector, or STT model/tuning. Resolve implementation details from Gate 0 evidence and bounded product requirements before relying on them. Preserve the ownership boundaries above when doing so.
