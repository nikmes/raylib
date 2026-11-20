# Baseline: bunnymark on a PSP-1000

Frame times for the workload phases 3–5 are measured against: one `DrawTexturePro` per sprite, no batching, GL11
through pspGL. This is the reference every later claim is compared to.

Established by [sprint 004](sprints/004-phase2-baselines.md). Re-measure with the same protocol before comparing
anything to it.

## Protocol

| | |
|---|---|
| Console | PSP-1000, 32 MB, firmware 6.60, ARK CFW |
| Build | `master` @ `711b5d9c`, release library `d72390f4…`, app `bunnymark` |
| App | `psp-tests/bunnymark`, `TARGET_FPS=0`, sprites spawned at startup, bounded run |
| Load | `psp-tests/bunnymark` over PSPLink, `reset` before each session |
| Measurement | `GetFrameTime()` per frame, sorted at `DONE`; percentiles over the run |
| Binary | `/home/psp/evidence/sprint004/bunnymark-n1000.prx`, sha256 `b4160cad…` |

## Frame time

Two runs per count, same binary, back to back.

| bunnies | `glEnd`/frame | median A | median B | p95 | worst | fps (median) |
|---|---|---|---|---|---|---|
| 100 | 124.0 | 16.683 ms | 16.686 ms | 16.78 | 33.7 | 59.9 |
| 1,000 | 1,024.97 | 90.291 ms | 90.284 ms | 90.50 | 120.8 | 11.1 |
| 5,000 | 5,024.0 | 436.450 ms | 436.455 ms | 510.1 | 510.1 | 2.3 |
| 10,000 | 10,025.0 | 869.408 ms | 869.385 ms | 989.9 | 989.9 | 1.2 |

**Repeatability floor: 0.02 % or better** (worst case at 100 bunnies; 0.008 % at 1,000, 0.003 % at 10,000).
Adopt **0.1 %** as the threshold below which a difference is noise and not worth reporting.

Marginal cost is linear and stable: **(436.45 − 90.29) / 4,000 = 86.5 µs per sprite**, and
(869.41 − 436.45) / 5,000 = **86.6 µs**, measured across different count ranges. Chrome is a constant
31 primitives/frame.

## The 100-bunny row is pacing, not cost

At 100 bunnies the frame sits at 16.68 ms = one display period, with `min = 12.2 ms`: the display pipeline is
pacing it. Treat that row as an upper bound on cost, not a measurement of it. The higher counts are the real
data.

`eglSwapInterval(display, 0)` does **not** uncap it — median is unchanged at 16.685 ms — and the swap call
itself measures **0.15 ms**, so the wait is not inside `eglSwapBuffers`: pspGL defers and blocks later, on the
next use of the buffer. Separating work from wait therefore needs pspGL-level timing, which is phase 3.

## Fill is not the wall

Same 1,000 sprites, same submission count, drawn at 4× size (16× the pixels):

| scale | `glEnd`/frame | median | delta |
|---|---|---|---|
| 1× (32×32 sprites) | 1,024.95 | 90.279 ms | — |
| 4× (128×128 sprites) | 1,024.93 | 95.438 ms | **+5.7 %** |

16× the pixels costs 5.7 %; 5× the sprites costs 5×. **The workload is submission-bound, not fill-bound**, so
collapsing N submissions into `ceil(N/capacity)` is the right lever and phase 4's premise holds.

## What this implies

- ~**181 sprites** fit in a 16.7 ms frame today (16.7 / 0.0865).
- At 86.5 µs per sprite, a 60 fps 2D game has room for roughly 180 unbatched sprites. With 1,024 sprites per
  draw call — what a hand-written GU renderer achieves — the same frame would spend its time on pixels instead.
- Any change worth reporting must move a median by more than **0.1 %**; run-to-run noise is far below that, so a
  single run per arm is adequate *on the same build and session*, and two runs remain the habit.
