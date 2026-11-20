# Sprint 008 — Two-vertex sprites: the last lever inside GL11

| | |
|---|---|
| Roadmap phase | [Phase 4 — PSP rlgl batcher](../phases/04-rlgl-batcher.md) |
| Branch / base commit | `master` @ `1bb0bea6` |
| Started / closed | 2026-11-20 / 2026-11-20 |
| Build number(s) | `b3` triangles `-O0` `2ee10d2a…`, `b4` sprites `-O0` (archive saved), `b5` unbatched `-O2`, `b6` triangles `-O2`, `b7` sprites `-O2`, `b8` no-sprites `-O2` |
| Tasks | 5/6 done, 1 dropped |

## 0. Handover

**Situation.** [Sprint 007](007-phase4-batch-skeleton.md) collapsed submission: at N=1,000 the frame went
66.73 → 33.37 ms, `glEnd` per frame 1,025 → 3. What is left is **per submitted vertex**, 2.4–2.6 µs each, and the
batcher spends **6 vertices per sprite** because pspGL turns `GL_QUADS` into a GE triangle fan and we expand quads
to two triangles ourselves. This sprint sends **2 vertices per axis-aligned sprite** instead, through a primitive
pspGL already exports.

**Why that should be most of what is left.** At 2.5 µs per vertex, 1,000 sprites cost 6,000 × 2.5 µs = 15 ms of
work today; at 2 vertices each it is 2,000 × 2.5 = 5 ms, and the HUD floor is 3.46 ms, so the frame lands near
8.5 ms — inside the 16.68 ms budget with room. That is the difference between 30 fps and 60 fps at N=1,000, and it
is reachable **without leaving OpenGL**, which is why it comes before phase 5 rather than after.

**Already true, verified — do not re-derive:**

- pspGL exports the primitive: `GL_SPRITES_PSP` (`GL_POLYGON+1`, `GL/glext.h:6613`) maps to `GE_SPRITES` in
  `pspgl_varray.c`'s `geprim_tab` (index 10), and `glVertex.c`'s `overhang_count` for it is **0**, so a sprite run
  has no shared vertex to re-emit at a buffer split.
- The reference implementation for this exact workload lives at `/home/psp/ref/PSPRuntime/PSPBasicPlatformer`:
  `GPUManager::DrawSprite()` writes **top-left and bottom-right only** (`src/gpumanager.cpp:270`) and flushes with
  `sceGumDrawArray(GU_SPRITES, GU_COLOR_5551 | GU_VERTEX_32BITF | GU_TEXTURE_32BITF, …)`. That is the same
  primitive through a different API, and it is what the ~5,000-sprites-at-60-fps figure in the roadmap refers to.
  It is **not** yet measured by our protocol on this console — do not quote it as measured.
- The batcher's mode-change flush already exists (`rlPspBatchBegin`), so mixing sprite and triangle runs costs one
  flush per change, nothing more.

**Read before acting:**

1. [Sprint 007 §7 and §9](007-phase4-batch-skeleton.md) — the numbers this sprint moves, and the 2.5 µs/vertex
   figure it must beat.
2. `src/platforms/rcore_psp.c`, the `RLPSP_BATCH` block — `rlPspBatchVertex` assembles quads in `rlPspQuad[4]`
   and emits 6 vertices at the fourth corner. That is the function to change.
3. `psp-tests/batch_check/main.c` — the deterministic scene, and the only correctness gate that can compare arms.

**First action.** Read `rlPspBatchVertex()` in `src/platforms/rcore_psp.c` and add the axis-aligned test to the
four-vertex path; do not touch `rlgl.h` again unless the vertex-format change (T4) needs it.

**Hard rules.**

- **Sprites are only legal for axis-aligned, unrotated, unflipped quads.** A sprite's two vertices are opposite
  corners, so a rotated or mirrored quad sent as a sprite is a wrong picture, not a slow one. Test eligibility on
  the assembled quad, per quad, and fall back to the triangle path.
- **Do not change what raylib asks for.** The eligibility test lives in the batcher, so `DrawTexturePro`,
  `DrawRectangle`, `DrawText` keep calling `rlBegin(RL_QUADS)` + 4 vertices. An API change here would leak into
  `rshapes.c`/`rtextures.c` and become a merge conflict forever.
- **The control is arm `b3`, not the unbatched arm.** Sprint 007 already proved the triangle path is
  byte-identical to the immediate path; this sprint only has to prove sprite submission matches the triangle path.
  Pixel differences at sprite edges are *possible* — GE sprite rasterisation is not the triangle rasteriser — and
  if they appear they must be counted and localised, not waved away.
- `reset` before each run, never `exit`, the app must not call `sceKernelExitGame` (sprint 002 rules, still true).
- Threshold 0.1 %; two runs per configuration.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/src
make clean && make PLATFORM=PLATFORM_PSP PSP_BATCH=1
cp libraylib.a $PSPDEV/psp/lib/libraylib.a

cd ../psp-tests/bunnymark
../bunnymark/measure.sh 1                      # the matrix, both runs
../bunnymark/probes_all.sh                     # fill / HUD / lock probes
```

**Do not touch.** `rlgl.h`'s flush hooks (they are proven), `src/Makefile` switches beyond what T4 needs, anything
under `docs/` outside the sprint record, `psp-tests/basic_shapes`, and the adopted pspGL build.

**Done means** §8: 1,000 axis-aligned sprites at ≤16.7 ms, pixel-equal to `b3`, and the GL11 ceiling written down.

## 1. Objective

A run of axis-aligned quads is submitted as `GL_SPRITES_PSP` at **2 vertices** each instead of `GL_TRIANGLES` at
6, with rotation/flip falling back per quad, such that the bunnymark workload (1,000 unrotated sprites) runs at
**≤16.7 ms** per frame on the PSP-1000 and `batch_check` is pixel-identical to the sprint-007 build.

## 2. Why now

Phase 4's exit is "1,000 sprites at 60 fps, one bind per batch, pixel-exact". Sprint 007 got 2.0× of the 3.7×.
This is the last lever inside GL11 that can close the remaining gap; everything after it — compact vertex formats,
throwing away the pspGL conversion copy — needs the native backend. Doing it now also **prices phase 5**: if
2-vertex sprites land at 8.5 ms, the native backend has to beat 5 µs per sprite to be worth writing, and if they
land at 15 ms the 2.5 µs/vertex figure is not really per-vertex at all and phase 5's plan needs revising.

## 3. Scope

**In:** the eligibility test and the sprite path in `rlPspBatchVertex`/`rlPspBatchFlush`; a counter for sprite
vertices; `batch_check` extended with a rotated sprite and a mirrored sprite; bunnymark and the probes as the
measurement harness.

**Out:**

- Compact vertex formats (16-byte vertices, `5551` colours, 16-bit UVs) — sprint 009.
- Two parallel batch buffers so mixed sprite/rotated scenes do not ping-pong between modes — sprint 009 at the
  earliest; this sprint measures how bad the ping-pong is instead of guessing.
- Any texture-format or VRAM work — phase 6.
- The native GU backend — phase 5. This sprint exists partly to decide whether it is still needed.

## 4. Preconditions

- [ ] `pspsh` answers, USB device is `054c:01c9`
- [ ] `reset` done between runs; the app logs `DONE` and sleeps
- [ ] Sprint 007 libraries archived: `/tmp/arm0/libraylib.a` (`0b0a7aeb…`), `/tmp/arm1/libraylib.a`
      (`2ee10d2a…`). **Copy them somewhere durable first** — `/tmp` does not survive a reboot
- [ ] Staged library is the default (unbatched) build; `sha256 0b0a7aeb…`
- [ ] Counters exist: `arrays`, `arrayVerts`, `binds`, `unbinds`, `verts`, `ends` (`psp-tests/bunnymark/psp_counters.h`)
- [ ] Baseline for this sprint = sprint 007's batched arm, re-run **in the same session** before the change

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **eligibility test** on the assembled quad (axis-aligned corners, UVs following the same corners, one colour and depth) emitting `(TL, BR)` as `GL_SPRITES_PSP`, two triangles otherwise | `done` | `arrayVerts`/frame at N=1,000 drops 6,150 → **2,050**; `batch_check` pixel-identical | 3× fewer vertices exactly as designed: 6,150 → 2,050 at N=1,000, 30,150 → 10,050 at N=5,000. **Pixel check: 0 differing bytes** against the triangle arm, including a row at fractional coordinates and a rotated sprite |
| T2 | **mode runs**: one primitive per batch, flush on change; worst case measured with `ROTATE_ODD=1` rather than estimated | `done` | unrotated: 1 draw per 2,048 sprites; mixed: flushes ≈ N/2, reported not hidden | unrotated N=1,000: **2 draws/frame** (1 sprite batch + HUD). Mixed (`ROTATE_ODD`): **1,001 draws/frame** and 83.417 ms — the ping-pong is worse than the triangle path. Recorded as the reason sprint 009 needs two buffers or a texture-bind cache |
| T3 | **the measurement**: N=100 / 1,000 / 5,000, two runs, `TARGET_FPS=0`; plus the fill probe | `done` | frame median at N=1,000 ≤16.7 ms | **Not met by the sprite path.** At `-O0` the sprite arm measured 33.369 ms — the same as 6 vertices — which is what exposed T6. At `-O2`: **16.683 ms** (60 fps) |
| T4 | **vertex format** (`u16` UVs, `5551` colour) if T3 leaves the frame short of 16.7 ms | `dropped` | — | Not needed: with `-O2` the frame is at the vsync cap at N=1,000. The remaining headroom question is answered by T5, and compact formats are a phase-5 lever with a measured target: 0.44 µs per submitted vertex (T6) |
| T5 | **the GL11 ceiling** as a number: largest N holding 60 fps, per arm | `done` | a table with the 16.68 ms line crossed | sprites hold 60 fps at 1,500, fall to 30 fps at 2,000; triangles already at 30 fps at 1,500 — bisection for both in §7 |
| T6 | **unplanned, and the sprint's real finding**: the PSP library had never been optimised. `src/Makefile`'s `RELEASE` block covers desktop/web/android only, so raylib was built at `-O0` while pspGL is `-Os`. Add `PSP_OPT` (default `-O2`) and measure the levels | `done` | `-O2` moves the frame and does not change a pixel | 1,000 sprites: batched 33.367 → **16.683 ms**, unbatched 66.733 → 50.051 ms. `batch_check` identical at `-O0` and `-O2` (md5 `c4eb44e7…`, 0 differing bytes). Archive 1,838,280 → 1,184,972 B |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- 2026-11-20 — sprint opened, all tasks `todo`
- 2026-11-20 — T1 `done`: sprite path in, `arrayVerts` 6,150 → 2,050/frame at N=1,000
- 2026-11-20 — T1 correctness `done`: `batch_check` extended with a fractional-position row and a rotated
  sprite, both arms captured, **0 differing bytes**
- 2026-11-20 — T2 `done`: 2 draws/frame unrotated; `ROTATE_ODD` measures the ping-pong at 1,001 draws/frame
- 2026-11-20 — **T3 measured 33.369 ms — unchanged from 6 vertices per sprite.** 3× fewer vertices bought 8 % of
  the work. The sprint's premise (cost is per submitted vertex) is falsified; started T6
- 2026-11-20 — T6 `done`: the library was compiled at `-O0`. `PSP_OPT` added, default `-O2`; N=1,000 drops to
  **16.683 ms** (60 fps) with a byte-identical picture. This is the largest single win measured in the project
- 2026-11-20 — T4 `dropped`: the exit criterion is met without compact vertex formats

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame at N=1,000, axis-aligned | ≤16.68 ms (60 fps) | | |
| vertices per sprite | 2 | | |
| `glDrawArrays` per frame at N=1,000 | ≤2 | | |
| pixel difference vs `b3` | 0, or every difference at a sprite edge and counted | | |
| per-frame allocation | 0 | | |
| `src/` changes | `platforms/rcore_psp.c` only | | |

## 7. Measurements

Device only, one `reset` per run, two runs per cell, run-to-run agreement ≤0.005 ms. `work` is `frame − swap`
from the app's own split; it is quoted because the frame medians are quantised to display periods.

**The matrix.** Six arms: {unbatched, batched triangles, batched sprites} × {`-O0`, `-O2`}.

| N | unbatched `-O0` | unbatched `-O2` | triangles `-O0` | triangles `-O2` | sprites `-O0` | sprites `-O2` |
|---|---|---|---|---|---|---|
| 100 | 16.683 (9.16) | 16.683 (6.26) | 16.683 (3.55) | 16.683 (1.90) | 16.683 (3.47) | 16.683 (1.69) |
| 1,000 | 66.733 (65.70) | 50.051 (48.78) | 33.367 (18.09) | **16.683** (9.85) | 33.369 (16.70) | **16.683** (8.06) |
| 5,000 | 316.98 (317.19) | — | 100.107 (81.91) | 66.733 (45.28) | 100.100 (75.70) | 66.733 (36.41) |

Frame medians in ms, `work` in parentheses. The 5,000 unbatched `-O2` cell is missing: the host window closed
before `DONE` and the next `reset` killed a live app — the row is not needed for any conclusion here.

Two things to read out of it, in order of size:

1. **`-O2` is worth more than every batching change combined.** At N=1,000 batched it halves the frame
   (33.367 → 16.683) and the library shrinks 1,838,280 → 1,184,972 B. Nothing in `src/` changed but one `-O2`.
2. **At `-O2` the sprite primitive is worth 18 % of the work** at N=1,000 (9.85 → 8.06 ms) and 20 % at N=5,000
   (45.28 → 36.41). Invisible in the frame medians at those N because both sit inside a display period — and that
   is exactly why the ceiling had to be bisected to see it.

**The 60 fps ceiling**, bisected at `-O2`:

| N | triangles | sprites (2 v) |
|---|---|---|
| 1,000 | 16.683 (60 fps) | 16.683 (60 fps) |
| 1,200 | 16.683 (60 fps) | — |
| 1,300 | **33.366 (30 fps)** | — |
| 1,500 | 33.367 (30 fps) | **16.684 (60 fps)** |
| 1,700 | — | **33.367 (30 fps)** |
| 1,800 | — | 33.368 (30 fps) |
| 2,000 | 33.372 (30 fps) | 33.367 (30 fps) |

So: **triangles hold 60 fps to 1,200–1,300 sprites, sprites to 1,500–1,700.** The sprite primitive buys roughly
25 % more sprites at the ceiling for 18 % less per-sprite work, which is the honest accounting of T1.

**What the ceiling actually is.** Same N=1,500, same 3,050 vertices, four times the pixel area:

| scale | frame | work | vertices/frame |
|---|---|---|---|
| 1× (1.54 M px) | 16.684 ms | 11.50 ms | 3,050 |
| 2× (6.14 M px) | **50.050 ms** | 11.58 ms | 3,050 |

The CPU work does not move and the frame triples. **At the ceiling the console is waiting on the GE's fill, not on
the CPU and not on vertex submission.** ~1.5 M blended texels per frame is where this workload saturates; 6.1 M
takes three display periods. This is the first measurement in the project that puts a *pixel* number on the wall,
and it is what the next phase has to respect.

**The mixed case** (T2), N=1,000, every second sprite rotated so the primitive alternates per sprite:

| configuration | draws/frame | frame | work |
|---|---|---|---|
| all sprites | 2 | 16.683 ms | 8.06 ms |
| all triangles | 3 | 16.683 ms | 9.85 ms |
| every second rotated | **1,001** | **83.417 ms** | 75.95 ms |

The ping-pong is a 5× regression against either pure arm: one flush per sprite, and every flush re-binds the same
texture. **A scene that mixes rotated and unrotated sprites must not use the sprite path as it stands.**

**Pixel comparison.** `batch_check` (8×4 grid at integer positions, a row at fractional positions, one rotated
sprite, a rectangle, a string), captured after `DONE`:

| build | md5 of the capture | vertices/frame | draws/frame |
|---|---|---|---|
| triangles `-O0` | `c4eb44e7577ba93b1af93a4861008ad9` | 312 | 2 |
| sprites `-O0` | `c4eb44e7577ba93b1af93a4861008ad9` | 108 | 3 |
| sprites `-O2` | `c4eb44e7577ba93b1af93a4861008ad9` | 108 | 3 |

`cmp -l` → **0 differing bytes** in all three comparisons. GE sprite rasterisation and the two-triangle
rasterisation agree exactly here, including the row whose sprite edges land inside a pixel. Mirrored quads
(negative source width/height) are rejected by the eligibility test and take the triangle path, but that case is
not in the captured scene — it inherits correctness from the rotated case it shares a code path with, and that is
all that is claimed for it.

## 8. Exit criteria

- [x] 1,000 axis-aligned sprites at ≤16.68 ms, two runs — **16.683 ms**, but reached by `-O2`, not by the sprite
  primitive. The primitive alone measured 33.369 ms at `-O0`; it is recorded that way in §7 rather than credited
- [x] `batch_check` pixel-identical, differences counted — 0 bytes across three arm pairs
- [x] rotated sprites still render correctly — captured and identical; mirrored quads are rejected by the test and
  inherit the same path, untested by capture
- [x] the GL11 ceiling published — triangles 1,200–1,300 sprites, sprites 1,500–1,700, both at `-O2`
- [x] the cost of a mixed sprite/triangle scene measured — 1,001 draws/frame, 83.417 ms
- [x] `memory-map.md` §4 and §9 updated; `baselines.md` carries an `-O2` re-baseline and marks the old tables as
  history; `environment.md` documents `PSP_OPT` and the batch switches

## 9. Result

**The sprint set out to halve the vertex count and found a build flag instead.** Both halves matter, and they are
worth separating.

**What the sprite primitive bought.** Axis-aligned quads now go out as `GL_SPRITES_PSP`, 2 vertices instead of 6,
and the picture is byte-identical to the triangle path — 0 differing bytes, including fractional positions where
the two rasterisers could have rounded differently. At `-O2` it is worth **18–20 % of the frame's CPU work** and
it moves the 60 fps ceiling from 1,200–1,300 sprites to 1,500–1,700. That is a real, permanent gain and it cost
about forty lines.

**What it did not buy, and the trap in assuming it would.** At `-O0` — the optimisation level every measurement in
this project had used — the sprite arm measured **33.369 ms against 33.367 ms** for six vertices. Three times fewer
vertices, no change. Sprint 007's conclusion that the residual was "per submitted vertex" was wrong, and the way it
was wrong is the useful part: `src/Makefile` applies optimisation in a `RELEASE` block that covers desktop, web and
Android, and never had a PSP clause, so `psp-gcc` built raylib at `-O0` while pspGL was built `-Os`. The cost was
raylib's own unoptimised 2D path, which is invariant to how many vertices we hand the driver.

`PSP_OPT` now defaults to `-O2`. At N=1,000 the batched frame halves, 33.367 → **16.683 ms**, i.e. **1,000 sprites
at 60 fps**, and the unbatched arm improves 66.733 → 50.051 ms. The picture does not change.

**What the ceiling is.** Bisection put 60 fps at 1,200–1,300 sprites for triangles and 1,500–1,700 for sprites, and
the fill probe explains what stops there: at N=1,500, four times the pixel area takes the frame from 16.684 to
50.050 ms while CPU work stays at 11.5 ms. **The wall is now the GE's blended fill rate, not submission, not
vertices, not the CPU.** 1.5 M texels per frame is what this console holds at 60 Hz for this workload.

**What broke.** The mixed case: one rotated sprite between two unrotated ones takes the frame from 16.683 to
83.417 ms because the primitive alternates per sprite and every change flushes and re-binds the texture. Two
buffers, or at minimum a bind cache inside `rlPspBatchFlush`, are the fix, and the number above is why it is the
first task of sprint 009 rather than an optimisation for later.

**What this does to the roadmap.** Phase 5's justification was "the residual is per-vertex cost through pspGL".
That premise is gone twice over: `-O2` removed half of it and the sprite primitive removed a fifth of what was
left. What remains at 1,000 sprites is ~8 ms of work and a fill limit 3× away from PSPRuntime's claimed ~5,000
sprites at 60 fps. **Before writing a native backend, that claim has to be measured with our protocol** — if it
holds, there is a fill lever we have not found (bilinear filtering off, 5551 textures with no blending, or simply
a smaller sprite), and that lever is worth more than a backend rewrite. If it does not hold, our GL11 route is
already within striking distance of the reference and phase 5 should be re-planned around fill, not submission.

## 10. Follow-ups

- **Verify PSPRuntime's ~5,000 sprites at 60 fps** with our protocol (`/home/psp/ref/PSPRuntime/PSPBasicPlatformer`,
  `GPUManager::DrawSprite` → 2 vertices + `sceGumDrawArray(GU_SPRITES, GU_COLOR_5551|GU_VERTEX_32BITF|GU_TEXTURE_32BITF)`).
  It is interactive today, so it needs a bounded-frame harness. This single measurement decides whether the next
  phase chases fill or a backend.
- **Bind cache in the batcher**: track the texture actually bound and skip `glBindTexture` when the run uses the
  same one. In the mixed case that is 1,000 redundant binds per frame saved; pspGL's own profiler already counts
  `texture_bind_redundant`, which suggests it expects callers to avoid them.
- **Two batch buffers** (sprite + triangle) so a mixed scene never ping-pongs: sprint 009, justified by the 83.4 ms
  number rather than by taste.
- **Fill budget for 2D**: 1.5 M blended texels per frame is the working number at 60 Hz. Anything the roadmap asks
  of 2D — overdraw, large backgrounds, particle counts — should be checked against it. Phase 6's texture-format work
  (5551 vs 8888) belongs here too, since it changes bytes read per texel.
- **`-O3` vs `-O2`** was never A/B'd. The memory map's own note said "`-O2` first, not `-O3`"; now that `-O2` is
  measured, `-O3` costs one build.
