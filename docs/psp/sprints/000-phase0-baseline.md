# Sprint 000 — Repository baseline

| | |
|---|---|
| Roadmap phase | [Phase 0 — Baseline](../phases/00-baseline.md) |
| Branch / base commit | `psp-6.2` @ `02566453` (tag `6.2`) |
| Started / closed | 2026-11-20 / 2026-11-20 |
| Build number(s) | none — no raylib build in this sprint |
| Tasks | 4/4 done |

## 0. Handover

**This sprint is closed.** It exists as a record of the baseline work and as the first filled example of the
template. Do not redo it.

- If you are picking up the project: read this page in two minutes, then go to
  [sprint 001](001-phase1-import-platform.md). Nothing here needs your attention.
- If you are checking the baseline still holds: `git -C /home/psp/raylib-psp rev-parse HEAD` should be
  `02566453880e06bec97f5b8160fa6fe1e98397ca` on branch `psp-6.2`, and `git status` should be clean apart from
  untracked docs.
- If the branch or remotes are gone, that is a regression against this sprint — recreate from tag `6.2` and
  record it in the task log rather than starting a new sprint for it.

**Read instead:** [memory map](../memory-map.md) §1 (status), §11 (source index), and the
[Phase 0 page](../phases/00-baseline.md).

## 1. Objective

A clone of upstream raylib at release tag 6.2, under our own remotes, on a branch that can be pushed, with no
source changes — verified by comparing the branch contents to the published release tag and confirming the
remote sha matches locally.

## 2. Why now

It is the precondition for every other measurement in this project. Basing on a moving `master` (which was
already 6.5.0-dev) would mean the platform interface changes under the port while it is being written.

## 3. Scope

**In:** cloning, branch creation, remote topology, author identity, credential helper, first push.

**Out:** the PSP platform file, any build, any sample. Also out, but done anyway: the documentation site,
because the roadmap needed somewhere to live.

## 4. Preconditions

- [x] Network access to GitHub, `gh` available
- [x] pspdev toolchain present (not needed this sprint, needed next)
- [x] Decision recorded: release tag, not `master`

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | full clone (not shallow), branch `psp-6.2` from tag `6.2` | `done` | `git rev-parse 6.2` equals `02566453880e06bec97f5b8160fa6fe1e98397ca` → expected: identical to the release tag published by upstream | — |
| T2 | remotes: `origin` = `nikmes/raylib`, `upstream` = `raysan5/raylib`, `consoles` = the port repo | `done` | `git remote -v`; the platform file readable as `consoles/raylib4Consoles:src/platforms/rcore_psp.c` | — |
| T3 | author identity and credential helper, both repo-local | `done` | `git config --local --list`; no `~/.gitconfig` created | — |
| T4 | push branch, set tracking | `done` | sha matches on both sides; branch visible in the GitHub branch list | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never
renumber; a task that no longer applies is `dropped` with the reason in the log below.

### Task log (append-only)

- 2026-11-20 — T1 `done`, `02566453` verified against the published release tag
- 2026-11-20 — T2 `done`, three remotes wired, platform file readable from `consoles`
- 2026-11-20 — T3 `done`, identity and credential helper repo-local
- 2026-11-20 — T4 `done`, branch pushed, tracking set, sha identical both sides

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| repository size | any | 413 MB `.git`, full history | `[measured]` |
| disk | — | 1.6 GB Sony SDK + 48 MB references + repo | `[measured]` |

## 7. Measurements

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| — | — | clone | `.git` size | 413 MB | full history, not shallow |
| — | — | tag check | `6.2` sha | `02566453` | matches upstream release |
| — | — | push | local vs `origin/psp-6.2` | identical | `02566453` both sides |

## 8. Exit criteria

- [x] branch exists on `origin`, contents identical to tag `6.2`
- [x] remotes and identity recorded
- [x] no source changes in this sprint (`git status` clean at the time of the push)

## 9. Result

Two things went wrong, both now documented so they are not rediscovered:

1. **`git push` failed with "could not read Username"** until a credential helper was configured. `gh auth
   login` sets its own config but does not wire git's helper; the fix was a repo-local
   `credential.https://github.com.helper = !/usr/bin/gh auth git-credential`.
2. **The docs site needed `mkdir .cache`** before systemd would start it — `ReadWritePaths` cannot bind-mount a
   path that does not exist, and the unit failed with `status=226/NAMESPACE`.

Not a sprint item, but worth recording: the site is served by a systemd unit on port 8100 (8000 and 8080 are
taken by docker-proxy). See `/home/psp/hello-psp/NOTES.md`.

## 10. Follow-ups

- [Open question 1](../memory-map.md#12-open-questions): when, if ever, to re-baseline on 6.5.0-dev.
- Sample corpus stays in `raylib4Psp/samples`, outside this tree. Decide before sprint 002 whether to vendor it.
