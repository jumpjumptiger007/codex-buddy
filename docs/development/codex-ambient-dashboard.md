<p align="right">
  <a href="codex-ambient-dashboard.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Codex Ambient Dashboard: Architecture and Delivery Gates

This document records the architecture and delivery gates for the Codex Ambient Dashboard. Phase 1 covers architecture, real-environment validation, host-testable core logic, and the Mac Companion truth layer. Phase 2 U0/U1 establishes the Passport application-model contract and host-testable mapping; U2 adds the first persistent LVGL product shell, and U3 fills that shell with bounded status and quota presentation without transport or interaction behavior.

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

## Phase 2 Passport UI contract (U0/U1)

`main/passport_ui_model.*` maps normalized synthetic/mock input into a fixed-size Passport view model. It has no ESP-IDF, LVGL, BLE, or heap dependency. Project text is limited to 47 bytes and activity text to 95 bytes; an over-limit field is rejected in full and reported as rejected. This input is not a BLE payload or wire schema.

The future product UI uses one persistent root view backed by the latest Companion snapshot. Companion supplies the already-normalized lifecycle; Passport does not aggregate Codex sessions. Lifecycle and voice overlay remain separate; listening can change the character presentation without changing lifecycle truth, and attention retains character priority over listening. Explicit failed or aborted outcomes cannot map to `DONE`. Each quota window has independent available/unavailable state for 300 and 10080 minutes; the model copies only source-provided used/remaining percentages and reset time/marker. It never derives quota from token counts. Attention, completion, reliable error, quota reset, and generic connection-loss notices are separate one-shot presentation facts. The caller controls their display duration; the model owns no timer or task.

The exact BLE implementation details remain open as listed below. The U0/U1 application model is transport-neutral and does not define or depend on those wire details. U2 must re-read the current display/LVGL/BSP implementation and relevant tests before implementing the 240 × 320 product layout.

### Verified current display and input constraints for U2

These facts were checked against `bsp_display.h`, `bsp_display_lvgl.c`, `bsp_pins.h`, `bsp_button.h`, their BSP implementations, `test_bsp_display_rounding.c`, `test_bsp_lvgl_init.c`, `test_bsp_button.c`, `sdkconfig.defaults`, and the display/button sections of the hardware guide. Current code and tests take precedence over older notes.

- The current BSP configures an ST7789P3 panel at logical 240 × 320 portrait, RGB565, on SPI2 using MOSI-only, 80 MHz, mode 0. Color inversion is enabled, reset is software-only, and panel gap is `(0, 0)`. The LVGL registration keeps `swap_xy`, `mirror_x`, and `mirror_y` false and swaps RGB565 bytes for SPI order.
- The final RGB565 flush masks a global 30 px rounded rectangle; pixels outside its visible row spans are cleared to black. Keep persistent content out of the masked pixels. No larger safe inset is specified. The AI guide places battery SOC in the top-right by default, omits it when `bsp_battery_soc()` returns `-1`, and says not to overlap application content.
- The current LVGL registration uses one DMA buffer for 240 × 40 RGB565 pixels (19,200 bytes, about 19.2 kB) with `double_buffer=false`. The ESP32-C3 has no PSRAM; `sdkconfig.defaults` selects 16-bit LVGL color and a separate 24 KB LVGL pool. Review internal RAM, the largest contiguous block, and I2S DMA before adding buffers or large assets.
- The BSP exposes UP, DOWN, and OK from one ADC ladder, with `PRESS`, `CLICK`, `DOUBLE`, and `LONG` events and 180 ms short/500 ms long timing. A separate hardware power button is not one of those BSP controls. Button callbacks run in the shared `esp_timer` task and must enqueue bounded work; they must not access LVGL. These facts do not assign product actions to the buttons.
- The current implementation and host tests are authoritative if older guide text differs. U2 must re-check these files and tests before layout work; this inventory does not define exact text safe areas, fonts, or rendering behavior beyond the code above.

## Phase 2 Passport product shell (U2)

U2 creates one persistent product root with 24 pre-created LVGL objects, within the planned 20–30 object inventory. Startup initializes the display and LVGL, enables the backlight, maps the deterministic offline model view, and creates the product shell while holding the LVGL lock. The product component builds only the application model and shell; hardware-demo sources remain available as references but are not part of product startup.

The shell presents lifecycle text, a static character face, project and activity fields, two quota-availability placeholders, and hidden reusable voice and notice panels. Updates change labels on the existing objects. They do not recreate screens or allocate UI objects. This gate does not add Companion/BLE binding, button actions, microphone capture, character animation, notices, or quota percentage/reset rendering. LVGL calls from outside LVGL context require `bsp_lvgl_lock()`.

## Phase 2 Passport persistent information UI (U3)

U3 keeps the U2 root and character face, then presents the 300-minute and 10080-minute quota windows before lifecycle, project, and activity information. Each quota window has an explicit `NO DATA` or `AVAILABLE` state. When available, only source-provided used and/or remaining percentages are shown, rounded to whole percentages for display. When both are present, the compact pair is ordered as used/remaining, as marked by the card's `U/R%` caption; the worst-case `100/100` line is about 52.6 px in Montserrat 14 and fits the 82 px value label. The presenter never derives a missing percentage, uses token counts, or displays reset time. Out-of-range or non-finite source values suppress numeric details while preserving availability; if detailed text does not fit, the explicit availability label remains.

The persistent shell has 25 LVGL objects, including one `HOLD OK TO TALK` hint. It retains bounded project/activity and quota strings in two fixed banks so each update reuses the existing root and objects. Empty, rejected, or unterminated project/activity fields render as `--`; long valid fields use LVGL's fixed-size dots mode. `OFFLINE`, `IDLE`, `WORKING`, `ATTENTION`, and `DONE` have distinct status text and status-panel colors. The hidden voice and notice panels remain placeholders. The PTT text is only a hint; U3 does not connect OK input, voice capture, BLE, Companion data, notice behavior, or U4/U5/U6 features. Calls to LVGL outside its task still require `bsp_lvgl_lock()`.

## BLE, voice, and data boundaries

Use a project-owned 128-bit BLE service. `CONTROL` flows from Mac Companion to Passport; `EVENT` and `AUDIO` flow from Passport to Mac Companion. Control messages use bounded newline-delimited JSON; audio uses a bounded binary format. Exact UUID values, characteristic/schema details, numeric payload/frame limits, and exact audio encoding remain to be established from measured requirements before implementation.

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
