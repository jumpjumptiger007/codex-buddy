<p align="right">
  <a href="codex-r2-companion-truth.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# R2 Companion Production Truth

Baseline: reviewed R1 checkpoint `c9da1728857e17d1f7567db9d3acf7d8bac0524d`. This document records the autonomous R2 implementation and evidence; independent Control Room review is pending. R3 is not authorized. [R1 contracts](codex-r1-contracts.md) remain unchanged.

## Evidence collected on 2026-10-01

| Evidence | Status and scope |
| --- | --- |
| Desktop identity | VERIFIED read-only `/Applications/ChatGPT.app/Contents/Info.plist`: bundle `com.openai.codex`, version `26.928.31416`, build `12553` |
| Desktop CLI | VERIFIED bundled `Contents/Resources/codex-cli/codex-package.json` and `bin/codex --version`: `0.159.2`, aarch64 Apple Darwin |
| Standalone CLI | VERIFIED `codex --version`: `0.155.1`. It differs from the Desktop binary; its schema is not Desktop-version evidence |
| Hook documentation | [Official release reference](https://learn.chatgpt.com/docs/hooks) documents session/turn identifiers, `UserPromptSubmit`, `Interrupt`, `Stop` and permission hooks. It does not establish a per-session ordinal. Prompt submission can be blocked, so it alone does not prove a started turn; Stop cannot establish product success |
| App Server schema | Offline schema generation from both installed binaries succeeds. Desktop `GetAccountRateLimitsResponse.json` SHA256 `cb9655e68130116f634ed9353e45b336492e6069a6628f424d1cd52e54eda0e0`; `TurnCompletedNotification.json` SHA256 `016870158603b0f84bd9f8f65f927161c9fd5128e5ec632087616462dc44e085`. These establish shapes, not live delivery or subscription coverage |
| App Server transport | [Official documentation](https://learn.chatgpt.com/docs/app-server) establishes stdio JSONL and initialize/initialized. No server, authenticated request, credentials or account endpoint was used; production quota adapter remains UNAVAILABLE |
| Current rollout sample | Read-only tails of five recent files, at most 1 MiB each: header version `0.159.2`; 116 records each with source windows 300/10080 and numeric usage/reset fields. Sampled ordinals increased within each file. No accepted `codex.rate_limits` form appeared. This does not establish Hook ordering or a complete production mapping |
| Donor patterns | Reinspected pinned MIT S3 `90972308293076e9b321e90880ccc891a3d7c10b`: instance guard, background bridge, file bridge and health categorization. Its background queue is unbounded and file bridge accepts generic dictionaries. New code uses only lifecycle/ownership concepts; no donor source code, content model, installation or approval mechanism is copied |

Only allowlisted aggregate metadata and generated schema hashes were recorded. No raw source payload, session/turn ID, prompt, transcript, tool content, command, credential or authorization payload was persisted as evidence. The sample contains `task_started`/`task_complete` labels, but label presence is not outcome proof. Existing rollout parsing and quota algorithms are preserved. The current sample cannot feed its quota parser, so the production rollout path remains unavailable for that shape. Rollout observations are not independent cross-source validation.

## Source, identity and ordering boundary

`codex_source_adapter` accepts a finite decoded metadata struct, never JSON/dictionaries/opaque data. DISABLED is unavailable; CURRENT_HOOK is unverified and emits nothing. SYNTHETIC schema1 is an explicit host test profile, never a verified production Hook schema. Unknown versions/facts, zero ordinal/epoch, invalid IDs, missing turns and unsupported source states fail closed, clearing mapped output. No live Hook parser or installation exists while schema/order/outcome delivery remain unverified.

Runtime `source_available` means that the selected adapter is ready to deliver accepted observations, not merely that a profile was selected or that the runtime process started. In this R2 build only SYNTHETIC is ready for host fixtures. CURRENT_HOOK can start the runtime but stays unavailable/unverified in diagnostics; enqueue rejects its inputs and `source_recovered` cannot promote it. A production profile becomes available only after a separately reviewed, evidence-backed adapter/readiness path exists.

The path is source observation → allowlist → `companion_ingestion` → transactional `identifier_registry` → frozen `codex_hook_contract` → CompanionCore. The runtime owns the only dispatcher. All runtime/ingestion/retirement calls require one serialized caller; this is not a thread-safe worker API. The busy guard also rejects synchronous reentry. Source ordinals are preserved unchanged; R2 assigns no ordinals and cannot repair ambiguous producer order. Per-session reducer ordering remains authoritative, including UINT64_MAX exhaustion, independent sessions, wrong-turn terminal rejection and attention preserving the active turn.

Registry uses exact length/byte comparison and monotonic nonzero numeric allocation, without hashing or possible hash aliasing. Session/turn namespaces are separate. Admission is transactional: unsupported facts, rejected terminals, replay and reducer capacity failure cannot consume mapping capacity. On reducer replacement, the evicted identity is retired automatically. Explicit retirement requires an already stale session. Retired identities remain rejected for the whole source epoch; they cannot be silently readmitted after an old stream arrives. Keys and retirement history are never evicted or wrapped.

Restart discards all volatile truth, queued observations and mappings. The injected ownership lease retains the highest source epoch; start requires a strictly greater epoch. An old queued callback fails the epoch check. A real process supervisor must establish a fresh source instance and stop old producers before selecting that epoch; synthetic epoch values do not authenticate live input. There is no durable raw-ID tombstone store.

## Host bounds

| Bound | Maximum and rationale |
| --- | --- |
| IDs | R1 127 bytes per span; exact opaque bytes, no embedded NUL |
| Registry | 16 session identities total: eight reducer slots plus eight retained retirements. 32 turn identities total: four per maximum concurrent session on average. All history is retained; exhaustion rejects admission until a whole-source restart, rather than weakening ordering protection |
| Ingress queue | Eight inline records, each at most336 host bytes; at most2688 bytes. One outstanding observation per maximum concurrent session is a conservative burst budget, not a BLE queue requirement |
| Runtime | Compile-time at most16384 host bytes including inline registry, reducer, queue, wire state and diagnostics. Bounded transactional registry copies also use host stack; none of this moves to ESP32 |
| Diagnostics | Typed state at most96 host bytes, numeric serialized record under128 bytes including local NUL. Counters saturate at UINT32_MAX; invalid state or insufficient output buffer returns failure with zero length |
| Notices | Four pending enum codes before publication, then R1 four-notice wire queue. Overflow drops the new notice and counts it; no content body |
| Semantic wire | R1 unchanged: 95-byte legal snapshot, line128/frame129, four-frame fake FIFO516 bytes, eight sessions, dedup16 |

Registry limits intentionally sacrifice unlimited session/turn throughput for finite replay protection. They are host admission limits, not proven Desktop service capacity or ESP32 resource requirements. Any later increase needs review; R1 semantic/frame hard maxima remain unchanged. No unbounded allocation, queue, retry loop or tombstone growth is introduced.

## Runtime, quota and diagnostics

`companion_runtime` is a project-local service state machine, not a daemon installation. Start/stop are idempotent for the same owner; a second instance using the same injected lease is rejected. Failed source startup invokes stop for partial-start rollback and releases ownership. Crash tests explicitly simulate supervisor lease release, then reconstruct fresh OFFLINE truth from a new epoch. This does not prove an OS process lock, persistent startup or real macOS background execution.

Source loss discards queued observations and clears quota availability through the existing source path. It also clears the bounded R2 pre-publication notice-code buffer: those transient notices belong to the lost source epoch and cannot be rebound to a later OFFLINE snapshot. This does not change persistent snapshot truth or mutate notices already owned by an R1 wire session; their supersession and link behavior remain R1-defined. Lifecycle truth ages into OFFLINE without refreshing last-seen timestamps. Recovery in the same epoch preserves ordinals and retirement history. Stop clears source IDs, reducer slots and link/notice/ACK state. No detached task, login item, LaunchAgent or user configuration is created.

The rollout poll wrapper retains the existing parser and source quota/reset path. Its abort-only lifecycle input cannot establish an active turn and is rejected rather than fabricating a start. Missing files mark source unavailable. Synthetic decoded quota input exercises exact 300/10080 source semantics, 15pp candidate/confirmation, expiry, missing observations and reset persistence through CompanionCore. Stored detector state never makes quota available by itself.

Missing/corrupt/unwritable stores are diagnosed using enums. Failed atomic writes preserve valid in-memory quota, mark durability degraded, and do not manufacture reset confirmation. A subsequent quota/publication call retries the latest detector state once even if source values are unchanged; no background retry loop exists. Successful persistence clears degradation. Successful persisted confirmation prevents duplicate reset notices after restart; failed durability cannot promise restart deduplication. Reset detection remains the existing algorithm.

Diagnostic output contains only saturating counters, source profile/availability, running state, queue/session/turn counts, candidate/confirmed masks, persistence load/write result/degradation, and approved reset markers. There are no string/blob/format-string extensions. Tests inspect the actual serialized record and prove buffer/diagnostic failure leaves product truth intact. Raw identifiers exist only in bounded local registry/queue memory and are cleared on stop.

## Snapshot and notice integration / R3 input

Notifications become only R1 enum codes. Runtime holds at most four codes generated since the previous publication, publishes the full current typed snapshot, then queues those codes against that snapshot. Failed snapshot sends retain pending codes; notices cannot transmit before the current snapshot ACK. Publishing a newer snapshot supersedes prior wire notices as frozen in R1. Dropped/replayed notices never change truth.

R3 must supply a project-authorized transport after its independently tested security predicate, then a fresh nonzero authenticated generation. Connected/subscribed/encrypted or donor `tx_ready` alone is insufficient authorization. `companion_runtime_link_ready` is an injected host seam, not authentication. It negotiates exact R1 v1/caps7; invalid HELLO leaves prior state intact. Link loss clears negotiation, revisions, notices and ACKs. The runtime rejects generation reuse during its lifetime; R3 must also establish uniqueness across process restarts and reject callbacks/frames from older links.

After recovery, a new negotiation and full current snapshot establish truth before notices are trusted. Transport send OK means accepted bounded ownership, not peer receipt; WOULD_BLOCK/disconnect leave revision unchanged. R3 owns bounded scheduling/retry deadlines, teardown/framer reset and backpressure. It must retain the R1 frame maxima, measure ESP32-C3 internal heap/stack and authenticated-link buffers with no PSRAM, MTU/fragmentation, maximum snapshot transfer, RF/reconnect and physical queue pressure. UUIDs, MTU, bonding/MITM/SC policy, pairing UX and physical resource acceptance remain R3 decisions. No BLE or device code is implemented in R2.

## Validation and consent boundary

Dedicated suites cover registry capacity/no-alias/retirement/restart; adapter version/identity rejection; independent ordering, replay, delayed terminals, active attention, success/failure/abort, stale replacement; diagnostic representation/saturation/failure; startup rollback/lease/queue/source loss/recovery; and full fake transport projection/ACK/replay/backpressure/reconnect with reset restart/corruption/atomic-write failure/recovery. Runtime regressions prove CURRENT_HOOK remains unavailable through start, enqueue and recovery while SYNTHETIC remains host-ready. The fake-link regression creates pending ATTENTION and ERROR notices, loses the source, publishes/ACKs the later OFFLINE snapshot, and proves neither notice remains sendable. Every suite is in `tools/validate.sh --static`, alongside the unchanged R1/reducer/quota/persistence tests.

Host behavior is synthetic and does not prove live Hook delivery. Production mappings stay unverified/unavailable until separately consented observations establish current schema, identity, source ordering, explicit outcome and attention clearing. The smallest next live task is a reviewed metadata-only Hook collector/configuration diff and user consent to enable it, followed by approved start/success/failure/abort/concurrent-session/attention scenarios. No raw private content may be persisted or forwarded. App Server credential-backed requests and persistent startup require separate authorization; they are not needed to complete these autonomous host tests. Firmware build and device tests are not run by this host gate. R3, commit and push remain unauthorized.
