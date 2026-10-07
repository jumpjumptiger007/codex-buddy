<p align="right">
  <a href="upstream-maintenance.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Independent Repository Upstream Maintenance

`codex-buddy` is an independent repository. Its `main` branch tracks its own
`origin/main`; FoloToy upstream changes are references for review and are never
automatically synchronized or merged into this project's `main`.

## Remote roles

- `origin`: `https://github.com/jumpjumptiger007/codex-buddy.git`, this project's
  independent repository.
- `upstream`: `https://github.com/FoloToy/ai-passport.git`, the AI Passport
  source and comparison repository.
- Local `main` tracks `origin/main`. `upstream/main` does not replace or merge
  into `main` automatically.

Fetching `upstream` is allowed for read-only comparison. Fetching updates remote-
tracking references; it does not integrate commits into a local branch or alter
the working tree. Review divergence before proposing any integration. An
upstream integration must be a separate, reviewed task with explicit scope.
Normal maintenance does not rewrite history.

## Review upstream changes

After fetching, inspect the commit and file differences. Prioritize reusable
changes to BSP, hardware definitions, ESP-IDF and build tooling, firmware layout,
validation, and generally useful AI Passport behavior.

Project-specific Companion, protocol, security/session, product UI, and Codex
behavior remain owned by `codex-buddy` unless an upstream contribution is
selected separately. Do not automatically replace these project changes with
upstream versions.

Use read-only commands to compare the branches:

```bash
git fetch upstream
git status --short --branch
git log --oneline --left-right --graph main...upstream/main
git diff --stat main...upstream/main
git diff --name-status main...upstream/main
git log --oneline main..upstream/main
```

These commands inspect fetched references; they do not merge, rebase, cherry-
pick, or push. Review relevant commits and diffs, then propose an integration
task separately. Do not change remotes or branch tracking as part of upstream
review.

## Contributing changes upstream

An upstream pull request may use the separate GitHub fork
`jumpjumptiger007/ai-passport`. That repository is not this project's `origin`.
Configure it as a separate contribution remote when needed; keep `origin` pointed
to `jumpjumptiger007/codex-buddy`.

See the [downstream fork guide](../../fork-guide.md) only when working in an
actual fork-style project. Its branch model does not describe this repository.
