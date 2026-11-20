# Sprint 003 — Backend counters that mean something

| | |
|---|---|
| Roadmap phase | [Phase 2 — Instrumentation](../phases/02-instrumentation.md) |
| Branch / base commit | `master` @ `b7b11cdb` (6.5-dev + port, device-validated) |
| Started / closed | — / — |
| Build number(s) | `c1`, `c2` |
| Tasks | 0/7 done |

## 0. Handover

**Situation.** raylib 6.5-dev renders on the PSP through pspGL on the GL11 path. That path is immediate mode:
`DrawTexturePro` binds a texture, issues one quad, unbinds — one `rlBegin`/`rlEnd` per sprite, per glyph, per
shape. Nothing in the tree counts any of it, and bunnymark's own "batched draw calls" figure is computed from
`1 + count/MAX_BATCH_ELEMENTS` on a path that does not batch, so it is fiction. Your job: counters, proven
correct against a workload whose answer is known in advance.

**Read before acting:**

1. [Memory map](../memory-map.md) §2 (why `rlBegin` *is* `glBegin` on this path), §4 (the two separate costs
   this instrumentation must later tell apart), §6 (measurement protocol — four traps, all paid for).
2. [Phase 2 page](../phases/02-instrumentation.md).
3. Sprint 002 §9 — why a noisy capture is not evidence of anything.

**Current state, verified 2026-11-20:**

- Staged library `sha256 13506883…`, headers `RAYLIB_VERSION "6.5-dev"`, both device-validated.
- `psp-tests/basic_shapes` is the model to copy: bounded run, logs `DONE frames N`, calls
  `sceKernelSleepThread()` so the host keeps control.
- The corpus samples are **not** usable for this: they need CROSS presses and exit on START. Nothing in this
  sprint may depend on a physical button press.

**First action.** T1: make the workload controllable. Without a fixed bunny count there is nothing to compare a
counter against.

**Hard rules** (all measured this session):

- Never send `exit` to pspsh — it shuts PSPLink down. Let the pipe close on EOF.
- Keep the shell attached for the whole run, or `TRACELOG` output is lost.
- The test must not call `sceKernelExitGame`: log `DONE`, then `sceKernelSleepThread()`; the host does `reset`.
- `PSP_LARGE_MEMORY=0` on every build.
- No `scrshot` while the app renders — it freezes the app and can return a torn frame. Two captures that are
  identical are the only trustworthy image.
- **A counter that cannot be falsified is not instrumentation.** T4 and T5 exist for exactly that.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/src && make PLATFORM=PLATFORM_PSP SUPPORT_PSP_COUNTERS=1
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd /home/psp/raylib-psp/psp-tests/bunnymark && make PSP_LARGE_MEMORY=0 BUNNY_COUNT=1000
( printf './raylib.prx\n'; sleep 20 ) | pspsh
pspsh -e "reset"
```

**Do not touch.** The renderer's behaviour, the platform layer, and anything in `raylib4Psp`. Counters observe;
they must not change what is drawn.

**Done means** §8: the 1000-sprite check lands, the mutation test drops the right counter, and the overhead of
having counters compiled in is measured rather than assumed.

## 1. Objective

Counters for primitives submitted, vertices, texture binds and unbinds exist behind a compile-time flag; they
are **proven correct** by two independent tests — a known workload (1000 sprites → ~1000 primitives) and a
mutation (draws disabled → primitives fall to the header/text count, logical sprites unchanged).

## 2. Why now

Phases 3 and 4 both claim to reduce submission count. Without a counter validated against a workload whose
answer is known in advance, those claims cannot be evaluated — and the first "improvement" would be
indistinguishable from the counter changing. bunnymark's existing figure is exactly that failure, shipped in
someone else's sample.

## 3. Scope

**In:** a controllable bunnymark in `psp-tests/`, counters in rlgl's GL11 path, read-out, the two validation
tests, the overhead measurement.

**Out:** timing instrumentation (sprint 004), any rendering change, pspGL modification (phase 3), batching
(phase 4).

## 4. Preconditions

- [ ] PSPLink up (`054c:01c9`), `reset` gives 25,149,440 B free on partition 2
- [ ] Staged library + headers are the validated 6.5-dev build
- [ ] `psp-tests/basic_shapes` still reaches `DONE frames 600` (proves the loop works before you change it)
- [ ] `mkdir -p /home/psp/evidence/sprint003`

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | port bunnymark into `psp-tests/bunnymark`: bunny count from `-DBUNNY_COUNT=<n>`, spawn at startup, `DONE` + sleep, no CROSS dependency | `todo` | builds; run with 1000 logs `DONE`; module resident afterwards | — |
| T2 | counters behind `SUPPORT_PSP_COUNTERS` in rlgl's GL11 path: `rlBegin` per mode, `rlEnd`, vertices (`rlVertex2i/2f`), texture binds (`rlSetTexture != 0`), unbinds (`== 0`); reachable from the test app (include `rlgl.h`) | `todo` | flag off: byte-identical PRX to the previous build (proves zero cost when off); flag on: counters change while the screen does not | — |
| T3 | read-out: one counter line per second to the log, plus a final line at `DONE` | `todo` | `FRAME …` and `COUNT …` lines interleave as expected | — |
| T4 | **falsifiable check**: at N=1000, primitives ≈ 1000 and vertices ≈ 4N + header/text, within a documented tolerance | `todo` | numbers land in tolerance, or the gap is explained | — |
| T5 | **mutation test**: skip the bunny draw loop, rebuild, run — primitives must fall to the header/text count while the *logical* sprite count stays 1000 | `todo` | both numbers recorded side by side | — |
| T6 | overhead: same workload with counters on and off, two runs each | `todo` | delta under 0.2 ms/frame, or recorded as higher and accepted | — |
| T7 | write the results into [§2](../memory-map.md#2-memory-map-where-the-work-happens)/§4 and record the **1:1 assumption** (one `rlEnd` = one pspGL submission) as an assumption, not a fact | `todo` | memory map updated in the same commit | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- _(not started)_

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| counter overhead | < 0.2 ms per frame | | |
| counters per frame | ≤ 8 | | |
| PRX with flag off | byte-identical to before | | |

## 7. Measurements

Known-workload check (`c2`, GL11, unbatched):

| N bunnies | primitives | vertices | binds | unbinds | expected |
|---|---|---|---|---|---|
| 100 | | | | | ~100 prims, ~400 vertices |
| 1,000 | | | | | ~1,000 prims, ~4,000 vertices |
| 5,000 | | | | | ~5,000 prims, ~20,000 vertices |

Mutation test (`c2`, N=1000, draw loop skipped):

| counter | expect |
|---|---|
| primitives | header/text only (single digits) |
| logical sprites | 1000, unchanged |

Overhead (`c2` vs `c1`, N=1000, two runs each):

| build | frame time | note |
|---|---|---|
| counters off | | |
| counters on | | delta = overhead |

## 8. Exit criteria

- [ ] the 1000-sprite check lands within the documented tolerance, or the discrepancy is explained and written
      down
- [ ] the mutation test shows the counter follows the draw, not the intent
- [ ] counters reach the log and survive to `DONE`
- [ ] overhead measured, not assumed
- [ ] the `rlEnd`→pspGL-submission mapping is labelled an assumption with the phase-3 experiment that would
      confirm it

## 9. Result

(To fill in on close. The most useful outcome would be a negative one: if primitives are *not* ≈ N, rlgl is
already merging something on this path and phase 4's premise changes.)

## 10. Follow-ups

- Timing (CPU per zone, GE wait) is sprint 004 — keeping counting and timing separate keeps each measurable.
- If the counters had to live inside pspGL to be trusted, that drags phase 3 earlier; record it if so.
- The corpus samples still exit on START and need CROSS; only `psp-tests/` is host-driven.
