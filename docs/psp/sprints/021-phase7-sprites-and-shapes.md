# Sprint 021 (P7-S4) — Sprites and shapes: culling and triangle submission

| | |
|---|---|
| Roadmap phase | [Phase 7 — 2D specialisation](../phases/07-2d-specialisation/index.md) |
| Sprint | P7-S4 |
| Branch / base commit | `master` @ `acc1ade4` (docs) / code unchanged since `9e34303d` |
| Started / closed | — / — |
| Build number(s) | pspgu `b24` (unchanged); `libraylib.a` builds with `PSP_CULL` and `PSP_TRI_FAST` |
| Tasks | 0/5 done |

## 0. Handover

**Situation.** Sprint 018 measured the shape scene (1,000 rectangles + 1,000 triangles + 1,000 lines) at
**10,000 vertices and 4 draws/frame, 21.79 ms**. The composition matters: on PSP `DrawRectanglePro` already uses
the textured quad path, so the batcher turns each rectangle into a **2-vertex GE sprite** (2,000); `DrawLine`
submits 2 vertices (2,000); `DrawTriangleGradient` uses a **4-vertex quad** with three distinct colours (the
fourth vertex duplicates the third), and the batcher cannot make a sprite of it, so each triangle becomes **6
vertices** (6,000). The cost is therefore the **triangle path**, not rectangles: 2,000 + 6,000 + 2,000 = 10,000.
The same 4-vertex quad is used for a solid triangle, where the fourth vertex is degenerate.

The phase's tasks are therefore (a) reject off-screen sprites **before** their per-sprite math and vertices, and
(b) let triangles submit **3 vertices** directly instead of a 4-vertex quad that becomes two triangles.

**Read before acting**, in this order:

1. `../sprints/018-phase7-corpus-and-budget.md` (P7-S1) §7 — the shape counters (10,000 vertices, 4 draws) and the
   mixed scene.
2. `../sprints/020-phase7-tilemap-and-windowing.md` (P7-S3) §9 — the current driver and gate state.
3. `src/rshapes.c:322` — `DrawTriangleGradient` and the `SUPPORT_QUADS_DRAW_MODE` branch; `src/rshapes.c:318` —
   `DrawTriangle` calling it with one colour.
4. `src/rshapes.c:618-710` — `DrawRectanglePro`: it is already a textured quad, and the batcher makes it a
   2-vertex sprite. Do not change it in this sprint.
5. `src/rtextures.c:4876` — `DrawTexturePro`, the single path behind sprites, text glyphs and tiles.
6. `src/rlgl.h:622-930` — `rlPspIsSpriteQuad` and the batcher, so the 2-vertex rectangle and the 6-vertex
   triangle are understood.
7. `src/Makefile:549-600` — where the PSP build switches live.

**First action.** Add `MODE=sprites` to `scene_2d` (fixed count, known off-screen fraction) and run the 018
`MODE=shapes` scene before changing `src/`. Count the submitted vertices to confirm the 2k/6k/2k split; do not
trust the arithmetic alone.

**Hard rules.**

- **Cull before generating anything.** The rejection goes at the top of `DrawTexturePro` and `DrawRectanglePro`,
  before the corner math and before `rlBegin`; a batcher-level cull after four `rlVertex` calls does not satisfy
  the phase task.
- **Cull only an unrotated axis-aligned draw.** `rotation == 0`, positive destination size, the destination
  entirely outside `[0, GetScreenWidth()) × [0, GetScreenHeight())`, and no active render target. A rotated draw
  whose corners are outside can still cover the screen; never cull it.
- **Triangles keep their pixels.** The 3-vertex path must have the same `(v1,v2,v3)` triangle as the 4-vertex
  quad's non-degenerate triangle and the same three colours. The `MODE=shapes` capture must be byte-identical.
- **Rectangles are out of scope.** They already submit as 2-vertex sprites; changing `DrawRectanglePro` is not
  part of this sprint and would only disturb a working path.
- **The 2D gate is the contract.** `batch_check` and the 018/019/020 captures must not move.
- **A/B needs a library switch.** `PSP_CULL=0|1` (default 1) and `PSP_TRI_FAST=0|1` (default 1) in
  `src/Makefile`; an app-only flag cannot A/B a library path.
- **No cap change.** `RLPSP_BATCH_CAP` stays 4,096. If the shape scene needs a second draw, record it.
- **No driver change.** pspgu stays `b24`; the 512-window path is untouched.
- **Stage only this sprint's paths.** Other sessions edit the tree; `git status` before every commit.
- Commit messages carry the task ID: `sprint 021 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
./tools/pspgu-build.sh                  # b24, unchanged

# Library arms: baseline, cull on, triangle fast path on.
cd src
make PLATFORM=PLATFORM_PSP -j8 PSP_CULL=0 PSP_TRI_FAST=0
cp libraylib.a $PSPDEV/psp/lib/libraylib.a
# ... run shapes, then rebuild with PSP_CULL=1 / PSP_TRI_FAST=1.

cd ../psp-tests/scene_2d
make clean && make MODE=sprites COUNT=10000 OFFSCREEN=0.5
make clean && make MODE=shapes COUNT=1000
cp scene_2d.prx /tmp/s3/app.prx
cd /tmp/s3 && timeout 20 pspsh -e "reset"; sleep 6
( printf './app.prx\n'; sleep 16 ) | timeout 35 pspsh
pspsh -e "scrshot host0:/s21.raw"          # after DONE only
```

**Do not touch.** `DrawRectanglePro`'s working quad path, the text path, the 512-window splitter, the particle
path, the allocator, and `DrawMesh`.

**Done means** §8: culling removes the off-screen work and vertices, triangles submit 3 vertices with identical
pixels, the shape scene is at most 7,000 vertices / 3 draws, and the regression hashes hold.

## 1. Objective

Four falsifiable results:

1. A **sprite scene** with a known off-screen fraction submits vertices only for the visible quads, and work
   falls because the skipped draws return before their math; the capture is unchanged.
2. A **triangle** submits **3 vertices** instead of 6 (1,000 triangles: 3,000 not 6,000), with identical pixels.
3. The **shape scene** drops from 4 draws and 10,000 vertices to at most **3 draws and 7,000 vertices**, with
   work recorded before/after.
4. **Bind audit:** consecutive shapes do not produce a bind per shape; rectangles keep their one shapes-texture
   run, triangles add none, so the frame's binds are runs, not objects.

## 2. Why now

Phase 7's exit criteria name sprites and shapes explicitly, and sprint 018 left shapes as the largest measured
2D cost. The window work (020) and text work (019) are closed; sprite rejection and the triangle path are the
last renderer changes before the particle and cache-strip sprints. The rectangle path is already optimal for
this scene and is explicitly left alone.

## 3. Scope

**In:** `psp-tests/scene_2d` sprite and shape modes; `src/rtextures.c` `DrawTexturePro` culling;
`src/rshapes.c` `DrawRectanglePro` culling and `DrawTriangleGradient` 3-vertex submission; `src/Makefile`
`PSP_CULL` and `PSP_TRI_FAST`.

**Out:** the rectangle quad path (already 2 vertices after the batcher), text, windowing, particles, 3D, render
targets (phase 10), the batch cap/arena, and any driver change.

## 4. Preconditions

- [ ] Sprint 020 closed at `9e34303d`; pspgu `b24`, gate `b69830b8…`
- [ ] 018 shape baseline (`4 draws, 10,000 vertices, 21.79 ms`) and capture are in `sprint018/`
- [ ] The 2k/6k/2k composition is confirmed by the counters, not assumed
- [ ] `batch_check` includes an untextured rectangle (it does) for the gate
- [ ] Device loop available; pspGL restorable (`2ab7378e`)

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Corpus**: `MODE=sprites COUNT OFFSCREEN` with deterministic positions; keep `MODE=shapes`; baseline both with `PSP_CULL=0 PSP_TRI_FAST=0` | `todo` | objects/submitted vertices/work recorded; captures deterministic; 2k/6k/2k confirmed | |
| T2 | **API culling**: in `DrawTexturePro` and `DrawRectanglePro`, return before the corner math when `rotation == 0`, size is positive, the destination is entirely outside the screen, and no render target is active; `PSP_CULL=0\|1` | `todo` | 10,000 sprites at 50 % off-screen submit ~10,000 vertices not 20,000; work drops; capture identical | |
| T3 | **Triangle fast path**: `DrawTriangleGradient` on PSP submits `RL_TRIANGLES` with 3 vertices and the three colours when `PSP_TRI_FAST=1`; `=0` keeps the 4-vertex quad | `todo` | 1,000 triangles submit 3,000 vertices not 6,000; shape capture byte-identical | |
| T4 | **Bind/run audit**: count binds vs object count in the shape and sprite scenes | `todo` | shapes: one shapes-texture run for rectangles, none added by triangles, not 1,000 binds; sprite scene: one bind per run | |
| T5 | **Regression**: 018/019/020 captures, `batch_check`, mixed, VRAM/residency | `todo` | all hashes unchanged; gate `b69830b8…`; 0 evictions | |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable.

### T1 design

- `MODE=sprites`: `COUNT` sprites (default 10,000) on a deterministic grid; `OFFSCREEN` is the fraction placed
  outside the 480×272 screen (default 0.5), alternating left/right/top/bottom so culling cannot be order-biased.
  Draw with `DrawTexturePro` at 32×32, one texture. Marker: `objects=COUNT visible=… submitted=…`.
- `MODE=shapes`: keep the 018 scene (1,000 each of rectangles, triangles, lines), group order rects → triangles
  → lines. The triangle implementation is chosen by the **library** switch; rectangles are not changed.
- Baseline with `PSP_CULL=0 PSP_TRI_FAST=0`, confirming 2k rects + 6k triangles + 2k lines = 10,000.
- Log the two library switches at boot (for example in `rcore_psp.c`'s init trace) so a stale archive is
  visible before a measurement is believed.

### T2 design

- `src/rtextures.c` `DrawTexturePro`, after the destination rectangle is normalised and before the corner math:
  ```c
  #if defined(PLATFORM_PSP) && (PSP_CULL == 1)
      if ((rotation == 0.0f) && (dstrec.width > 0.0f) && (dstrec.height > 0.0f) &&
          (rlGetActiveFramebuffer() == 0))
      {
          float x = dstrec.x - origin.x;
          float y = dstrec.y - origin.y;
          if ((x + dstrec.width <= 0.0f) || (x >= (float)GetScreenWidth()) ||
              (y + dstrec.height <= 0.0f) || (y >= (float)GetScreenHeight())) return;
      }
  #endif
  ```
  The function must not touch texture state before returning.
- `src/rshapes.c` `DrawRectanglePro`, at the top with the same `rotation == 0`, positive size, screen-rect test,
  no-FBO guard. This is the solid rectangle path; it already submits two vertices and is otherwise unchanged.
- `PSP_CULL` default is 1 in `src/Makefile`; `=0` compiles the cull out. Do not put the cull in the batcher:
  that is after the four vertices the phase task asks to avoid.
- Text glyphs call `DrawTexturePro`, so off-screen glyphs are skipped too; tiles likewise. That is a benefit,
  but the sprite scene is the evidence.

### T3 design

- `src/rshapes.c` `DrawTriangleGradient`: on PSP with `PSP_TRI_FAST == 1`, use the existing untextured triangle
  branch:
  ```c
  #if SUPPORT_QUADS_DRAW_MODE && !(defined(PLATFORM_PSP) && (PSP_TRI_FAST == 1))
      ... existing 4-vertex textured quad ...
  #else
      rlBegin(RL_TRIANGLES);
          rlColor4ub(c1...); rlVertex2f(v1);
          rlColor4ub(c2...); rlVertex2f(v2);
          rlColor4ub(c3...); rlVertex2f(v3);
      rlEnd();
  #endif
  ```
  The 4-vertex quad's only non-degenerate triangle is `(v1,v2,v3)` with `c1,c2,c3`; the 3-vertex path is the
  same triangle without the duplicated vertex and without the shapes texture.
- `DrawTriangle` passes one colour to this function; the 3-vertex path keeps that colour on all three vertices.
- `PSP_TRI_FAST == 0` keeps the current quad path for the A/B.
- `DrawPoly`, `DrawRectanglePro`, gradients and `DrawTriangleLines` are untouched.
- The `MODE=shapes` capture is the pixel check; `batch_check` has no triangle, so the scene is the evidence.
  If the capture moves, revert T3 and record the null; T2 still stands.

### T4 design

- The `--wrap` counters already report binds/unbinds and array vertices. The app marker adds `visible` computed
  from `COUNT × (1 − OFFSCREEN)`; the submitted count must match the visible count (2 vertices each).
- Shapes: rectangles bind the shapes texture once per run; triangles add no bind; total binds do not scale with
  1,000 objects. A bind per object is a bug.
- The sprite scene uses one texture for the whole run; binds should be one for the run, not per sprite.

### T5 design

- Rebuild `libraylib.a` with the adopted switches; run text 8/24, tiles, particles, mixed, oversize and the wide
  tile atlas; compare to the 018/019/020 captures. Run `batch_check WRAP=0` vs `par_pspgl.raw`. Record
  VRAM/residency.

## 6. Budgets

| item | budget | why |
|---|---|---|
| sprite scene | 10,000, 50 % off-screen | a large enough culling signal without a long frame |
| shape scene | 1,000 + 1,000 + 1,000 | the 018 corpus, unchanged |
| measured frames | 300, two runs | project rule |
| triangle vertices | 3 per triangle | the target; 6 is the baseline |
| shape target | ≤3 draws, 7,000 vertices | 2k rects + 3k triangles + 2k lines |
| cull decision | fewer submitted vertices and lower work | a vertex-only win with flat work is recorded separately |
| gate | byte-identical | the untextured rectangle is in the gate |

## 7. Measurements

Filled by T1–T5.

**Sprites:** total / visible / submitted vertices / draws / work, with `PSP_CULL=0` and `=1`.

**Shapes:** `PSP_TRI_FAST=0` vs `=1`: vertices, draws, binds, work; capture hashes.

**Regression:** 018/019/020 hashes, gate, residency.

## 8. Exit criteria

- [ ] the sprite scene records total, visible and submitted vertices, and work with culling on and off
- [ ] culling reduces submitted vertices to the visible count and lowers work; capture identical
- [ ] a triangle submits 3 vertices, not 6, with a byte-identical `MODE=shapes` capture
- [ ] the shape scene is at most 3 draws / 7,000 vertices, with work recorded
- [ ] binds do not scale with object count in either scene
- [ ] 018/019/020 captures and the gate are byte-identical
- [ ] no driver (`tools/pspgu/`) change is in the sprint diff

## 9. Result

Not run yet.

## 10. Follow-ups

- **Sprint 022 (P7-S5) — particles**: one dynamic stream per texture run, simulation separate, cap boundary.
- **Sprint 023 (P7-S6) — cache strips, regression, phase close**.
- If T3 is a pixel null, `PSP_TRI_FAST` stays 0 and the reason is recorded; the culling win (T2) is independent.
