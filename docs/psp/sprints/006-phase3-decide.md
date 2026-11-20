# Sprint 006 — Adopt the fork, reproducibly

| | |
|---|---|
| Roadmap phase | [Phase 3 — pspGL upgrade A/B](../phases/03-pspgl-upgrade.md) |
| Branch / base commit | `master` @ `276ed105` |
| Started / closed | — / — |
| Build number(s) | `g2` (adopted library) |
| Tasks | 0/7 done |

## 0. Handover

**The decision is already made by sprint 005's data: adopt `TheMrIron2/pspgl`.** This sprint makes the adoption
survive the next session, re-publishes the baseline on the adopted library, and takes the one experiment the
profiler made worth running.

**Sprint 005's result, which this sprint acts on:**

| arm | N=1000 | N=5000 |
|---|---|---|
| shipped package | 90.285 ms | 436.422 ms |
| `de4260ad` built here (control) | 90.301 ms | 436.428 ms |
| **fork HEAD `ae9a668`** | **81.240 ms** | **392.060 ms** |

−10.03 % / −10.17 %, control within 0.03 % of shipped, symbol set a strict superset (287 identical + 6 added,
nothing removed), BSD-3. The machine is already staged on the fork at `PSPGL_PROFILE=0`.

**The lead the profiler handed us:** 257 command-list submissions per frame for 1,025 primitives at the default
`PSPGL_DLIST_SIZE_WORDS=512`, each with a wait, totalling **2,725 µs/frame** of command-list reuse wait. A bigger
list may collapse some of those submissions. That is T3.

**Read before acting:** [baselines](../baselines.md) (the numbers that must be re-published), [memory map](../memory-map.md)
§4B (the A/B and the profiler findings), §7 (licences), §8 (decision log format).

**First action.** T1 — write the decision down before touching anything. If this session ends early, the next
one must know what was chosen and why.

**Hard rules.**

- **Reproducible or not adopted.** `tools/pspgl-build.sh` is the artefact: pinned commit, recorded flags,
  verified output. A `libGL.a` that exists only on this disk is not adoptable.
- **Licence:** BSD-3, copyright notices intact. The fork is a personal repository — pin by **commit sha**, not by
  branch name, and record the sha256 of the archive the script produces.
- Every library swap requires `make clean` in the app (static link), then a re-measure. A stale PRX measures the
  old library and will lie.
- Threshold for reporting a delta stays **0.1 %**; two runs per arm.
- The profiling build is never an A/B arm (18.6 % slower).
- Never send `exit` to pspsh; `reset` between runs; the app must not call `sceKernelExitGame`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

# restore what pspdev ships, any time
cp /home/psp/libGL-pspdev-shipped.a.bak $PSPDEV/psp/lib/libGL.a

# relink + measure after any swap
cd /home/psp/raylib-psp/psp-tests/bunnymark
make clean && make PSP_LARGE_MEMORY=0 BUNNY_COUNT=1000 FRAMES=100 TARGET_FPS=0
```

**Done means** §8: the decision recorded, a script that reproduces the adopted library, a re-published baseline,
and a restore path that has actually been walked.

## 1. Objective

The fork adopted with a script that reproduces it from a pinned commit, the baseline re-published on it, a
command-list size chosen by measurement, and a restore path that works when exercised.

## 2. Why now

10 % for a library swap is the cheapest win in the project, and phase 4's batcher will be judged against this
baseline. Two things must be true before that: the library must be reproducible (or phase 4's numbers rest on an
artefact nobody can rebuild) and the baseline must describe the library actually in use.

## 3. Scope

**In:** the decision record, `tools/pspgl-build.sh`, the dlist-size experiment, baseline re-publication,
verification beyond bunnymark, the restore path, documentation.

**Out:** raylib changes, the batcher, the native backend, anything past `ae9a668`.

## 4. Preconditions

- [ ] Sprint 005 closed; the numbers above verified in its result section
- [ ] `libGL.a` shipped backup present and restorable (`/home/psp/libGL-pspdev-shipped.a.bak`, 315,198 B)
- [ ] Fork tree at `/home/psp/ref/pspgl-themiriron2`, commit `ae9a668`
- [ ] `psp-tests/bunnymark` relinks in under two minutes
- [ ] PSPLink up; `reset` gives 25,149,440 B free

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | record the decision: adopt `TheMrIron2/pspgl` @ `ae9a668`, with sprint 005's numbers and the licence | `todo` | phase page and memory map §8 both say the same thing, with the delta that justified it | — |
| T2 | `tools/pspgl-build.sh`: `set -euo pipefail`; clone-or-reuse, checkout the pinned commit, `make clean && make`; print and record the sha256 of the built `libGL.a`; install `libGL.a` + headers into `$PSPDEV/psp`; refuse to run without `$PSPDEV` | `todo` | running it twice produces the same sha256; a fresh `$PSPDEV` gets a working library from the script alone | — |
| T3 | dlist experiment: `PSPGL_DLIST_SIZE_WORDS` = 512 / 1024 / 4096, `make clean` between builds (config stamp), two runs each at N=1000 | `todo` | a size beats 512 by >0.1 % and is adopted, or 512 stays with the flat result recorded; **record the memory cost of the list, checked in `pspgl_dlist.c` rather than assumed** | — |
| T4 | re-publish [baselines](../baselines.md) on the adopted library: 100 / 1,000 / 5,000 / 10,000, two runs each | `todo` | new median table; the old numbers kept as "pre-adoption" for the record | — |
| T5 | verify beyond bunnymark: `basic_shapes` renders (two captures that are identical) and one corpus sample runs | `todo` | 284 colours, matching the pre-adoption fingerprint; no exception in any log | — |
| T6 | walk the restore path: swap pspdev's library back, relink, measure | `todo` | median returns to ~90.3 ms at N=1000; then restore the adopted one and confirm ~81.2 ms | — |
| T7 | documentation: memory map §7 provenance and §8 decision, the environment page's staged-library line (commit, sha256, how to rebuild and how to revert) | `todo` | `--strict` clean; the environment page names the command to restore | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- _(not started)_

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| reproducible build | script-only, same sha256 twice | | |
| restore time | < 5 minutes by hand | | |
| adopted dlist memory | recorded, not assumed | | |

## 7. Measurements

Command-list size (adopted library, N=1000, two runs each):

| `PSPGL_DLIST_SIZE_WORDS` | run A | run B | Δ vs 512 | memory |
|---|---|---|---|---|
| 512 (default) | | | — | |
| 1024 | | | | |
| 4096 | | | | |

Re-published baseline (adopted library, two runs each):

| bunnies | median A | median B | `glEnd`/frame |
|---|---|---|---|
| 100 | | | |
| 1,000 | | | |
| 5,000 | | | |
| 10,000 | | | |

Restore check:

| library | median N=1000 | note |
|---|---|---|
| adopted | | expect ~81.2 ms |
| pspdev shipped (restored) | | expect ~90.3 ms |

## 8. Exit criteria

- [ ] the decision is recorded in the phase page and §8 with its numbers
- [ ] `tools/pspgl-build.sh` reproduces the adopted library, twice, with the same sha256
- [ ] the dlist size is either measurably better than 512 or 512 is kept with the flat result written down
- [ ] the baseline is re-published on the adopted library, old numbers preserved
- [ ] `basic_shapes` and one corpus sample still run
- [ ] the restore path has been walked, not described
- [ ] Phase 3 exit criteria recorded in the phase page

## 9. Result

(To fill in on close. Include the dlist verdict — if a bigger list buys nothing despite 257 submissions/frame,
the reason matters: the wait may be GE-side, not list-capacity-side.)

## 10. Follow-ups

- If the dlist experiment is flat, the ~2,725 µs/frame of reuse wait is a GE-side cost and the lever is phase 4,
  not list sizing. Say so in §4B.
- A personal fork is a dependency: record how to notice upstream moving (`pspdev/pspgl` HEAD was base + 2 CI
  commits) and whether any of the 11 commits land upstream later.
- If pspdev ever ships a newer `pspgl` package, the sha check in the script is how we notice it replacing our
  adopted library.
