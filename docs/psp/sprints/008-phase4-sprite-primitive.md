# Sprint 008 — Two-vertex sprites: the last lever inside GL11

| | |
|---|---|
| Roadmap phase | [Phase 4 — PSP rlgl batcher](../phases/04-rlgl-batcher.md) |
| Branch / base commit | `master` @ `1bb0bea6` |
| Started / closed | — / — |
| Build number(s) | `b3` (sprint 007 batched, `2ee10d2a…`), `b4` (this sprint) |
| Tasks | 0/5 done |

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
| T1 | **eligibility test**, in `rlPspBatchVertex` on the assembled quad: the four corners `TL, BL, BR, TR` form an axis-aligned rectangle and the four texture coordinates match the same corners, i.e. `x0==x1`, `x2==x3`, `y0==y3`, `y1==y2`, and `u`/`v` monotone in the same sense. Emit `(TL, BR)` and their two UVs as `GL_SPRITES_PSP` when eligible, the existing two triangles when not | `todo` | `batch_check` grid renders identically to `b3`; `arrayVerts`/frame at N=1,000 drops from 6,150 to ≈2,050 + HUD → expected: ≤2,200 | — |
| T2 | **mode runs**: a batch holds sprite quads or triangles, never both. On a change, flush. Count the flushes so a mixed scene's cost is visible, and add a bunnymark build that draws every other sprite rotated (`ROTATE_ODD=1`) to measure the worst case honestly | `todo` | unrotated run: 1 draw per 2,048 sprites, i.e. **1** at N=1,000; mixed run: flushes ≈ N/2 and the frame time is reported, not hidden | — |
| T3 | **the measurement**: N=100 / 1,000 / 5,000, two runs, `TARGET_FPS=0`, plus the fill probe (`SPRITE_SCALE=2`) at N=1,000 to confirm sprites did not move the workload onto pixels | `todo` | frame median at N=1,000 **≤16.7 ms** → expected 8–10 ms work; N=5,000 ≈28 ms; the 4×-fill probe within 2 % of the 1× probe | — |
| T4 | **vertex format for the sprite path** (only if T3 leaves the frame short of 16.7 ms): `GL_UNSIGNED_SHORT` UVs and `GL_UNSIGNED_SHORT_5_5_5_1` colour if pspGL's array path accepts them, cutting the per-vertex conversion and the GE's read width | `todo` | 24 B → 16 B per vertex and a measured delta, or a recorded "pspGL rejects the type" with the `assert`/`GLERROR` that says so | — |
| T5 | **the GL11 ceiling**, stated as a number: the largest N that holds 60 fps, the cost per sprite at the ceiling, and the gap to the `PSPRuntime` reference (~5,000 at 60 fps, unverified by our protocol). This is what phase 5 has to beat and the number §9 must publish | `todo` | a table: N vs frame, with the 16.68 ms line crossed and the per-sprite cost on each side of it | — |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable — never renumber.

### Task log (append-only)

- _(not started)_

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

Protocol: device only; one variable per run; control (`b3`) → change (`b4`) → control again. Two runs per row.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control | b3 | N=1,000 | frame / vertices per frame | 33.367 ms / 6,150 | sprint 007, same session, re-run first |
| change | b4 | N=1,000 | | | |
| change | b4 | N=5,000 | | | |
| change | b4 | N=1,000, `SPRITE_SCALE=2` | | | fill probe |
| change | b4 | N=1,000, `ROTATE_ODD=1` | | | worst-case ping-pong |
| control again | b3 | N=1,000 | | | |

Pixel comparison: `batch_check` `b3` vs `b4`, byte count and, if nonzero, the differing pixel count and whether
every one lies on a sprite boundary.

## 8. Exit criteria

- [ ] 1,000 axis-aligned sprites at ≤16.68 ms, two runs, evidence in §7
- [ ] `batch_check` pixel-identical to `b3`, or the differences counted and localised
- [ ] rotated and mirrored sprites still render correctly (they take the triangle path)
- [ ] the GL11 ceiling number is published in §9 and in `baselines.md`
- [ ] the cost of a mixed sprite/triangle scene is measured, not estimated
- [ ] `memory-map.md` §4 and §9 updated with what `GL_SPRITES_PSP` actually costs

## 9. Result

(To fill in on close. The interesting outcome is not "faster": it is whether 2-vertex sprites put 1,000 sprites
inside 16.68 ms. If they do, phase 4 closes 2D and phase 5 becomes a question of how much more than 60 fps at
1,000 sprites we want. If they do not, the per-vertex 2.5 µs was never per-vertex, and phase 5's premise needs
re-checking before a line of it is written.)

## 10. Follow-ups

- Two parallel buffers (sprite + triangle) so a scene that interleaves rotated and unrotated sprites never
  ping-pongs — sprint 009, with the mixed-scene number from T2 as the justification.
- `GE_SPRITES` has no share of the `GU_TRANSFORM_2D` saving; if that lever matters, it is phase 5.
- Measure `PSPRuntime`'s bunnymark with our protocol (bounded frames, logged medians) so the phase 5 target is a
  number from this console rather than a claim. It is interactive today, so it needs a harness change first.
