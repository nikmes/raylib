# Phase 0 — Baseline

| | |
|---|---|
| State | **done** (2026-11-20) |
| Depends on | nothing |
| Exit criterion | branch `psp-6.2` at tag `6.2` exists on `origin` and its contents are identical to the release tag |

## What this phase was about

Getting a repository we can grow: upstream raylib at a known release, under our own remotes, with history
intact so future upstream merges are ordinary merges rather than re-clones. No code was changed in this phase,
which is the point — it makes every later diff attributable to a decision rather than to setup noise.

## What was done

- Full clone of `raysan5/raylib` (413 MB `.git`, not shallow), branch `psp-6.2` created from tag `6.2`
  (`02566453`), contents verified against the published release tag.
- Remotes: `origin` = `nikmes/raylib` (fork), `upstream` = `raysan5/raylib`, `consoles` =
  `mcidclan/raylib4Consoles` for the PSP platform file.
- Author identity repo-local (`nikmes <nikmes@gmail.com>`); credential helper repo-local; no `~/.gitconfig`.
- Branch pushed, tracking `origin/psp-6.2`, sha verified on both sides.
- Documentation site (`mkdocs.yml`, `docs/`, systemd unit on port 8100) — outside the phase scope, but it is
  where the roadmap lives.

## Why the release tag and not `master`

`master` was already 6.5.0-dev on the day we started, three minors past the 6.2 release. A release tag is a
stable baseline; tracking dev would mean re-deriving the platform interface against a moving target. Because
the clone has full history, moving to 6.5.0-dev later is a merge, not a rebuild.

## What this phase deliberately did not do

- No PSP code imported, no build attempted, no samples run.
- No decision about pspGL versus a native backend — that is phase 5, and phase 3 informs it.

## Carried forward

- Open question 1 in [§12](../../memory-map.md#12-open-questions): when to re-baseline on 6.5.0-dev.
- The five regression samples live in `raylib4Psp/samples`, not in this tree — see
  [§11.2](../../memory-map.md#112-raylib-sources).

## Sprints

- [P0-S1 — Sprint 000 — Repository baseline](../../sprints/000-phase0-baseline.md) — **closed**, and doubles as the first
  filled example of the [sprint template](../../templates/sprint.md)
