# Sprint 005 — pspGL from source, and an A/B that means something

| | |
|---|---|
| Roadmap phase | [Phase 3 — pspGL upgrade A/B](../phases/03-pspgl-upgrade.md) |
| Branch / base commit | `master` @ `bb6da92c` (Phase 2 closed; baselines published) |
| Started / closed | — / — |
| Build number(s) | `g0` (control build), `g1` (fork build) |
| Tasks | 0/7 done |

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
| T1 | build `de4260ad` from source with the tree's own flags (`g0`), install it | `todo` | builds clean; `libGL.a` is not byte-identical to pspdev's (different compiler run) but exports the same symbol set | — |
| T2 | build `TheMrIron2` HEAD with identical flags (`g1`) | `todo` | builds clean; record any new/removed exported symbols against T1 | — |
| T3 | symbol and header comparison | `todo` | `psp-nm --defined-only` diff: fork must export a superset; `GL/` and `GLES/` headers diffed, differences listed | — |
| T4 | A/B/C on `bunnymark` N=1000, `TARGET_FPS=0`, two runs per arm | `todo` | shipped vs control-build within 0.1 % (if not, the recipe is the variable — stop and investigate); fork vs control the real number | — |
| T5 | repeat the A/B at N=5000, two runs per arm | `todo` | delta direction consistent with N=1000, or the inconsistency explained | — |
| T6 | profiling arm: fork built with `make PSPGL_PROFILE=1`, app includes `pspgl_profile.h` (`-I` the fork tree), `pspgl_profile_reset()` before the loop, `pspgl_profile_snapshot()` at `DONE`, log the counters that matter: `draw_calls`, `vertices_submitted`, `command_words`, `command_list_submissions`, `command_list_rollovers`, `command_list_capacity_words`, `command_list_reuse_sync_wait_us`, `texture_bind_calls`, `texture_bind_redundant`, `explicit_flushes`, `finishes`, `queue_waits` | `todo` | counters are non-zero and `draw_calls` agrees with our `--wrap` `glEnd` count; **`command_list_reuse_sync_wait_us` is the GE wait time sprint 004 could not separate** | — |
| T7 | write the tables into the phase page and record the decision input | `todo` | numbers and threshold verdict in one place | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- _(not started)_

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
| shipped package | | | | |
| `de4260ad` built here (control) | | | | |
| fork HEAD built here | | | | |

Deltas against the control arm:

| arm | Δ N=1000 | Δ N=5000 | exceeds 0.1 %? |
|---|---|---|---|
| shipped | | | |
| fork HEAD | | | |

pspGL-internal counters (`PSPGL_PROFILE=1`), per frame at N=1000:

| counter | value | note |
|---|---|---|
| command words | | |
| submissions / rollovers | | |
| GE waits | | |

## 8. Exit criteria

- [ ] all three arms built with identical flags and measured twice at two counts
- [ ] control arm confirms the build recipe is neutral, or the confounding is documented
- [ ] symbol comparison recorded — the fork exports a superset, or the difference is listed
- [ ] the fork's internal counters captured at least once
- [ ] a verdict stated against the 0.1 % threshold, and what it implies for sprint 006

## 9. Result

(To fill in on close. A flat result is a real result: if the 11 commits move nothing measurable on this
workload, the honest conclusion is that pspGL's own cost is not where the 86.5 µs/sprite goes, and phase 4 is
the only lever — which is worth knowing before spending a phase on it.)

## 10. Follow-ups

- The fork's `PSPGL_DLIST_SIZE_WORDS` is a separate experiment (512 / 1024 / 4096) and belongs to sprint 006 if
  adoption is on the table.
- If the profiling build shows GE waits dominating, that reframes the CPU-vs-GE question from sprint 004.
- Whatever is adopted must be reproducible; a hand-built `libGL.a` with no recorded recipe is not adoptable.
