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

## Adopted library (current reference)

Since sprint 006 the project ships **pspGL from `TheMrIron2/pspgl` @ `ae9a668`, `PSPGL_DLIST_SIZE_WORDS=8192`**,
built by `tools/pspgl-build.sh`. That changed the numbers, so these are the ones phase 4 is judged against.

| bunnies | median A | median B | fps (median) |
|---|---|---|---|
| 100 | 16.684 ms | 16.689 ms | 59.9 (pacing, not cost) |
| 1,000 | **61.625 ms** | **61.641 ms** | 16.2 |
| 5,000 | **294.606 ms** | **294.660 ms** | 3.4 |
| 10,000 | **586.694 ms** | **586.704 ms** | 1.7 |

Marginal cost: (294.63 − 61.63) / 4,000 = **58.3 µs per sprite**, i.e. **~287 sprites** in a 16.7 ms frame.
Against the pre-adoption baseline that is **−31.8 % at 1,000 bunnies, −32.5 % at 5,000 and 10,000**.

How it breaks down, from sprint 005 and 006:

| change | N=1000 median | delta |
|---|---|---|
| pspdev's shipped pspGL (2022) | 90.285 ms | — |
| fork `ae9a668`, default 512-word lists | 81.240 ms | −10.0 % |
| fork, `PSPGL_DLIST_SIZE_WORDS=4096` | 63.602 ms | −21.7 % vs 512 |
| **fork, 8192 (adopted)** | **61.63 ms** | **−24.1 % vs 512** |
| fork, 16384 | 62.73 ms | worse — the curve turns over |

Memory cost of the adopted size: **16 command lists × 8,192 words × 4 B = 512 KiB** of main RAM, up from 32 KiB
at the default. It is not free, and it is why 16,384 was not taken.

## Pre-adoption reference (2022 pspGL, kept for the record)

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

## Batched reference (sprint 007)

Same workload, `PSP_BATCH=1`: one static 4,096-vertex buffer, one `glDrawArrays` per texture run, quads expanded
to two triangles (pspGL maps `GL_QUADS` to a GE triangle fan, so a quad *run* cannot be sent as quads).

| bunnies | `glDrawArrays`/frame | vertices/frame | median A | median B | work | fps (median) |
|---|---|---|---|---|---|---|
| 100 | 2 | 744 | 16.683 ms | 16.683 ms | 3.55 ms | 59.9 (pacing) |
| 1,000 | 3 | 6,150 | 33.367 ms | 33.369 ms | 18.09 ms | 30.0 |
| 5,000 | 9 | 30,150 | 100.107 ms | 100.107 ms | 81.91 ms | 10.0 |

Marginal cost: **(100.107 − 33.367) / 4,000 = 16.7 µs per sprite**, against 62.6 µs for the same-day unbatched
arm. Submission was 46 µs of it; what is left is per submitted vertex (2.4–2.6 µs), flat in pixels: 4× the fill
(`SPRITE_SCALE=2`) moved the frame by 0.1 %, and the HUD-only floor is 3.46 ms.

**Read this table with the drift note in mind.** The published unbatched baseline (61.63 ms at N=1,000) does not
reproduce on 2026-11-20: the same arm reads **66.73 ms**, four 16.68 ms display periods. 61.63 sits between
periods, so it was taken under a different display state. Compare batched and unbatched **within one session**
until the archived rows are re-taken.

## What this implies

- ~**181 sprites** fit in a 16.7 ms frame today (16.7 / 0.0865).
- At 86.5 µs per sprite, a 60 fps 2D game has room for roughly 180 unbatched sprites. With 1,024 sprites per
  draw call — what a hand-written GU renderer achieves — the same frame would spend its time on pixels instead.
- Batched, the frame holds **~790 sprites** at 60 fps ((16.68 − 3.46) / 0.0167) and runs 1,000 at 30 fps. The
  sentence above about pixels was measured and is wrong for this workload: 4× the fill costs 0.1 % once
  submission is gone, so the frame is limited by vertices, not by what they cover.
- Any change worth reporting must move a median by more than **0.1 %**; run-to-run noise is far below that, so a
  single run per arm is adequate *on the same build and session*, and two runs remain the habit.
