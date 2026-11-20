# Sprint 006 — Decide, and make the choice reproducible

| | |
|---|---|
| Roadmap phase | [Phase 3 — pspGL upgrade A/B](../phases/03-pspgl-upgrade.md) |
| Branch / base commit | `master` @ the sprint 005 commit |
| Started / closed | — / — |
| Build number(s) | `g2` (adopted library) |
| Tasks | 0/6 done |

## 0. Handover

**Situation.** Sprint 005 produced a three-arm comparison: the shipped pspGL, `de4260ad` built here, and
`TheMrIron2` HEAD built here. This sprint turns that into a decision that survives contact with the next session.

Three outcomes are legitimate and all three must be written down with their evidence:

1. **Adopt the fork** — its 11 commits moved the numbers past 0.1 %.
2. **Keep the package** — the fork moved nothing measurable; the cheapest choice is the one pspdev maintains.
3. **Take individual commits** — profiling is useful, the rest is not (or vice versa). This is the most likely
   outcome and the most work: it means a small fork of our own with a recorded base.

**Read before acting:** sprint 005's result table; [Memory map](../memory-map.md) §7 (the licence table and the
provenance of what we ship today); [baselines](../baselines.md).

**First action.** If sprint 005's control arm disagreed with the shipped package by more than 0.1 %, stop: the
build recipe is the variable and no adoption decision is safe until that is explained.

**Hard rules.**

- **Reproducible or not adopted.** A `libGL.a` built by hand with no recorded recipe cannot be adopted: the next
  session cannot rebuild it, and phase 4's numbers would then rest on an artefact nobody can reproduce.
- Licence first: pspGL is **BSD-3**; keep the copyright notices intact and record the commit we build from.
- Whatever is adopted gets a restore path: pspdev's shipped `libGL.a` backed up, and one command to put it back.
- Every arm relinked and re-measured after the swap. A stale PRX was linked against the old library and will lie.
- Threshold stays **0.1 %**; two runs per arm.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

# restore the shipped library at any time
cp /home/psp/libGL-pspdev-shipped.a.bak $PSPDEV/psp/lib/libGL.a

# after any swap, relink before measuring
cd /home/psp/raylib-psp/psp-tests/bunnymark && make clean && make PSP_LARGE_MEMORY=0 BUNNY_COUNT=1000 FRAMES=100 TARGET_FPS=0
```

**Do not touch.** raylib's `src/`, the platform layer, the harness.

**Done means** §8: one of the three outcomes recorded with its numbers, a reproducible build recipe if anything
was adopted, and the memory map updated.

## 1. Objective

A recorded decision — adopt the fork, keep the package, or take specific commits — with the evidence behind it,
a reproducible way to produce the adopted `libGL.a`, and a restore path to what pspdev ships.

## 2. Why now

Adoption is only worth anything if it is repeatable. The tempting shortcut — copy a `libGL.a` built on this
machine and move on — makes phase 4's baseline depend on an artefact that cannot be rebuilt or explained.

## 3. Scope

**In:** the decision and its record, a build script if adopting, the command-list size experiment if the fork is
adopted, verification that the samples and bunnymark still run, restore path.

**Out:** raylib changes, batching, the native backend, upgrading past what sprint 005 measured.

## 4. Preconditions

- [ ] Sprint 005 closed with a verdict against the 0.1 % threshold
- [ ] Shipped `libGL.a` backup present and restorable
- [ ] A relink-and-run loop that takes under two minutes

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | state the decision and the numbers behind it, in the phase page and the memory map §8 | `todo` | one of the three outcomes named, with the delta that justifies it | — |
| T2 | if adopting (whole or partial): a build script in the repo — pinned commit, recorded flags, install path, `set -e` | `todo` | a clean machine with pspdev can reproduce the adopted `libGL.a` from the script alone; the script records the sha256 it produces | — |
| T3 | if the fork is adopted: `PSPGL_DLIST_SIZE_WORDS` experiment at 512 / 1024 / 4096, two runs each at N=1000 | `todo` | a size that measurably beats 512 is adopted, or 512 stays with the flat result recorded | — |
| T4 | verify the adopted library on more than bunnymark: `basic_shapes` renders (fresh two-identical captures) and one corpus sample runs | `todo` | shapes match the 6.2-era fingerprint (284 colours); no exception in any log | — |
| T5 | restore path exercised: put pspdev's library back, relink, confirm the baseline median returns within 0.1 % | `todo` | median back at ~90.3 ms at N=1000 | — |
| T6 | update [§7](../memory-map.md#7-reference-sources-read-only-outside-this-repo) provenance, §8 decision log, and the environment page's staged-library line | `todo` | docs match reality; `--strict` clean | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- _(not started)_

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| restore time | < 5 minutes by hand | | |
| adopted library reproducibility | script-only, no manual steps | | |
| licence | BSD-3, notices intact | | |

## 7. Measurements

If a command-list size is adopted:

| `PSPGL_DLIST_SIZE_WORDS` | N=1000 run A | N=1000 run B | Δ vs 512 |
|---|---|---|---|
| 512 (default) | | | — |
| 1024 | | | |
| 4096 | | | |

Restore check:

| library | median N=1000 | note |
|---|---|---|
| adopted | | |
| pspdev shipped (restored) | | expect ~90.3 ms |

## 8. Exit criteria

- [ ] the decision is recorded with its numbers, in the phase page and the decision log
- [ ] if anything was adopted: reproducible by script, sha256 recorded, licence notices intact
- [ ] `basic_shapes` and one corpus sample still run on the adopted library
- [ ] the restore path has been exercised, not just described
- [ ] Phase 3 exit criteria recorded in the phase page

## 9. Result

(To fill in on close. "We kept what pspdev ships" is a perfectly good outcome and should be written as one,
with the delta that made it the choice.)

## 10. Follow-ups

- If the fork's profiling is adopted, it becomes part of the phase 4 measurement toolkit — record how to switch
  it on, because a profiling build is not the build you ship.
- A partial fork means we now maintain a patch set; note where it lives and how to rebase it.
- If the library turns out not to matter at this workload, say so plainly in the phase page so phase 4 is not
  re-litigated later.
