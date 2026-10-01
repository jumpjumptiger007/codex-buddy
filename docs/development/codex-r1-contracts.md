<p align="right">
  <a href="codex-r1-contracts.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# R1 host contracts and R2 entry plan

This contract implements the R1 gate of [the authoritative architecture](codex-ambient-dashboard.md), starting from R0 checkpoint `4784877fd707477b86dec746f127db0de3c02281`. It specifies synthetic, transport-neutral boundaries. Production Codex Hook schemas, permissions, quota acquisition, authenticated transport, firmware memory use, and device behavior remain unverified. R2 requires separate Control Room authorization.

## Normalized Hook input

`companion/mac/codex_hook_contract.h` accepts already-decoded metadata. Its enum names are **normalized facts**, not claims about current Codex Hook event names or fields. R2 must establish a verified source adapter before selecting `VERIFIED`; unavailable and unverified sources produce no event. A source adapter must not treat absence of an error, a tool completion, `Stop`, or a generic session end as verified turn success.

| Fact | Required turn | Canonical effect |
| --- | --- | --- |
| SESSION_IDLE | absent | Clear active turn; IDLE |
| TURN_STARTED | nonzero | Set/replace active turn; WORKING |
| TURN_SUCCEEDED | matching active turn | SUCCESS; DONE |
| TURN_FAILED | matching active turn | FAILED; IDLE |
| TURN_ABORTED | matching active turn | ABORTED; IDLE |
| ATTENTION_REQUIRED | absent | ATTENTION; preserve active turn |
| ATTENTION_CLEARED | absent | WORKING if active, otherwise IDLE |
| NON_LIFECYCLE | ignored | No reducer input |
| Unknown | rejected | No reducer input |

Every lifecycle fact requires a session identifier and nonzero per-session sequence. Session and turn IDs are borrowed spans of 1–127 bytes, without embedded NUL. They are opaque host data, never wire fields; no Unicode/text interpretation is required. Validate every span before calling the mapper. The injected mapper must assign stable, unique, nonzero numeric keys for the lifetime of retained reducer state; collision or capacity failure is rejection. Raw payload content has no field in this contract. Output is cleared on rejected mapping; no reducer mutation occurs.

The mapper does not invent ordering. R2 must prove source ordering or assign an ordinal through one serialized per-session ingestion path. Ordinals must not restart while that session's state is retained, and must never wrap. A terminal event without a verified turn ID/outcome remains unavailable. Unsupported permission observations remain unavailable; verified permission observations may produce only content-free ATTENTION, never an approval decision.

Read-only donor inspection at S3 commit `90972308293076e9b321e90880ccc891a3d7c10b` found `UserPromptSubmit`, tool hooks, `Stop`, and `SessionEnd` mapping in `tools/windows_buddy_controller/codex_hook.py`. It also carries project/model/tool/hint/message and approval fields. This is pattern evidence only: no donor code or these fields are imported; its `Stop → complete` assumption does not establish product success. The existing rollout watcher stays distinct and only emits its previously verified `turn_aborted` lifecycle shape.

## Existing lifecycle and capacity

Reuse `ambient_reducer`, with no competing state model. Sequences are independent across sessions: equal sequence numbers in two sessions are valid. A retained session rejects sequence ≤ its last accepted sequence. A terminal for another/no active turn is rejected without consuming the ordinal. A newer start replaces the active turn, so the old turn cannot finish the replacement. Failed/aborted work cannot become DONE.

Aggregate priority is ATTENTION > WORKING > DONE > IDLE; no fresh sessions means OFFLINE. Freshness is inclusive at its configured deadline; stale means elapsed > freshness. DONE expires when elapsed ≥ its configured hold. Attention preserves an active turn; clearing attention returns to WORKING. Regressed monotonic time is clamped and reported locally, preventing rejuvenation or expiry reversal.

Fresh slots are never evicted. On exhaustion, return NO_CAPACITY; the oldest stale slot may be reused with a new identity and fresh sequence baseline. Ordering protection applies while a session identity is retained. R2 must prevent a retired identity's delayed stream from being reintroduced as a new session; there is no unbounded tombstone history. Companion has an explicit maximum of eight sessions and sixteen notification dedup keys. Generic reducer/dedup primitives remain caller-bounded. Companion now preserves OFFLINE instead of converting missing/stale source truth to IDLE merely because Companion itself is alive. Host process availability and Codex source truth are separate facts.

## Quota and reset

Only source-provided 300-minute and 10080-minute windows are supported. Missing windows are independently unavailable; expiry at `reset_marker <= now_unix_seconds` invalidates that window and its reset baseline. Unsupported windows are ignored. Finite source used percentage in [0,100] is mandatory when present. No token-based estimate and no remaining-percent field exist. An unrelated quota bucket is ignored without replacing existing Codex truth. Invalid/unavailable Codex observations cancel relevant baseline/candidate state and require re-baselining.

`AMBIENT_QUOTA_PRODUCT_RESET_DROP_PERCENT` is 15 percentage points; Companion requires this product setting. The generic detector remains configurable. Reset requires a drop ≥15pp and a marker greater than the highest previously observed comparable marker, then a subsequent comparable observation at the same candidate marker still satisfying the drop. A candidate alone emits nothing. Marker regression/recovery, a small drop, usage increase, missing data, or loss of confirmation do not manufacture a reset. Each window confirms independently and exactly once after confirmation/restart persistence.

The existing content-free reset store retains baseline, highest marker and candidate state, validates loaded data, and writes atomically. Corrupt/missing/unwritable persistence never creates quota availability or a successful reset. Companion reports load/write errors; an in-memory valid source observation remains valid when persistence fails. Restart cannot promise durable dedup if a write failed; R2 must expose that bounded diagnostic and test recovery. Cache/source data are not made available merely from stored reset state.

## Project-owned wire v1

`ambient_wire.h` is the complete approved semantic model. There are four kinds: HELLO=1, full SNAPSHOT=2, one-shot NOTICE=3, ACK=4. All fields are numeric enums/bitsets/counters; no string, blob, JSON object, extension map, or generic/opaque payload exists. The legacy `AMBIENT_MESSAGE_CONTROL` is reserved and always rejected. Companion's former arbitrary snapshot encoding callback is replaced by typed projection and publishing.

Forbidden data include prompts, transcripts, assistant content, tool output, shell commands, diffs, authentication material, raw authorization payloads and privileged approval decisions. Project/activity/model strings and raw session/turn IDs are also absent. Unknown kinds, fields, extra separators, appended data, unsupported encodings and unknown enums are rejected. Permission observations reduce to ATTENTION only. ACKs acknowledge semantic delivery and cannot trigger a Codex or privileged operation.

Encoding is fixed-width **uppercase ASCII hexadecimal**, with `|` between fields and one LF terminating the frame. Decode receives a line excluding LF. Each numeric width below is exact; lowercase, whitespace, CRLF, NUL, non-ASCII/UTF-8 content, signs and prefixes are unsupported. There is no extensible field syntax. Internal struct padding is never serialized.

| Kind | Fields in order (hex digits) | Frame bytes including LF |
| --- | --- | --- |
| All | kind(1), major version(2), generation(16) | prefix only |
| HELLO | prefix, offered capabilities(8), required capabilities(8) | 40 |
| SNAPSHOT | prefix, revision(16), status(1), fresh(1), working(1), attention(1), done(1), quota presence(1), short used(4), long used(4), short reset marker(16), long reset marker(16) | 95 |
| NOTICE | prefix, notice ID(16), snapshot revision(16), code(1) | 58 |
| ACK | prefix, target kind(1), ID/revision(16) | 41 |

Version is one major integer, currently1; unsupported versions fail closed, with no implicit downgrade. Capabilities: snapshot=bit0, notices=bit1, ACK=bit2; all three are required by v1. Peers advertise offered/required bitsets; negotiation uses the intersection of known bits, checks both required sets, ignores unknown optional offered bits, and rejects unknown required bits. There is no currently optional application capability. No state/notice/ACK is accepted before negotiation; an already negotiated session cannot renegotiate in place.

A nonzero 64-bit generation identifies one secure transport session. R3 must authenticate and establish a fresh generation across reconnect and process restart, without assuming wall-clock uniqueness. R1 does not provide authentication or randomness. Initialization clears negotiated state, received truth, revisions, ACKs, notice IDs and queues. Delayed records from another generation are rejected.

Snapshot revisions are nonzero monotonically increasing 64-bit counters per generation; no wrap. A revision becomes **emitted only after transport send returns OK**, meaning the bounded transport accepted ownership, not that the peer received it. Invalid encoding, backpressure and disconnect do not advance it. A newer full snapshot supersedes all pending one-shot notices. A receiver accepts only a newer full snapshot, atomically replacing retained truth. It may accept revision gaps because each snapshot is complete.

Only the latest actually emitted snapshot revision can be ACKed; stale, duplicate, zero and future ACKs do not mutate state. Notices may queue after a snapshot was emitted, but cannot be sent until that current revision is ACKed. Notice IDs are monotonic/nonzero with no wrap; each notice references the current snapshot revision. Receivers require that exact current revision and a newer notice ID. Gaps are allowed; duplicates/replay are dropped. Accepted notices never alter retained snapshot truth. Only the actually sent head notice can be ACKed; retry resends the same ID, bounded by caller scheduling. Full queue drops the new notice, preserving truth. Disconnect requires session initialization and framer reset before accepting new data; new negotiation and a full snapshot establish truth before notices are trusted. R3 must enforce secure disconnect/reset and finite retry timeouts; R1 runs no worker, timer or reconnect loop.

Snapshot status/counts must satisfy reducer priority, counts ≤8 and classified counts sum ≤fresh. Quota presence bits identify the two fixed windows; absent windows have zero wire usage/marker. Source used percentage is rounded to 0.01 percentage points (0–10000 basis points) only for wire presentation; reset logic retains the source double. No quota estimate or inferred remaining value is transmitted. Notice codes are ATTENTION, COMPLETED, ERROR, SHORT_RESET and LONG_RESET; no body or permission details.

## Bounds and recovery

| Item | R1 hard host maximum | Rationale / later measurement |
| --- | --- | --- |
| Raw Hook session/turn identifier | 127 bytes each | Local opaque identity admission budget; long IDs fail closed until source evidence justifies a reviewed change |
| Numeric field | 16 ASCII bytes | Full uint64 domain without variable-width parsing or integer overflow |
| Approved state payload/frame | 94 / 95 bytes | Exact worst case of fixed snapshot grammar, independent of values |
| Line / frame | 128 / 129 bytes | 34-byte grammar headroom; no new fields accepted merely because space exists |
| Framer storage | 129 bytes | 128-byte line plus local NUL; encoder needs no NUL |
| Retained typed snapshot | ≤64 host bytes | Compile-time struct budget; actual encoding remains95 bytes |
| Typed semantic message | ≤64 host bytes | Compile-time budget, no allocated payload |
| Protocol session | ≤512 host bytes | Compile-time budget includes four inline notice records and retained truth |
| Sessions / lifecycle dedup | 8 / 16 entries | Bounded concurrent dashboard scope, two dedup keys per maximum session on average; reducer ordering still rejects old lifecycle events after dedup eviction |
| Pending notices / transport queue evidence | 4 / 516 bytes | Four129-byte frame slots in host fake transport; bounded transient loss instead of unbounded backlog |
| Rollout host file line | existing1 MiB | Separate desktop file-parser budget, never a semantic frame or device allocation |

Eight-session, sixteen-dedup and four-notice bounds are conservative R1 admission limits, not measured ESP32 requirements or donor MTU values. R3 must measure C3 internal RAM high-water mark, parser/queue stack/heap usage, authenticated-link and BLE buffers, selected MTU/fragmentation overhead, maximum95-byte snapshot transfer and backpressure/reconnect under load with no PSRAM. It may tighten these limits only with documented compatibility/capability implications; it must not silently exceed any R1 hard maximum. Product preference keys require a separately reviewed schema change.

The existing caller-owned framer consumes through the first LF, supporting arbitrary split/coalesced input. Empty input needs more data. Exact-limit lines are bounded; over-limit input is discarded through LF and reported once, then the next frame can recover. A limit mismatch, impossible storage size or corrupt retained length is invalid without buffer access. A truncated record stays bounded and is discarded on session/framer reset; there is no end-of-file record acceptance. Empty/malformed/unsupported records are rejected by the strict decoder without changing protocol truth. Repeated errors cannot grow parser state.

## Local settings and unpair

`ambient_settings.h` freezes schema1 with **zero approved preference keys**, avoiding invented UX settings. Updates accept only this exact version and empty keyset, atomically. Unknown/corrupt versions or keys preserve current settings and fail closed; no automatic migration guesses. Later migrations need explicit bounded schema tests.

Any supplied local-effect output is cleared before request validation; rejected requests leave every effect flag false, including after a previous successful unpair or factory reset. Unpair requires an explicit device-local request; there is no remote unpair/factory-reset wire opcode. Its policy effects clear the bond and link/session state, preserving product settings and all host quota/history. Factory reset is a distinct explicit local action that may additionally clear product settings. Neither operation grants approval, submits to Codex, clears host quota persistence or runs commands. R1 returns only bounded success/failed/unavailable acknowledgement codes; the future local executor must report success only after effects finish, fail/unavailable otherwise. No bond deletion, NVS write or settings UI is implemented here.

## Validation evidence

`tools/validate.sh --static` compiles every new pure-C suite with C11, `-Wall -Wextra -Werror`, plus affected existing suites. Coverage includes unavailable/unverified Hook input, identifier/mapping rejection, all normalized facts, equal independent session sequences, active-turn replacement, capacity/stale recovery, attention overlay, outcome truth, freshness/clock boundaries, quota non-finite/range/threshold/reset/rebaseline behavior, atomic persistence failure/restart, all wire kinds, exact widths, malformed/truncated/extra/forbidden fields, incompatible version/capabilities, integer extremes/no wrap, split/coalesced framing, repeated oversized recovery, backpressure/disconnect, unsent/future/duplicate ACK, notice overflow/replay/drop and reconnect full-snapshot ordering. Rejection is followed by valid recovery tests. Static firmware-layout tooling uses synthetic artifacts and does not prove a firmware build or device behavior.

## Executable R2 plan — Companion Production Truth

R2 begins only after independent R1 review and explicit Control Room authorization. Use the current repository, not the old U4/U5/U6 chat plan.

### A. Autonomous work without changing user configuration

1. Build a host adapter around the verified normalized contract with an injected identifier registry, serialized per-session sequencing and outcome mapper. Use synthetic fixtures for start/success/failure/abort/attention, concurrent sessions, source loss and delayed retired streams; unsupported fixtures remain unavailable.
2. Inspect authorized installed application/CLI documentation and read-only source/version metadata to establish candidate source interfaces. Record versions, source paths and schema evidence, without changing Hook configuration or copying private payloads into diagnostics.
3. Extend Companion integration tests from source observation through reducer/quota persistence to typed snapshots/notices. Test restart, expiry, full-resync and bounded diagnostic codes. Keep fake transport; R2 introduces no device link.
4. Test atomic reset-store writes, corrupt/unwritable paths, restart during candidate/after confirmation, failed-write diagnostics and re-baselining without inventing quota truth.
5. Prepare reversible project-local startup/background lifecycle code and tests for idempotent start/stop, crashes, source unavailability, stale recovery and bounded queues. Persistent login items/daemons remain separately consented.

### B. Explicit consent / possible human-only blockers

| Evidence | Exact collection and acceptance |
| --- | --- |
| Hook version/schema | Record current Codex build/CLI version, supported Hook names and schema/version provenance; consent before installing/changing user Hooks. Capture only allowlisted metadata presence/types and synthetic/redacted test examples; prove session/turn identity and serialized ordering |
| Outcome mapping | Authorized real turn start, successful completion, failure and user abort; prove each terminal outcome and turn association; ambiguous `Stop` or termination cannot become SUCCESS |
| Concurrent sessions | Two overlapping real sessions with independent sequence baselines; delayed/duplicate records and session retirement/restart; prove no cross-session outcome contamination |
| Permission/attention | Authorized observation of attention-required and clearing, preserving an active turn; collect only bounded semantic metadata. macOS trust/accessibility dialogs or unobservable clearing are human blockers; no approval automation |
| Quota source | Identify a current authoritative production source, version/bucket and source-provided300/10080 window durations, used percentages and reset timestamps; missing, expired and unavailable behavior. No token or remaining estimation |
| Reset persistence | Real/reproducible source marker advance +≥15pp drop +confirmation, restart before/after confirmation, write failure and recovery; no forced/fabricated quota reset |
| Startup/background | User consent before persistent login/daemon changes; observe start/stop/restart, source outage and crash recovery; prove stale truth returns OFFLINE |
| Diagnostics/privacy | Explicit observation scope; verify logs contain only bounded codes/counters/window markers, never prompts/transcripts/tools/commands/diffs/auth/approval payloads; inspect actual produced sanitized output |

### C. Unsupported paths remain unavailable

Unknown Hook names/schema, missing identities/outcomes/ordering, unverified attention clearing, inaccessible quota endpoints, ambiguous session endings and unsupported App Server/Accessibility sources must return unavailable/unsupported. Do not infer success, reconstruct private content, guess quota, bypass consent or install a fallback. A human-only blocker stops the affected path and is reported with the precise missing evidence; independent synthetic work may continue inside R2 once authorized. R1 has not begun any of these live integrations.
