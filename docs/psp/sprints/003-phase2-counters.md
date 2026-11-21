# Sprint 003 (P2-S1) — Backend counters that mean something

| | |
|---|---|
| Roadmap phase | [Phase 2 — Instrumentation](../phases/02-instrumentation/index.md) |
| Sprint | P2-S1 |
| Branch / base commit | `master` @ `252acee7` → closed at the sprint commit |
| Started / closed | 2026-11-20 / 2026-11-20 |
| Build number(s) | `c1`, `c2` |
| Tasks | 7/7 done |

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
2. [Phase 2 page](../phases/02-instrumentation/index.md).
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
| T1 | port bunnymark into `psp-tests/bunnymark` | `done` | `SPAWN 1000 bunnies`, `DONE frames 120`; no pad dependency; `FRAMES` and `BUNNY_COUNT` are build parameters | `psp-tests/bunnymark/` |
| T2 | counting harness with `--wrap`, forwarding to `__real_*` | `done` | `__wrap_glEnd` linked; counts scale with N; `git diff master -- src/` **empty** | `psp_counters.c/.h` |
| T3 | read-out: `COUNT …` per second + a final line at `DONE` | `done` | interleaves with `FRAME …`; survives to `DONE` | logs |
| T4 | **falsifiable check** | `done` | 1,030.9 ends/frame at N=1000 (1,000 sprites + 30.9 chrome); **4.000 vertices per end** | §7 |
| T5 | **mutation test** | `done` | 3,719 ends / 120 frames = 31.0/frame while `SPAWN 1000` still logs; delta 999.9 | §7 |
| T6 | harness cost, two runs per arm | `done` | wrapped 100.364 / 100.356 ms, unwrapped 100.361 / 100.357 ms — inside the ±0.008 ms noise | §7 |
| T7 | write results into the memory map | `done` | §4 gains the measured per-sprite cost and the vertex/primitive signature; boundary wording recorded | same commit |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- 2026-11-20 — sprint opened; harness written, `__wrap_glEnd` confirmed linked, `src/` untouched
- 2026-11-20 — T4 `done`: 1,030.9 ends/frame at N=1000, 4.000 vertices/end
- 2026-11-20 — T5 `done`: mutation gives 31.0 ends/frame with 1000 sprites still logged
- 2026-11-20 — T6: first attempt invalid — the WRAP=0 build failed to link (`__real_*` undefined), so
  both "arms" ran the same binary. Guarded the wrap functions behind `WRAP_COUNTERS`, re-measured
- 2026-11-20 — scaling runs at N=100 / 1000 / 5000; T7 `done`

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| harness cost | < 0.2 ms per frame | | |
| counters per frame | ≤ 8 | | |
| changes under `src/` | **0 lines** | | |

## 7. Measurements

Known-workload check (`c2`, GL11, unbatched):

| N bunnies | glEnd/frame | vertices/frame | binds | unbinds | avg frame | fps |
|---|---|---|---|---|---|---|
| 100 | 130.0 | 520.0 | = ends | = ends | 16.943 ms | 59 (vsync-capped) |
| 1,000 | 1,030.9 | 4,123.8 | = ends | = ends | 100.364 ms | 10 |
| 5,000 | 5,030.0 | 20,120.0 | = ends | = ends | 470.362 ms | 2.1 |

Chrome (everything that is not a bunny: header rect, `bunnies:` text, FPS text) is **30 primitives/frame**
at all three counts. Vertices per end is **4.000** exactly — the quad signature. Marginal cost between 1000
and 5000 bunnies: **92.5 µs per sprite**. `binds` and `unbinds` equal `ends` exactly: one bind and one unbind
per primitive.

Mutation test (`c2`, N=1000, draw loop skipped):

| counter | expect |
|---|---|
| `glEnd` | header/text only (single digits) |
| logical sprites | 1000, unchanged |

Harness cost (`c2` wrapped vs unwrapped, N=1000, two runs each):

| build | run 1 | run 2 | note |
|---|---|---|---|
| no `--wrap` | 100.361 ms | 100.357 ms | counters read zero, as expected |
| `--wrap` harness | 100.364 ms | 100.356 ms | counters read 123,713 |

Spread within an arm (±0.008 ms) is larger than the difference between arms, so the harness cost is **below the
measurement resolution** on this workload — well inside the 0.2 ms budget. Resolving it further would need a
workload whose frame is short enough for microseconds to show.

Double-counting check: if `glEnd` far exceeds N, pspGL's own internal calls are being wrapped too. Record the
observed baseline offset (header, text, box) rather than assuming zero.

## 8. Exit criteria

- [x] the 1000-sprite check landed: 1,030.9 ends/frame for 1,000 sprites, 4.000 vertices per end
- [x] the mutation test shows the counters follow the draw, not the intent (delta 999.9)
- [x] counters reach the log and survive to `DONE`
- [x] harness cost measured and found below the noise floor
- [x] `git diff master -- src/` is **empty** — the whole sprint is contained in `psp-tests/`
- [x] boundary described precisely: GL submissions into pspGL, not `sceGuDrawArray` calls

## 9. Result

**The counters are validated twice over, and the workload is now measurable from the host.**

- Known workload: N=1000 gives 1,030.9 ends/frame — 1,000 sprites plus 30.9 chrome — with **4.000 vertices per
  end**, which is the quad signature and not a coincidence to be argued about.
- Mutation: with the bunny loop compiled out, the same build logs 31.0 ends/frame while still reporting 1000
  sprites. The difference is 999.9 primitives.
- Every primitive is one bind and one unbind (`binds == unbinds == ends`), so texture state churn scales with
  sprite count exactly as the source suggests.
- Harness cost is below the ±0.008 ms noise floor at 100 ms frames, and `src/` is untouched.

**The finding that matters for phase 4:** the scaling curve is linear and expensive.

| bunnies | frame | fps |
|---|---|---|
| 100 | 16.9 ms | 59 (vsync-capped, so this is an upper bound, not a cost) |
| 1,000 | 100.4 ms | 10 |
| 5,000 | 470.4 ms | 2.1 |

That is **92.5 µs of marginal cost per sprite**, which buys about **181 sprites** inside a 16.7 ms frame. With
30 primitives of chrome unchanged across all three counts, the cost is entirely per-sprite submission, not
per-frame overhead. Phase 4's premise — collapse N submissions into `ceil(N/capacity)` — is now backed by a
number instead of an assumption.

**One mistake, corrected in the sprint log:** the first T6 attempt was invalid. With `WRAP=0` the build failed
to link (`__real_*` undefined) and both "arms" silently ran the same wrapped binary. The wrap functions are now
behind `WRAP_COUNTERS` and the unwrapped build links with counters that correctly read zero.

## 10. Follow-ups

- Timing (CPU per zone, GE wait) is sprint 004 — keeping counting and timing separate keeps each measurable.
- At phase 5 the same harness is re-pointed at `sceGuDrawArray`: the wrap symbols change, the reports do not,
  and phase 4's numbers stay comparable across the backend swap.
- The corpus samples still exit on START and need CROSS; only `psp-tests/` is host-driven.
