# Sprint 005 — pspGL from source, and an A/B that means something

| | |
|---|---|
| Roadmap phase | [Phase 3 — pspGL upgrade A/B](../phases/03-pspgl-upgrade.md) |
| Branch / base commit | `master` @ `bb6da92c` (Phase 2 closed; baselines published) |
| Started / closed | 2026-11-20 / 2026-11-20 |
| Build number(s) | `g0` (control build), `g1` (fork build) |
| Tasks | 7/7 done |

## 0. Handover

**Situation.** Everything measured so far runs on the pspGL that pspdev ships, built from `de4260ad`
(2022-03-23). `TheMrIron2/pspgl` carries **11 commits on top of that exact commit**, including aggregate
profiling, configurable command-list capacity and corrected profiling accounting. `pspdev/pspgl`'s own HEAD is
only base + 2 commits, both CI/build, so the fork is where the work is.

Your job: build both from source with identical flags and measure whether the 11 commits are worth anything —
with a control arm that proves the build recipe itself is not the variable.

**Read before acting:**

1. [Baseline numbers](../baselines.md) — the reference you are comparing against (median 90.29 ms at 1,000
   bunnies, repeatability floor 0.02 %, reportable threshold **0.1 %**).
2. [Memory map](../memory-map.md) §6 (measurement protocol), §7 (pspGL provenance, and the licence table).
3. [Phase 3 page](../phases/03-pspgl-upgrade.md).

**Verified facts about the build (both arms already built once, 2026-11-20):**

- pspGL builds with a plain `make` in the tree root. `PSPPATH` and `PSPSDK` come from `psp-config`.
- Flags are fixed in its Makefile: `-g -Wall -Wmissing-prototypes -Os -G0 -fsingle-precision-constant`,
  plus `-DPSPGL_PROFILE=$(PSPGL_PROFILE)` and `-DPSPGL_DLIST_SIZE_WORDS=$(PSPGL_DLIST_SIZE_WORDS)`.
- Defaults: `PSPGL_PROFILE=0`, `PSPGL_DLIST_SIZE_WORDS=512`.
- **Both arms build clean with this toolchain**: `de4260ad` → `libGL.a` 1,634,950 B; fork HEAD `ae9a668` →
  1,646,714 B (it contains `pspgl_profile.o` even at `PROFILE=0`).
- **The shipped `libGL.a` is 315,198 B — 5× smaller.** Cause unknown (pspdev's package strips, or builds
  without `-g`) and irrelevant: **compare behaviour, never bytes.** If you find yourself investigating the size
  difference, stop.
- The two trees are **separate shallow clones pinned by commit**. `de4260ad` is not reachable in the fork's
  clone, so both arms cannot be built from one tree.
- A config stamp keys the objects to `PROFILE`/`DLIST_SIZE` (`.deps/config-*.stamp`), so **`make clean` between
  arms is mandatory**, not tidiness.
- The fork exposes `pspgl_profile.h` (`struct pspgl_profile_stats`, `pspgl_profile_reset()`,
  `pspgl_profile_snapshot()`), which is **not** installed into pspdev: the app needs
  `-I/home/psp/ref/pspgl-themiriron2` to include it.
- pspdev's package builds with bare `make` and installs `libGL.a`, `libGLU.a`, `libglut.a` plus `GL/` and
  `GLES/` headers manually.

**First action.** Build the **shipped commit** from source first (`g0`). If `de4260ad` built here does not behave
like the packaged library, the whole comparison is confounded and that has to be settled before the fork is
measured at all.

**Hard rules.**

- Same flags, same toolchain, same console state for every arm. The only intended variable is the source commit.
- Back up the shipped `libGL.a` before the first swap; every arm must be restorable by one `cp`.
- Two runs per arm. Report a delta only if it exceeds **0.1 %**; below that it is noise.
- `reset` before each run; never send `exit` to pspsh; keep the shell attached; the app must not call
  `sceKernelExitGame`.
- This sprint **measures**. Do not adopt anything yet, and do not change raylib or the platform layer.
- **The profiling arm is not an A/B arm.** `PSPGL_PROFILE=1` instruments the library and changes its cost, so the
  three-arm comparison runs `PROFILE=0`; profiling is measured separately and its numbers are never subtracted
  from the A/B.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cp $PSPDEV/psp/lib/libGL.a /home/psp/libGL-pspdev-shipped.a.bak

cd /home/psp/ref/pspgl-pspdev && git checkout -q de4260ad && make clean && make
cp libGL.a $PSPDEV/psp/lib/libGL.a            # arm: control build

cd /home/psp/ref/pspgl-themiriron2 && make clean && make
cp libGL.a $PSPDEV/psp/lib/libGL.a            # arm: fork HEAD
```

A raylib app must be relinked after a `libGL.a` swap — the samples and `psp-tests/*` link statically.

**Do not touch.** `src/` of raylib, the platform layer, the harness, the corpus.

**Done means** §8: a three-arm table at two counts, a symbol comparison, and a written decision input.

## 1. Objective

Three measured arms on the same workload — shipped package, `de4260ad` built here, fork HEAD built here — with
the control arm proving the build recipe is neutral, plus the fork's internal profiling counters captured. The
output is evidence for a decision, not the decision.

## 2. Why now

Phase 2 established that the wall is per-sprite submission at 86.5 µs each. Before writing a batcher (phase 4),
find out whether the library underneath is already leaving something on the table: 11 commits of profiling and
command-list work exist and cost nothing to try.

## 3. Scope

**In:** building both trees, symbol/header comparison, the three-arm A/B, the fork's profiling counters.

**Out:** adopting anything (sprint 006), raylib changes, the native backend, `-O` experiments beyond recording
the flags used.

## 4. Preconditions

- [ ] Phase 2 closed; [baselines](../baselines.md) published
- [ ] `psp-tests/bunnymark` builds at `BUNNY_COUNT=1000 FRAMES=100 TARGET_FPS=0`
- [ ] Shipped `libGL.a` backed up, sha256 recorded
- [ ] Both pspGL trees present at `/home/psp/ref/pspgl-pspdev` and `/home/psp/ref/pspgl-themiriron2`
- [ ] PSPLink up, `reset` gives 25,149,440 B free

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | build `de4260ad` from source (`g0`) | `done` | builds clean (0 errors), 1,634,950 B; exports **287 symbols, identical set** to the shipped 315,198 B library | `/tmp/sym-control.txt` |
| T2 | build fork HEAD `ae9a668` (`g1`) | `done` | builds clean, 1,646,714 B; **293 symbols** | `/tmp/sym-fork.txt` |
| T3 | symbol comparison | `done` | superset confirmed: **nothing removed**, 6 added (`pspgl_profile_reset/snapshot`, `glDepthRangeRaw`, `eglGetPresentedFrameBufferPSP`, two dlist internals) | §7 |
| T4 | A/B/C at N=1000 | `done` | shipped 90.285/90.286, control 90.308/**90.294** (+0.03 %), fork **81.247/81.234 (−10.03 %)** | §7 |
| T5 | A/B/C at N=5000 | `done` | control 436.448/436.408, fork **392.086/392.033 (−10.17 %)** — consistent | §7 |
| T6 | profiling arm | `done` | 257 submissions/frame for 1,025 primitives (~4 each, cap 512 words); **2,725 µs/frame command-list reuse wait**; binds 2,050/frame, zero redundant. `draw_calls` reads 0 — these counters cover the array paths, not `glBegin`; submissions is the immediate-path counter | §7 |
| T7 | record for the decision | `done` | verdict: adopt (10 % ≫ 0.1 %), superset, BSD-3 | this page |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- 2026-11-20 — T1/T2 `done`: both arms build clean; shipped libGL.a is 315,198 B, local builds ~1.63 MB
  (cause not investigated, by rule)
- 2026-11-20 — T3 `done`: control exports exactly the shipped 287 symbols; fork exports those plus 6
- 2026-11-20 — T4/T5 `done`: control within 0.03 % of shipped; fork −10.03 % at N=1000 and −10.17 % at N=5000
- 2026-11-20 — T6 `done`: profile build reveals 257 submissions/frame and 2,725 µs/frame of command-list wait;
  the profiling build itself is 18.6 % slower, which is why it is not an A/B arm
- 2026-11-20 — T7 `done`; machine left on the fork at PROFILE=0

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| runs per arm | 2 minimum | | |
| reportable delta | > 0.1 % (baseline floor 0.02 %) | | |
| `src/` changes | 0 | | |

## 7. Measurements

Three arms, `TARGET_FPS=0`, median frame time:

| arm | N=1000 run A | N=1000 run B | N=5000 run A | N=5000 run B |
|---|---|---|---|---|
| shipped package | 90.285 | 90.286 | 436.422 | 436.453 |
| `de4260ad` built here (control) | 90.308 | 90.294 | 436.448 | 436.408 |
| fork HEAD built here | **81.247** | **81.234** | **392.086** | **392.033** |

Deltas against the control arm:

| arm | Δ N=1000 | Δ N=5000 | exceeds 0.1 %? |
|---|---|---|---|
| shipped | −0.02 % | −0.006 % | no — recipe is neutral |
| fork HEAD | **−10.03 %** | **−10.17 %** | **yes, by 100×** |

pspGL-internal counters (`PSPGL_PROFILE=1`), per frame at N=1000:

Per frame (100 frames, N=1000, profiling build at 96.4 ms/frame):

| counter | per frame | note |
|---|---|---|
| `command_words` | 16,415 | |
| `command_list_submissions` | **257** | for 1,025 primitives, so ~4 primitives per submission at the 512-word cap |
| `command_list_rollovers` | 0 | |
| `command_list_capacity_words` | 512 | the default |
| `command_list_reuse_sync_wait_us` | **2,725 µs** | the GE wait sprint 004 could not isolate; ~2.8 % of the frame |
| max single wait | 126 µs | |
| `texture_bind_calls` | 2,050 | bind + unbind per primitive, matches our `--wrap` count |
| `texture_bind_redundant` | 0 | nothing to skip: every bind follows an unbind |
| `queue_waits` | 257 | one per submission |
| `draw_calls` | **0** | these counters cover the array paths, not `glBegin` — submissions is the immediate-path counter |

## 8. Exit criteria

- [ ] all three arms built with identical flags and measured twice at two counts
- [ ] control arm confirms the build recipe is neutral, or the confounding is documented
- [ ] symbol comparison recorded — the fork exports a superset, or the difference is listed
- [ ] the fork's internal counters captured at least once
- [ ] a verdict stated against the 0.1 % threshold, and what it implies for sprint 006

## 9. Result

**The 11 commits are worth 10 %, and the control arm says the measurement is real.**

| arm | N=1000 | N=5000 | vs control |
|---|---|---|---|
| shipped | 90.285 | 436.422 | −0.02 % / −0.006 % |
| control (`de4260ad` built here) | 90.301 | 436.428 | — |
| fork HEAD | **81.240** | **392.060** | **−10.03 % / −10.17 %** |

The control arm matching the shipped package within 0.03 % is what makes the rest trustworthy: the build recipe
is not the variable, so the 10 % is the code.

Marginal cost per sprite drops from 86.5 to **77.7 µs**, which moves the 60 fps budget from ~181 to **~215
sprites** in a 16.7 ms frame.

The fork is a strict superset — 287 symbols identical plus 6 added, nothing removed.

**What the profiling build showed** (and it is the first time we could see inside pspGL):
- **257 command-list submissions per frame** for 1,025 primitives — about 4 primitives each, at the default
  512-word list.
- **2,725 µs/frame of command-list reuse wait**, i.e. ~2.8 % of the frame. This is the wait sprint 004 could
  not locate: it is not in `eglSwapBuffers`, it is in list reuse.
- 2,050 texture bind calls per frame with **zero redundant** — every primitive binds after the previous unbind,
  so there is nothing for a "skip duplicate bind" optimisation to save yet.
- `draw_calls` reads **0**: the fork's draw counters cover the array paths, not `glBegin`. Submissions is the
  counter that sees the immediate path.

**Verdict for sprint 006: adopt.** 10 % for a library swap, superset ABI, BSD-3, and it hands us an
instrumentation surface we did not have.

One caveat to carry: the profiling build is **18.6 % slower** than the same library without it (96.4 vs 81.2
ms), which is exactly why it was kept out of the A/B arms.

## 10. Follow-ups

- The fork's `PSPGL_DLIST_SIZE_WORDS` is now clearly worth testing: 257 submissions/frame at a 512-word cap,
  each with a wait. 512 / 1024 / 4096 belongs to sprint 006.
- If the profiling build shows GE waits dominating, that reframes the CPU-vs-GE question from sprint 004.
- Whatever is adopted must be reproducible; a hand-built `libGL.a` with no recorded recipe is not adoptable.
