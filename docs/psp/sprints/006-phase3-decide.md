# Sprint 006 — Adopt the fork, reproducibly

| | |
|---|---|
| Roadmap phase | [Phase 3 — pspGL upgrade A/B](../phases/03-pspgl-upgrade/index.md) |
| Branch / base commit | `master` @ `276ed105` |
| Started / closed | 2026-11-20 / 2026-11-20 |
| Build number(s) | `g2` (adopted library) |
| Tasks | 7/7 done |

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
| T1 | record the decision | `done` | decision log entry with both deltas (commits −10.0 %, list size −24.1 %) and the memory cost | §8 |
| T2 | `tools/pspgl-build.sh` | `done` | `set -euo pipefail`, pinned commit, refuses without `$PSPDEV`, installs archive + `GL/`/`GLES/` headers. **The archive is never byte-identical** (two objects embed `__DATE__`), so reproducibility is checked on a content hash — identical across rebuilds, and `--check` verifies the installed library | `tools/pspgl-build.sh` |
| T3 | dlist experiment | `done` | 512 → 81.24, 1024 → 72.37, 4096 → 63.60, **8192 → 61.63**, 16384 → 62.73 (turns over). Cost read from `pspgl_dlist.c`: `NUM_CMDLISTS 16` × 8,192 words × 4 B = **512 KiB** | §7 |
| T4 | re-publish baselines | `done` | 16.68 / 61.63 / 294.63 / 586.70 ms; pre-adoption table kept and labelled | [baselines](../baselines.md) |
| T5 | verify beyond bunnymark | `done` | `basic_shapes`: 600 frames, 0 exceptions, **284 colours**, two identical captures. `cubicmap`: 3 textures loaded, 0 exceptions | §7 |
| T6 | walk the restore path | `done` | pspdev's library back: 90.287 / 90.303 ms; adopted again by script: **61.642 / 61.630 ms** | §7 |
| T7 | documentation | `done` | §4B, §7, §8, environment page, baselines page; `--strict` clean; restore command named | same commit |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- 2026-11-20 — T1 `done`; T2 `done`: script builds and installs; **the archive is not byte-reproducible** — two
  objects (`eglQueryString.o`, `glGetString.o`) embed `__DATE__` in the version string. Switched the contract to
  a content hash, identical across rebuilds, plus `--verify-repeat` and `--check`
- 2026-11-20 — T3 `done`: the list size is a bigger lever than the commits. 512 → 81.24, 1024 → 72.37,
  4096 → 63.60, 8192 → **61.63**, 16384 → 62.73. Adopted 8192; cost 512 KiB of main RAM (16 lists)
- 2026-11-20 — `--check` caught its own stale expectation: it still held the 512-word hash after the default
  moved to 8192. Updated to the adopted configuration's hash
- 2026-11-20 — T4 `done`: baseline re-published, 61.63 / 294.63 / 586.70 ms
- 2026-11-20 — T5 `done`: basic_shapes 284 colours with two identical captures; cubicmap loads 3 textures
- 2026-11-20 — T6 `done`: restore walked in both directions (90.29 → 61.63)
- 2026-11-20 — T7 `done`

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| reproducible build | script-only, same sha256 twice | | |
| restore time | < 5 minutes by hand | | |
| adopted dlist memory | recorded, not assumed | | |

## 7. Measurements

Command-list size (adopted library, N=1000, two runs each):

| `PSPGL_DLIST_SIZE_WORDS` | run A | run B | Δ vs 512 | memory (16 lists) |
|---|---|---|---|---|
| 512 (default) | 81.232 | 81.247 | — | 32 KiB |
| 1024 | 72.364 | 72.368 | −10.9 % | 64 KiB |
| 4096 | 63.602 | 63.624 | −21.7 % | 256 KiB |
| **8192 (adopted)** | **61.625** | **61.641** | **−24.1 %** | **512 KiB** |
| 16384 | 62.732 | 62.722 | −22.8 % | 1 MiB — turns over, rejected |

Re-published baseline (adopted library, two runs each):

| bunnies | median A | median B | note |
|---|---|---|---|
| 100 | 16.684 | 16.689 | pacing, not cost |
| 1,000 | 61.625 | 61.641 | 16.2 fps |
| 5,000 | 294.606 | 294.660 | 3.4 fps |
| 10,000 | 586.694 | 586.704 | 1.7 fps |

Restore check:

| library | median N=1000 | note |
|---|---|---|
| adopted (fork, 8192) | 61.642 / 61.630 | matches the sweep |
| pspdev shipped (restored) | 90.287 / 90.303 | matches pre-adoption |

## 8. Exit criteria

- [x] the decision is recorded in §8 with its numbers
- [x] `tools/pspgl-build.sh` reproduces the adopted library; the reproducibility claim is content-based because
      two objects embed `__DATE__`
- [x] the dlist size is measurably better: 8192 at −24.1 % vs 512, with its 512 KiB cost recorded
- [x] the baseline is re-published on the adopted library, old numbers preserved
- [x] `basic_shapes` (284 colours, two identical captures) and `cubicmap` (3 textures) still run
- [x] the restore path walked in both directions
- [x] Phase 3 exit criteria recorded in the phase page

## 9. Result

**Adopted, and it turned out to be two wins, not one.** The library swap is worth 10 %; the command-list size it
made discoverable is worth another 24 %.

| configuration | N=1,000 median | vs shipped |
|---|---|---|
| pspdev's shipped pspGL (2022) | 90.285 ms | — |
| fork `ae9a668`, default 512-word lists | 81.240 ms | −10.0 % |
| fork, 4096 | 63.602 ms | −29.6 % |
| **fork, 8192 (adopted)** | **61.63 ms** | **−31.8 %** |
| fork, 16384 | 62.73 ms | −30.5 % — turns over |

Full baseline on the adopted library: 16.68 / 61.63 / 294.63 / 586.70 ms at 100 / 1,000 / 5,000 / 10,000.
Marginal cost **58.3 µs per sprite**, ~287 sprites in a 16.7 ms frame.

**Reproducibility needed redefining.** The archive is never byte-identical between builds: exactly two objects
(`eglQueryString.o`, `glGetString.o`) embed `__DATE__` in pspGL's version string — the same string raylib prints
as `pspGL build <date>`. Everything else is stable. The script therefore records a **content hash** over all
other members, `--verify-repeat` shows it is identical across builds, and `--check` compares the installed
library against it. That check caught its own stale expectation when the default size moved from 512 to 8192,
which is the behaviour one wants from a check.

**Cost, stated plainly:** 16 command lists × 8,192 words × 4 B = **512 KiB of main RAM**, up from 32 KiB. That is
why 16,384 was rejected — 1 MiB for no gain.

**Verification beyond the benchmark:** `basic_shapes` renders 600 frames with 284 colours and two identical
captures; `cubicmap` loads its three textures; no exceptions anywhere. Restore path walked both ways:
pspdev's library measures 90.287/90.303 ms, the adopted one 61.642/61.630 ms.

## 10. Follow-ups

- The dlist experiment was **not** flat, so the reuse wait sprint 005 saw (2,725 µs/frame at 512 words) was
  list-capacity-side after all. Worth re-measuring with the profiler at 8192 to see what is left.
- 512 KiB of RAM for command lists is a real budget item at 24 MB. If a scene ever needs that memory back, drop
  to 4096 (−21.7 % instead of −24.1 %, at a quarter of the RAM).
- A personal fork is a dependency: record how to notice upstream moving (`pspdev/pspgl` HEAD was base + 2 CI
  commits) and whether any of the 11 commits land upstream later.
- If pspdev ever ships a newer `pspgl` package, the sha check in the script is how we notice it replacing our
  adopted library.
