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

## Optimisation level: the re-baseline that matters most (sprint 008)

Every number below this section was measured with the library compiled at **`-O0`**. `src/Makefile` applies
optimisation in a `RELEASE` block that covers desktop, web and android only, and the PSP block had never added its
own, so `psp-gcc` used its `-O0` default — while pspGL itself is built `-Os`. `PSP_OPT` now defaults to `-O2`
(`make PLATFORM=PLATFORM_PSP PSP_OPT=-O0` reproduces the old measurements). Same console, same protocol, same
commit, two runs per cell:

| bunnies | unbatched `-O0` | unbatched `-O2` | batched `-O0` | batched `-O2` (triangles) | batched `-O2` (sprites) |
|---|---|---|---|---|---|
| 100 | 16.683 | 16.683 | 16.683 | 16.683 | 16.683 |
| 1,000 | 66.733 | 50.051 | 33.367 | **16.683** | **16.683** |
| 5,000 | 316.98 | — | 100.107 | 66.733 | 66.733 |

**1,000 sprites at 60 fps**, and the picture is byte-identical between `-O0` and `-O2` (`batch_check` md5
`c4eb44e7…`, 0 differing bytes). The archive shrinks 1,838,280 → 1,184,972 B.

The 60 fps ceiling, bisected at `-O2` (the frame leaves 16.683 ms in exactly one step, so these are brackets of
100–200 sprites, not interpolations):

| arm | 60 fps at | 30 fps at | ceiling |
|---|---|---|---|
| unbatched, `-O2` | — | 1,000 (50.05 ms) | below 1,000 |
| batched, triangles, `-O2` | 1,200 | 1,300 | 1,200–1,300 |
| batched, sprites (2 v), `-O2` | 1,500 | 1,700 | 1,500–1,700 |

Before this, at `-O0`, 1,000 sprites was already 30 fps in the batched arm. **Treat the two tables further down as
history**: they are still the record of what batching and the pspGL list size bought, but they are not what the code
does today.

## Scaling past the ceiling, and what "maximum at 60 fps" means

Batched sprite arm at `-O2`, 32×32 RGBA8888 blended sprites, `TARGET_FPS=0`, two runs per point (agreement
≤0.01 ms):

| sprites | frame | periods | work | texels/frame |
|---|---|---|---|---|
| 100 | 16.683 | 1 | 1.69 ms | 0.10 M |
| 1,000 | 16.683 | 1 | 8.06 ms | 1.02 M |
| 1,500 | 16.684 | 1 | 11.50 ms | 1.54 M |
| 1,700 | 33.367 | 2 | — | 1.74 M |
| 2,000 | 33.367 | 2 | 14.97 ms | 2.05 M |
| 2,500 | 33.367 | 2 | 18.84 ms | 2.56 M |
| 4,000 | 50.050 | 3 | 29.20 ms | 4.10 M |
| 5,000 | 66.733 | 4 | 36.41 ms | 5.12 M |
| 8,000 | 83.417 | 5 | 57.62 ms | 8.19 M |

**The CPU side is exactly linear: `work = 0.98 ms + 7.08 µs × sprites`.** That fit is within 1.1 % at all eight
points, so it is a budget, not a trend. The frame is then quantised into 16.683 ms display periods, and
`ceil((work + ~5 ms)/16.683)` predicts the period count at every point up to 4,000 — the model's predictions of
1,511 sprites at 60 fps matches the measured bracket (1,500 passes, 1,700 fails) to within 1 %.

| question | answer | how |
|---|---|---|
| max sprites at 60 fps **today** | **~1,500** (bracket 1,500–1,700) | bisection + the model above |
| what a 60 fps frame costs per sprite | ≤11.7 ms of work, i.e. 7.08 µs is what we pay and ~11.7 ms is all there is | `16.683 − 5 ms fixed` |
| to hold 3,000 sprites | **3.57 µs/sprite** | model |
| to hold 5,000 sprites | **2.14 µs/sprite** | model |
| where the fill ceiling is | **~4,000–5,000 sprites (4–5 M blended texels/frame)** | at 5,000 and 8,000 the frame is one period worse than the CPU model predicts, i.e. pixels have started to bind |

Two independent limits therefore meet near **5,000**: the CPU needs 2.14 µs/sprite to get there, and the pixel
budget runs out about the same place. That is also where `PSPRuntime`'s ~5,000-at-60-fps sits, which makes its
number plausible rather than surprising — it is not a better API, it is a cheaper per-sprite CPU path that lets the
console reach its own fill limit.

**So the gap between 1,500 and 5,000 is entirely per-sprite CPU work in raylib's 2D path.** At 222 MHz, 7.08 µs is
~1,570 cycles for what is ~30 float operations, four texture-coordinate stores, four vertex pushes and the quad
staging copy — which points at call overhead, not arithmetic: `rlVertex2f`/`rlTexCoord2f` are real calls into a
*different translation unit* (`platforms/rcore_psp.c`) with six float arguments passed on the stack under the o32
ABI, so nothing inlines and each sprite pays for fifteen to twenty of them `[derived: 1,570 cycles/sprite]`.

### After sprint 009 (current default)

The batcher is the default build now, and the vertex push path is inlined. The fit, six points, ≤1.1 % error:

```
work (ms)  = 0.87 + 6.35 µs x sprites        (was 0.98 + 7.08 before sprint 009)
frame (ms) = ceil((work + ~5.5)/16.683) periods
```

| sprites | frame | work | note |
|---|---|---|---|
| 1,000 | 16.683 | 7.30 ms | 60 fps |
| 1,500 | 16.683 | 10.39 ms | 60 fps |
| **1,600** | **16.684** | 11.01 ms | **last N at 60 fps** |
| 1,700 | 33.367 | 11.64 ms | 30 fps |
| 5,000 | 66.733 | 32.63 ms | 2 binds/frame |

`work` is not all raylib: at N=1,000 the bunnymark loop and HUD cost 2.35 ms on their own (`SKIP_DRAW`), so the
drawing path is **4.95 µs per sprite** (5.71 before). The ceiling being 1,600 rather than the ~2,500 the CPU model
alone would allow is the GE: at 1,600 sprites its fill takes ~5.5 ms, and work + GE is what must fit a period.

Mixed rotated/unrotated scenes: an alternation budget caps primitive-class changes at 8 per frame, so the worst case
is 12 draws/frame and a 16.683 ms frame, against 1,001 draws and 83.417 ms before it.

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

## Batched reference (sprint 007, `-O0`, historical)

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
- Batched at `-O0`, the frame held ~790 sprites at 60 fps; batched at `-O2` it holds **1,500–1,700** with the
  sprite primitive and **1,200–1,300** with triangles, and 1,000 sprites run at 60 fps. The sentence above about
  pixels is wrong for this workload at `-O0` (4× the fill cost 0.1 %); at `-O2` it has not been re-tested, and the
  ceiling probe that would settle it is a follow-up.
- Any change worth reporting must move a median by more than **0.1 %**; run-to-run noise is far below that, so a
  single run per arm is adequate *on the same build and session*, and two runs remain the habit.
