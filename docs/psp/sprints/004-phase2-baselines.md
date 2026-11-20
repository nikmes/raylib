# Sprint 004 — Timing, fixed-count runs, repeatability floor

| | |
|---|---|
| Roadmap phase | [Phase 2 — Instrumentation](../phases/02-instrumentation.md) |
| Branch / base commit | `master` @ `52ee5b1a` (counters closed, 6.5-dev validated) |
| Started / closed | — / — |
| Build number(s) | `t1`, `t2` |
| Tasks | 0/7 done |

## 0. Handover

**Situation.** Sprint 003 shipped a validated submission counter and, as a side effect, a first frame-time
measurement. `psp-tests/bunnymark` already has: `BUNNY_COUNT` and `FRAMES` as build parameters, a bounded run
that logs `DONE` and sleeps, `COUNT` lines from the GL-boundary harness, and an `AVG frames=… avgms=… fps=…`
line. What is missing is **distribution and repeatability**: an average hides the stalls, and nothing yet says
how far two identical runs differ.

**Already measured (sprint 003), do not re-derive:**

| bunnies | frame | fps | `glEnd`/frame |
|---|---|---|---|
| 100 | 16.9 ms | 59 | 130.0 |
| 1,000 | 100.4 ms | 10 | 1,030.9 |
| 5,000 | 470.4 ms | 2.1 | 5,030.0 |

That is 92.5 µs of marginal cost per sprite. **The 100-bunny row is vsync-capped**, not a cost — which is the
first thing this sprint has to settle.

**Read before acting:**

1. [Memory map](../memory-map.md) §6 — measurement protocol, including the capture and PSPLink traps.
2. [Memory map](../memory-map.md) §4 — the measured cost table and why CPU-vs-GE matters.
3. Sprint 003 §9 — what counters do and do not tell you.

**First action.** Find out whether `eglSwapBuffers` blocks. Run N=10 with `TARGET_FPS=0` twice: once as built,
once with `eglSwapInterval(display, 0)`. If the frame time stays ~16.7 ms the swap is paced by the display and
every throughput number needs the uncapped path; if it drops, the swap was never the limit.

**Hard rules.**

- **Device only.** PPSSPP answers "does it render", never "how fast".
- `reset` before every session; `kill` leaks the module's heap.
- Never send `exit` to pspsh; keep the shell attached for the whole run.
- No `scrshot` during a sweep — it freezes the app.
- Two runs per count, same binary, same session. One run per arm separates nothing.
- **A change to `src/` is allowed in this sprint only as the guarded vsync switch** (T1). Default behaviour must
  be unchanged when the define is absent; anything else in `src/` is out of scope.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/src && make PLATFORM=PLATFORM_PSP
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd /home/psp/raylib-psp/psp-tests/bunnymark && make PSP_LARGE_MEMORY=0 \
    BUNNY_COUNT=1000 FRAMES=300 TARGET_FPS=0
( printf './bunnymark.prx\n'; sleep 40 ) | pspsh
pspsh -e "reset"
```

**Done means** §8: percentiles for each count, a repeatability floor stated as a number, an answer to
CPU-vs-GE, and `docs/psp/baselines.md` published.

## 1. Objective

A published baseline: **median / p95 / worst** frame time for bunnymark at 100 / 1,000 / 5,000 / 10,000 bunnies
with `TARGET_FPS=0`, two runs per count, plus the **repeatability floor** (how far two identical runs differ) —
and a measured answer to whether the wall is per-sprite submission or per-pixel fill.

## 2. Why now

Phase 2's exit criterion is this table. Phase 3's pspGL A/B and phase 4's batcher both need a threshold below
which a difference is noise, and phase 4 needs to know that batching is even the right lever: if the wall turns
out to be fill rate, batching N submissions will not move the number.

## 3. Scope

**In:** uncapped measurement (one guarded platform switch), per-frame distribution, the run matrix, the
repeatability floor, a fill-rate experiment, the published baseline.

**Out:** GE-side timing (needs a backend that can fence), pspGL counters (phase 3), any optimisation, the
corpus samples.

## 4. Preconditions

- [ ] Sprint 003 closed (counters validated: 1,030.9 `glEnd`/frame at N=1000)
- [ ] `psp-tests/bunnymark` builds with `BUNNY_COUNT` / `FRAMES`
- [ ] PSPLink up, `reset` gives 25,149,440 B free
- [ ] `cwd` for the app has `textures/bunnymark/wabbit_alpha.png` under the usbhostfs root
- [ ] `mkdir -p /home/psp/evidence/sprint004`
- [ ] Baseline binary archived with sha256 before the first sweep

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | settle vsync: `TARGET_FPS` as a build parameter; `#if defined(PSP_DISABLE_VSYNC) eglSwapInterval(platform.device, 0); #endif` next to the commented line in `src/platforms/rcore_psp.c` | `todo` | N=10 with `TARGET_FPS=0`: frame time with and without the switch; default build behaves exactly as before | — |
| T2 | per-frame distribution in the app: store each frame's time, log `STAT median= p95= worst= min= n=` at `DONE` | `todo` | median ≈ the `AVG` line from sprint 003 on the same workload; worst ≥ p95 ≥ median | — |
| T3 | run matrix with `TARGET_FPS=0`: 100 / 1,000 / 5,000 / 10,000, two runs each | `todo` | 8 runs logged; each ends in `DONE`, no exception | — |
| T4 | repeatability floor: difference between the two runs of the same count, per count, as a % | `todo` | a single number adopted as the "measurable" threshold | — |
| T5 | **fill-rate experiment**: `SPRITE_SCALE` as a build parameter, N=1,000 drawn at 1× (32×32) and 4× (128×128, 16× the pixels) via `DrawTexturePro` with a scaled dest rect — still one primitive per sprite | `todo` | **`glEnd`/frame identical in both builds** (the counters prove the experiment changes fill only); then: unchanged frame time → submission-bound, growth → fill matters and phase 4 must be re-scoped | — |
| T6 | publish `docs/psp/baselines.md`: table, protocol, hardware, build sha, repeatability floor, the fill answer | `todo` | page in the nav, `--strict` clean | — |
| T7 | archive the exact binary + sha256 into `/home/psp/evidence/sprint004/`; record the crossover facts in [§4](../memory-map.md#4-where-the-time-goes-today) | `todo` | sha in the page matches the archived file | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- _(not started)_

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frames per run | ≥ 200 (or 30 s, whichever comes first) | | |
| runs per count | 2, same binary | | |
| `src/` changes | 1 guarded switch in the platform layer | | |

## 7. Measurements

Sweep (`t2`, `TARGET_FPS=0`, two runs per count):

| bunnies | run | median ms | p95 ms | worst ms | `glEnd`/frame | notes |
|---|---|---|---|---|---|---|
| 100 | A | | | | | |
| 100 | B | | | | | |
| 1,000 | A | | | | | |
| 1,000 | B | | | | | |
| 5,000 | A | | | | | |
| 5,000 | B | | | | | |
| 10,000 | A | | | | | expected ~940 ms/frame: note frame count, do not pretend it is a window |
| 10,000 | B | | | | | |

Repeatability floor per count (|A−B| / mean):

| 100 | 1,000 | 5,000 | 10,000 |
|---|---|---|---|
| | | | |

Fill-rate experiment (N=1,000, same submissions, 4× sprite scale):

| build | median ms | note |
|---|---|---|
| 1× (32×32 sprites) | | reference |
| 4× (128×128 sprites) | | 16× the pixels |

## 8. Exit criteria

- [ ] the vsync question is answered, and the sweep is run in the state that actually measures cost
- [ ] percentiles (not just averages) for every count, from two runs
- [ ] repeatability floor stated as one number and used as the threshold for future claims
- [ ] the CPU-vs-GE question answered by the fill experiment, not by intuition
- [ ] `docs/psp/baselines.md` published with build sha256 and protocol
- [ ] Phase 2 exit criteria recorded in the phase page

## 9. Result

(To fill in on close. A negative result is valuable: if 1,000 sprites is fill-bound, phase 4's submission
batching will not show on this workload and the plan changes before any code is written.)

## 10. Follow-ups

- If the crossover between submission-bound and fill-bound sits at a particular sprite size or count, that is
  a memory-map fact, not a note.
- GE-side timing needs a backend that can fence, so it waits for phase 5.
- pspGL-side counters (phase 3) would separate "pspGL's own cost" from "GE's cost" inside the same frame.
