<p align="right">
  <a href="AGENTS.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Repository Guidelines for AI Agents

This file is the durable entry point for AI-assisted work in this repository. Keep stable architecture and detailed design in project documentation; keep these instructions concise and operational.

## Product and hardware boundaries

- Keep AI Passport a thin client. Passport owns display/UI, character animation, buttons, microphone capture, speaker cues, BLE transport, and small deterministic state/reducer logic. Mac Companion owns Codex integration, quota acquisition, Codex lifecycle interpretation, notification generation, local STT, Codex composer text injection, and BLE reconnect/orchestration. Do not move Codex-specific parsing or heavy/volatile desktop integrations onto the ESP32 without an explicit architecture decision. Record stable boundary decisions in project architecture documentation.
- Target hardware is ESP32-C3 with 8 MB Flash, no PSRAM, and ESP-IDF 5.5.3. Budget internal RAM accordingly.
- Preserve the valid default 8 MB partition layout unless the product explicitly requires another valid and documented layout. Detailed partition policy is in `docs/development/engineering/firmware-layout.md`.
- Keep reusable board-specific hardware capabilities in `components/bsp`; application UI, state, animation, protocol behavior, and application tasks belong outside reusable BSP code. Treat product specifications and measurements, `components/bsp/include/bsp_pins.h`, BSP headers and implementation, then the hardware guide as the sources of hardware facts. Do not guess unspecified board details.
- LVGL is not thread-safe. Code outside the LVGL task must hold `bsp_lvgl_lock()` while accessing LVGL objects.
- Button callbacks must not block. Move audio, storage, networking, and other slow work to worker tasks.
- Stop tasks, timers, callbacks, and other producers that may access UI before destroying the affected UI.
- Keep state, reducer, protocol, timing, and layout logic that can be tested without hardware independent of ESP-IDF and LVGL; cover applicable logic with host tests.
- Product UI must be designed for the product requirements. Do not reuse the baseline hardware-test menu, screens, or visual shell as a product UI; BSP APIs and non-UI logic remain reusable.

## Safety, data, and distributable assets

- Passport is a product device, not an approval controller. Do not use its UI or BLE connection to grant repository, agent, deployment, or other privileged-operation approval.
- Keep BLE payloads, buffers, queues, and retained data explicitly bounded. Validate lengths and formats before parsing; transmit only data needed for the active feature. Do not send arbitrary prompts, transcripts, shell commands, diffs, tool output, assistant content, authentication material, or secrets to Passport. This does not prohibit a later explicitly modeled voice-audio transport or bounded project/status fields. Do not log or commit credentials, private user/device data, or unsanitized logs.
- Public character engine/code and public example assets must remain distributable. Spider-Man and any other private or non-distributable character assets must never enter public Git history or public releases.
- Before introducing any private/local character source pack, explicitly protect its source path in `.gitignore`. Do not decide that path in this file. Generated public assets must never silently derive from private, non-distributable sources.

## Required Passport skills

The five core skills are `passport-develop`, `passport-setup`, `passport-build`, `passport-device-test`, and `passport-debug`. Before firmware development, confirm that these skills are available and use only the skill relevant to the active task.

If a required skill or tool is unavailable, follow the active task and environment authorization boundaries. The skill requirement does not authorize changes to global or user configuration, system-package installation, or other host-environment changes.

## Task routing

- For firmware/code work, read `docs/development/ai-guide.md`, affected public headers, and neighboring implementation.
- For board, BSP, display, audio, button, or battery work, consult the relevant hardware guide and BSP sources.
- For build, dependency, or partition work, consult `docs/development/engineering/build-and-test.md` and `docs/development/engineering/firmware-layout.md`.
- For maintained documentation, follow `docs/contribution/doc-conventions.md`.
- Read only context relevant to the active task; do not mechanically load all repository documentation.

## Git and authorization

- This is an independent repository: `origin` is `https://github.com/jumpjumptiger007/codex-buddy.git`, `upstream` is `https://github.com/FoloToy/ai-passport.git`, and local `main` tracks `origin/main`. `upstream/main` is a reference; review divergence and integrate upstream changes only in a deliberate, separately reviewed task.
- Interpret inherited upstream/reference branch instructions against the `upstream` remote in this repository; for example, inherited `origin/demo/*` references mean `upstream/demo/*` here.
- Check `git status --short --branch` before editing. Preserve unrelated user changes; do not overwrite, clean, or include them in a task change.
- Do not perform destructive Git operations or rewrite shared history. Commit, push, flash, erase, deploy, release, and publish only when explicitly authorized by the active task or project workflow.
- Never commit credentials, private character assets, personal data, or unsanitized logs.

## Validation and C2C

- Use the repository validation entry points appropriate to the change: `./tools/validate.sh --static` for repository/host validation, `./tools/validate.sh --firmware` for firmware build validation, and `./tools/validate.sh` for the complete gate when required.
- Run the smallest relevant checks while iterating. Documentation and pure-host changes should at least pass the applicable static validation; firmware delivery follows the firmware/full-gate policy.
- Report `Build`, `Host tests`, `Device tests`, and `Unverified` separately.
- When the active workflow requires C2C review, use the project-bound C2C connector and let ChatGPT inspect the actual workspace, Git state, diffs, files, and recorded execution output directly. Do not require users to paste repository file bodies, diffs, or logs that are already accessible through C2C.
- Codex executes bounded tasks; ChatGPT performs independent review. Do not automatically continue into the next architectural task after review.

## Maintained Markdown

Use English at each maintained Markdown document's default `.md` path and Simplified Chinese in its paired `.zh_CN.md` file. Keep the versions aligned and retain reciprocal language-switch links.
