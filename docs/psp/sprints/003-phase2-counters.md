# Sprint 003 — Backend counters that mean something

| | |
|---|---|
| Roadmap phase | [Phase 2 — Instrumentation](../phases/02-instrumentation.md) |
| Branch / base commit | `master` @ `252acee7` (6.5-dev + port, device-validated) |
| Started / closed | — / — |
| Build number(s) | `c1`, `c2` |
| Tasks | 0/7 done |

## 0. Handover

**Situation.** raylib 6.5-dev renders on the PSP through pspGL on the GL11 path. That path is immediate mode:
`DrawTexturePro` binds a texture, issues one quad, unbinds — one `rlBegin`/`rlEnd` per sprite, per glyph, per
shape. Nothing counts any of it, and bunnymark's own "batched draw calls" figure comes from
`1 + count/MAX_BATCH_ELEMENTS` on a path that does not batch, so it is fiction. Your job: counters, proven
correct against a workload whose answer is known in advance.

**Counting lives in the test harness, not in raylib.** Interpose on the GL entry points at link time with
`-Wl,--wrap=<symbol>`. This is deliberate:

- `src/rlgl.h` stays byte-identical to upstream. The plan is to drop OpenGL for a platform-specific rGL, and
  every line we add to rlgl is a line that has to be carried through that change and through every upstream
  merge until then.
- Counting at the GL boundary measures what actually crosses into pspGL, not what raylib intended.
- When the backend changes, the harness is re-pointed at `sceGuDrawArray`/`sceGuGetMemory` and the same
  numbers stay comparable. It is the only instrumentation that survives phase 5.

**Read before acting:**

1. [Memory map](../memory-map.md) §2 (why `rlBegin` *is* `glBegin` on this path), §4 (the two separate costs
   this instrumentation must later tell apart), §6 (measurement protocol — four traps, all paid for).
2. [Phase 2 page](../phases/02-instrumentation.md).
3. Sprint 002 §9 — why a noisy capture is not evidence of anything.

**Current state, verified 2026-11-20:** staged library `sha256 13506883…`, headers `RAYLIB_VERSION "6.5-dev"`,
both device-validated. `psp-tests/basic_shapes` is the pattern to copy: bounded run, `DONE frames N`, then
`sceKernelSleepThread()`. The corpus samples are **not** usable here — they need CROSS presses and exit on START.

**First action.** T1: make the workload controllable. Without a fixed bunny count there is nothing to compare a
counter against.

**Hard rules** (all measured this session):

- **No changes to `src/rlgl.h`, `src/rshapes.c` or pspGL in this sprint.** If counting appears to require it, the
  harness is wrong, not the renderer.
- Never send `exit` to pspsh — it shuts PSPLink down. Let the pipe close on EOF.
- Keep the shell attached for the whole run, or `TRACELOG` output is lost.
- The test must not call `sceKernelExitGame`: log `DONE`, then `sceKernelSleepThread()`; the host does `reset`.
- `PSP_LARGE_MEMORY=0` on every build.
- No `scrshot` while the app renders — it freezes the app and can return a torn frame. Two identical captures
  are the only trustworthy image.
- **A counter that cannot be falsified is not instrumentation.** T4 and T5 exist for exactly that.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/psp-tests/bunnymark && make PSP_LARGE_MEMORY=0 BUNNY_COUNT=1000
( printf './raylib.prx\n'; sleep 20 ) | pspsh
pspsh -e "reset"
```

**Done means** §8: the 1000-sprite check lands, the mutation test drops the right counter, and the harness cost
is measured rather than assumed.

## 1. Objective

A link-time counting harness in `psp-tests/` that records GL submissions crossing into pspGL — `glBegin` per
mode, `glEnd`, vertices, texture binds and unbinds — and is **proven correct** twice: against a known workload
(1000 sprites → ~1000 `glEnd`) and by mutation (draws disabled → submissions fall to the header/text count,
logical sprites unchanged). `src/` is untouched.

## 2. Why now

Phases 3 and 4 both claim to reduce submission count. Without a counter validated against a workload whose
answer is known in advance, those claims cannot be evaluated — and the first "improvement" would be
indistinguishable from the counter changing. bunnymark's existing figure is exactly that failure, shipped
inside someone else's sample.

## 3. Scope

**In:** a controllable bunnymark in `psp-tests/`, the `--wrap` harness, read-out, the two validation tests, the
harness cost measurement.

**Out:** any change under `src/` (this is the sprint's defining constraint), timing instrumentation (sprint
004), pspGL modification (phase 3), batching (phase 4).

## 4. Preconditions

- [ ] PSPLink up (`054c:01c9`), `reset` gives 25,149,440 B free on partition 2
- [ ] Staged library + headers are the validated 6.5-dev build
- [ ] `psp-tests/basic_shapes` still reaches `DONE frames 600`
- [ ] `git status` clean, and `git diff --stat master -- src/` empty at the end of the sprint
- [ ] `mkdir -p /home/psp/evidence/sprint003`

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | port bunnymark into `psp-tests/bunnymark`: bunny count from `-DBUNNY_COUNT=<n>`, spawn at startup, `DONE` + sleep, no CROSS dependency | `todo` | builds; N=1000 logs `DONE`; module resident after | — |
| T2 | counting harness with `-Wl,--wrap=glBegin,glEnd,glVertex2f,glVertex2i,glBindTexture,glDrawArrays,glDrawElements`, forwarding to `__real_*`; counters and a dump function in one small `.c` | `todo` | program links; counts are non-zero and scale with N; `git diff master -- src/` empty | — |
| T3 | read-out: one `COUNT …` line per second plus a final line at `DONE` | `todo` | interleaves with `FRAME …` as expected | — |
| T4 | **falsifiable check**: N=1000 → `glEnd` ≈ 1000, vertices ≈ 4N + header/text, within a documented tolerance | `todo` | numbers land, or the gap is explained | — |
| T5 | **mutation test**: skip the bunny draw loop, rebuild, run — `glEnd` must fall to the header/text count while the *logical* sprite count stays 1000 | `todo` | both numbers recorded side by side | — |
| T6 | harness cost: same workload linked with and without the `--wrap` flags, two runs each | `todo` | delta recorded; under 0.2 ms/frame or accepted with a reason | — |
| T7 | write results into [§2](../memory-map.md#2-memory-map-where-the-work-happens)/§4; record that counting is at the **GL boundary** (one `glEnd` is one submission *into pspGL*, not necessarily one `sceGuDrawArray`) | `todo` | memory map updated in the same commit | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- _(not started)_

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| harness cost | < 0.2 ms per frame | | |
| counters per frame | ≤ 8 | | |
| changes under `src/` | **0 lines** | | |

## 7. Measurements

Known-workload check (`c2`, GL11, unbatched):

| N bunnies | glEnd | vertices | binds | unbinds | expected |
|---|---|---|---|---|---|
| 100 | | | | | ~100 ends, ~400 vertices |
| 1,000 | | | | | ~1,000 ends, ~4,000 vertices |
| 5,000 | | | | | ~5,000 ends, ~20,000 vertices |

Mutation test (`c2`, N=1000, draw loop skipped):

| counter | expect |
|---|---|
| `glEnd` | header/text only (single digits) |
| logical sprites | 1000, unchanged |

Harness cost (`c2` wrapped vs unwrapped, N=1000, two runs each):

| build | frame time | note |
|---|---|---|
| no `--wrap` | | |
| `--wrap` harness | | delta = cost |

Double-counting check: if `glEnd` far exceeds N, pspGL's own internal calls are being wrapped too. Record the
observed baseline offset (header, text, box) rather than assuming zero.

## 8. Exit criteria

- [ ] the 1000-sprite check lands within tolerance, or the discrepancy is explained in writing
- [ ] the mutation test shows the counters follow the draw, not the intent
- [ ] counters reach the log and survive to `DONE`
- [ ] harness cost measured, not assumed
- [ ] `git diff master -- src/` is **empty** — the whole sprint is contained in `psp-tests/`
- [ ] the boundary is described precisely: GL submissions, not `sceGuDrawArray` calls, and the phase-3/5
      experiment that would count one level deeper is named

## 9. Result

(To fill in on close. The most useful outcome would be a negative one: if `glEnd` is *not* ≈ N, something in
pspGL already merges, and phase 4's premise changes.)

## 10. Follow-ups

- Timing (CPU per zone, GE wait) is sprint 004 — keeping counting and timing separate keeps each measurable.
- At phase 5 the same harness is re-pointed at `sceGuDrawArray`: the wrap symbols change, the reports do not,
  and phase 4's numbers stay comparable across the backend swap.
- The corpus samples still exit on START and need CROSS; only `psp-tests/` is host-driven.
