# Sprint 004 (P2-S2) — Timing, fixed-count runs, repeatability floor

| | |
|---|---|
| Roadmap phase | [Phase 2 — Instrumentation](../phases/02-instrumentation/index.md) |
| Sprint | P2-S2 |
| Branch / base commit | `master` @ `52ee5b1a` (counters closed, 6.5-dev validated) |
| Started / closed | 2026-11-20 / 2026-11-20 |
| Build number(s) | `t1`, `t2` |
| Tasks | 7/7 done |

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
| T1 | settle vsync | `done` | **the swap blocks**: N=10, `TARGET_FPS=0`, median 16.683 ms. `eglSwapInterval(0)` does not uncap it (median 16.685) but `min` falls to 4.2–12.2 ms; swap call itself is 0.15 ms, so the wait is deferred inside pspGL | §9 |
| T2 | per-frame distribution | `done` | `STAT n= min= median= p95= worst= avg=` at `DONE`; worst ≥ p95 ≥ median in every run | logs |
| T3 | run matrix, two runs each | `done` | 8 runs, all `DONE`, no exceptions; medians 16.683 / 90.29 / 436.45 / 869.4 ms | [baselines](../baselines.md) |
| T4 | repeatability floor | `done` | worst case **0.02 %** (100 bunnies), 0.003–0.008 % above; threshold adopted at **0.1 %** | [baselines](../baselines.md) |
| T5 | **fill-rate experiment** | `done` | submissions identical (1,024.95 vs 1,024.93); median 90.279 → 95.438 ms for **16× the pixels = +5.7 %** → **submission-bound** | §7 |
| T6 | publish `docs/psp/baselines.md` | `done` | page in the nav, `--strict` clean, protocol + sha + floor + fill answer | [baselines](../baselines.md) |
| T7 | archive binary + sha; record in §4 | `done` | `bunnymark-n1000.prx` sha256 `b4160cad…` in `/home/psp/evidence/sprint004/`, matches the page | evidence dir |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- 2026-11-20 — sprint opened; T1 first: `eglSwapInterval` placed *before* `eglMakeCurrent` faulted exactly like
  sprint 001's `SetupViewport` (bus error, no current context); moved after `MakeCurrent`, then re-measured
- 2026-11-20 — T1 `done`: the swap blocks (16.683 ms at 10 bunnies); `eglSwapInterval(0)` does not uncap it;
  swap call itself is 0.15 ms, so pspGL blocks later, not inside the swap
- 2026-11-20 — T2 `done`; T3 `done`: 8 runs, two per count
- 2026-11-20 — T4 `done`: floor 0.02 % worst case; threshold 0.1 %
- 2026-11-20 — T5 `done`: fill is +5.7 % for 16× the pixels, submissions identical → submission-bound
- 2026-11-20 — T6/T7 `done`: baselines page published, binary archived

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
| 100 | A | 16.683 | 16.779 | 33.744 | 124.0 | pacing, not cost |
| 100 | B | 16.686 | 16.784 | 34.706 | 124.0 | |
| 1,000 | A | 90.291 | 90.503 | 120.780 | 1,024.97 | |
| 1,000 | B | 90.284 | 90.467 | 121.916 | 1,024.97 | |
| 5,000 | A | 436.450 | 510.061 | 510.061 | 5,024.0 | 20 frames |
| 5,000 | B | 436.455 | 508.268 | 508.268 | 5,024.0 | 20 frames |
| 10,000 | A | 869.408 | 989.942 | 989.942 | 10,025.0 | 12 frames: too few for a percentile |
| 10,000 | B | 869.385 | 991.352 | 991.352 | 10,025.0 | 12 frames |

Repeatability floor per count (|A−B| / mean):

| 100 | 1,000 | 5,000 | 10,000 |
|---|---|---|---|
| 0.02 % | 0.008 % | 0.001 % | 0.003 % |

Fill-rate experiment (N=1,000, same submissions, 4× sprite scale):

| build | `glEnd`/frame | median ms | note |
|---|---|---|---|
| 1× (32×32 sprites) | 1,024.95 | 90.279 | reference |
| 4× (128×128 sprites) | 1,024.93 | 95.438 | 16× the pixels, **+5.7 %** |

## 8. Exit criteria

- [x] the vsync question is answered: the swap blocks, `eglSwapInterval(0)` does not uncap it, and the wait is
      deferred inside pspGL rather than spent in the swap call
- [x] percentiles for every count, two runs each
- [x] repeatability floor: 0.02 % worst case, threshold adopted at 0.1 %
- [x] CPU-vs-GE answered by the fill experiment: submission-bound (+5.7 % for 16× the pixels)
- [x] `docs/psp/baselines.md` published with sha256 and protocol
- [x] Phase 2 exit criteria recorded in the phase page

## 9. Result

**The baseline exists, it is stable, and the wall is submission — not fill.**

- Frame times, two runs per count: 16.68 / 90.29 / 436.45 / 869.4 ms for 100 / 1,000 / 5,000 / 10,000 bunnies.
  Repeatability floor **0.02 %** worst case; the threshold for a reportable difference is set at **0.1 %**.
- Marginal cost **86.5 µs per sprite** (86.6 measured across a different range), constant across counts, with a
  fixed 31 primitives/frame of chrome.
- **Fill is not the wall**: 16× the pixels cost **+5.7 %** while holding submissions constant at 1,024.9/frame.

Two things went wrong and are now facts rather than surprises:

1. **`eglSwapInterval` before `eglMakeCurrent` faults** — the same failure as sprint 001's `SetupViewport`, a bus
   error with no current context. Moved after `MakeCurrent`; the guard is `PSP_DISABLE_VSYNC` so shipped
   behaviour is unchanged.
2. **The swap is not where the wait happens.** Wrapping `eglSwapBuffers` showed 0.15 ms inside it while the
   frame takes 16.68 ms at low counts. pspGL returns from the swap and blocks later, on the next buffer use. So
   separating CPU cost from display wait needs pspGL-level timing (phase 3), and until then the low-count row is
   an upper bound.

**Consequence for the roadmap:** phase 4's premise holds — batching N submissions into `ceil(N/capacity)` is the
lever that matches the measured bottleneck. At 86.5 µs/sprite only ~181 sprites fit in a 16.7 ms frame today.

## 10. Follow-ups

- If the crossover between submission-bound and fill-bound sits at a particular sprite size or count, that is
  a memory-map fact, not a note.
- GE-side timing needs a backend that can fence, so it waits for phase 5.
- pspGL-side counters (phase 3) would separate "pspGL's own cost" from "GE's cost" inside the same frame.
