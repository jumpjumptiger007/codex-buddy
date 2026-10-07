---
name: plays-archive
description: After a firmware release, archive the published application into the upstream FoloToy ai-passport repository's docs/reference/ directory with an AI-generated bilingual functional summary (text-only; the cover image is recorded by file name and format, not committed).
---

<p align="right">
  <a href="SKILL.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Archive an Application to Reference

This skill archives a published application into the upstream
`FoloToy/ai-passport` repository's `docs/reference/` application archive so it is
discoverable in-repository for later querying. It runs after a firmware release
(see `docs/development/release/publish-to-community.md` for publishing itself) and only
when the developer asks to archive the application.

## Safety and consent gate (run first)

Do not create, write, or commit anything until every gate below is satisfied.

1. **Confirm consent up front.** This work touches project-private content.
   Ask the developer to confirm they agree to archive the application. If they
   decline, stop immediately.
2. **Never modify or commit on the current branch.** Base the archive on the
   latest upstream `main` for a clean baseline and create a dedicated branch or
   worktree. Push to a separate fork of `FoloToy/ai-passport` and open the PR
   from that fork branch. In the `codex-buddy` checkout, `origin` belongs to
   `jumpjumptiger007/codex-buddy`; configure and use a separate remote such as
   `contribution` for `https://github.com/jumpjumptiger007/ai-passport.git`.
   Leave the current checkout and its `origin` untouched unless a separate
   workflow explicitly authorizes a change.
3. **No credentials or private data.** Never include credentials, device QR
   secrets, private device links, personal data, or unsanitized logs. Run
   `python3 tools/check_repo.py` before committing anything.

## Determine what to archive

Confirm the application name, the source it belongs to (for example a `demo/*`
branch or `main/`), and the contributor's GitHub username. Use the
lowercase-kebab-case username and application name beneath the repository-relative
path `docs/reference/<username>/<app-name>/`. See
[`../../docs/reference/README.md`](../../docs/reference/README.md) for the full convention.

## Check the project README

Before generating the summary, check whether a **root README** exists on the
relevant source branch and current archive branch:

- `git ls-tree --name-only main README.md` — is there a README on `main`?
- `test -f README.md` — is there a README on the current branch?

The fork-owner/root-README rules in [`docs/fork-guide.md`](../../docs/fork-guide.md)
apply only when working on an actual downstream fork that follows that guide.
For archive work in the independent `codex-buddy` repository, a root README may
inform the functional summary, but do not automatically create or maintain a
fork-style root README catalog. Keep the archive discoverable through
`docs/reference/README.md`. Any separate root README change in codex-buddy
requires its own authorization.

1. **If a README exists** (on `main` or the current branch): when archiving, **merge
   the README content into the functional summary** so the summary reflects the
   human-facing description, not just the code. A README that already exists is
   kept for the branch that owns it.
2. **If no README exists**: summarize directly from the implementation, with no
   README merge.
3. **Only for an actual downstream fork following `docs/fork-guide.*`**, handle
   each branch's root README according to that guide. Do not apply fork-owner
   rules to the independent `codex-buddy` repository or automatically add an
   archive catalog to its root README.

## Generate the functional summary

First collect the metadata the developer filled in when publishing to the
community (bilingual title, bilingual description, and the source address they
submitted), then write `docs/reference/<username>/<app-name>/README.md` and its paired
`.zh_CN.md` as an AI-generated functional summary for later querying (not a
publishing artifact). Record:

- **Publish title and description**: the bilingual title and description the
  developer submitted when publishing to the community.
- Application name and one-line positioning.
- What the app does and its feature list.
- Interaction and gameplay (buttons, screens, flow).
- Source, given as the **source address the developer submitted when
  publishing** (the HTTPS Git source page), so the application can be located
  precisely.
- The cover image file name and format, recorded as publish metadata only — the
  cover image itself is **not** committed (the archive is text-only).

If a relevant root README exists, merge its content into the summary rather
than ignoring the human-facing description. This does not imply a root README
change.

Write the default `.md` in English and the `.zh_CN.md` in Simplified Chinese,
aligned in the same change. Register the application in both language versions
of `docs/reference/README.md` so the archive can be discovered from the index.

## Cover image

The archive is **text-only**: do **not** commit the cover image. Record only its
file name and format in the summary as publish metadata. The image itself lives
with the community publication; if a cover must be generated for the publication
(not the archive), use the official product references under
[`docs/brand/`](../../docs/brand/README.md): pass a reference (e.g.
`ai-passport-front.png` or a colorway shell render) as input to the generation
call, keep its shell, buttons, ports, and key-ring hole as they are, and redraw
only the reference's screen region into the play's actual on-screen content,
keeping the screen's size, aspect ratio, corners, and position identical to the
reference. See the full
convention in [`docs/brand/README.md`](../../docs/brand/README.md).

## Commit

Commit the summary on the dedicated branch (English imperative
Conventional Commit title, for example
`docs(reference): add <app-name> application archive`). Include a root README
only when operating on an actual downstream fork under its fork-guide rules
and that change is authorized. Do **not** store the merged
firmware `.bin` here; it is a build/publish artifact. Report Build, Host tests,
Device tests, and Unverified separately.

After review, open the PR from the fork branch against the upstream
`FoloToy/ai-passport` through the first available GitHub channel — GitHub MCP, a
GitHub skill, or
`gh pr create --repo FoloToy/ai-passport --base main --head <fork>:<branch>` —
and read it back to confirm. Opening a PR requires separate confirmation.

## What this skill does not do

- It does not publish firmware or run the publisher workflow.
- It does not modify production source or the firmware.
- It does not store the firmware `.bin` binary.
- It does not store the cover image (the archive is text-only).
- It does not auto-submit anything without developer review and consent.

## Related documents

- Application archive convention: `../../docs/reference/README.md`
- Post-release follow-up overview: `docs/development/release/project-completion.md`
- Firmware publishing: `docs/development/release/publish-to-community.md`
- Contribution and commit rules: `docs/contribution/commit-and-pr.md`
