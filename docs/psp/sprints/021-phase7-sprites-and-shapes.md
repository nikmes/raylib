# Sprint 021 (P7-S4) — Sprites and shapes: culling and solid-quad submission

| | |
|---|---|
| Roadmap phase | [Phase 7 — 2D specialisation](../phases/07-2d-specialisation/index.md) |
| Sprint | P7-S4 |
| Branch / base commit | `master` @ `a571aa3c` (docs) / code unchanged since `9e34303d` |
| Started / closed | — / — |
| Build number(s) | pspgu `b24` (unchanged); `libraylib.a` builds with `PSP_CULL` and `PSP_RECT_QUADS` |
| Tasks | 0/5 done |

## 0. Handover

**Situation.** Sprint 018 measured the two 2D workloads that are still expensive: the shape scene (1,000
rectangles + 1,000 triangles + 1,000 lines) emits **10,000 vertices and 4 draws/frame at 21.79 ms**, and sprites
are already one draw per texture run but pay for quads that are entirely off-screen. The batcher chooses a
2-vertex GE sprite for an axis-aligned quad with one colour (`src/rlgl.h:rlPspIsSpriteQuad`), yet solid
rectangles on PSP take `src/rshapes.c:DrawRectanglePro`'s per-triangle branch — six vertices each — because the
quad path is disabled by the unsigned-TEXCOORD wrap guard. The phase's tasks are therefore (a) reject
off-screen sprites **before** the per-sprite math and vertices, and (b) give solid rectangles the quad/sprite
path without that wrap.

**Read before acting**, in this order:

1. `../sprints/018-phase7-corpus-and-budget.md` (P7-S1) §7 — shapes and mixed counters, the 4-draw/10,000-vertex baseline.
2. `../sprints/020-phase7-tilemap-and-windowing.md` (P7-S3) §9 — the current driver and gate state.
3. `src/rtextures.c:4876` — `DrawTexturePro`; it is the one path behind sprites, text glyphs and tiles.
4. `src/rshapes.c:618-710` — `DrawRectanglePro` and the PSP per-triangle branch; `src/rshapes.c:1489` — the
   `DrawPoly` guard and why it exists.
5. `src/rlgl.h:622-930` — the batcher's sprite selection, so the 2-vertex result of a solid quad is understood.
6. `src/Makefile:549-600` — where `PSP_BATCH` and the other PSP build switches live.

**First action.** Add `MODE=sprites` to `scene_2d` (a fixed count with a known off-screen fraction) and run the
018 `MODE=shapes` scene before changing `src/`. The culling claim is measured as submitted vertices **and**
work, not as a frame-time guess.

**Hard rules.**

- **Cull before generating anything.** The rejection goes at the top of `DrawTexturePro` and `DrawRectanglePro`,
  before the corner math and before `rlBegin`; a batcher-level cull after four `rlVertex` calls does not satisfy
  the phase task.
- **Cull only an unrotated axis-aligned draw.** `rotation == 0`, positive destination size, and the destination
  rectangle entirely outside `[0, GetScreenWidth()) × [0, GetScreenHeight())`. A rotated draw whose corners are
  outside can still cover the screen; never cull it. Render targets are phase 10: do not cull when one is
  active.
- **The 2D gate is the contract.** `batch_check` draws an untextured rectangle; a rectangle change that moves one
  pixel there is rejected even if it is faster. The 018 shape capture must also stay byte-identical.
- **Order is preserved.** Skipping invisible draws and changing a rectangle from 6 triangles to 1 sprite must not
  reorder visible primitives.
- **The vertex format is unchanged.** A solid rectangle uses `RL_QUADS` with no texture; the batcher's existing
  sprite selection decides the primitive. Do not add a new vertex format or a new public API.
- **A/B needs a library switch.** The cull and the rectangle path are in `libraylib.a`, so the sprint adds
  `PSP_CULL=0|1` (default 1) and `PSP_RECT_QUADS=0|1` (default 1) to `src/Makefile`. An app-only switch cannot
  A/B a library path.
- **No cap change.** `RLPSP_BATCH_CAP` stays 4,096. If a scene needs a second draw, record it.
- **No driver change.** pspgu stays `b24`; the 512-window path is untouched.
- **Stage only this sprint's paths.** Other sessions edit the tree; `git status` before every commit.
- Commit messages carry the task ID: `sprint 021 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
./tools/pspgu-build.sh                  # b24, unchanged

# Build the four library arms and run the matching scene.
cd src
make PLATFORM=PLATFORM_PSP -j8 PSP_CULL=0 PSP_RECT_QUADS=0
cp libraylib.a $PSPDEV/psp/lib/libraylib.a
# ... run scene, then rebuild with PSP_CULL=1 / PSP_RECT_QUADS=1 as needed.

cd ../psp-tests/scene_2d
make clean && make MODE=sprites COUNT=10000 OFFSCREEN=0.5
make clean && make MODE=shapes COUNT=1000
cp scene_2d.prx /tmp/s3/app.prx
cd /tmp/s3 && timeout 20 pspsh -e "reset"; sleep 6
( printf './app.prx\n'; sleep 16 ) | timeout 35 pspsh
pspsh -e "scrshot host0:/s21.raw"          # after DONE only
```

**Do not touch.** The text path, the 512-window splitter, the particle path, the allocator, and `DrawMesh`.

**Done means** §8: culling removes the off-screen work and vertices, rectangles submit as one sprite each, the
gate and the 018/019/020 captures are byte-identical, and the shapes work/draw table is recorded.

## 1. Objective

Four falsifiable results:

1. A **sprite scene** with a known off-screen fraction submits vertices only for the visible quads, and work
   falls because the skipped draws return before their math; the capture is unchanged.
2. A **solid rectangle** submits **2 vertices** instead of 6 (1,000 rectangles: 2,000 not 6,000), with identical
   pixels at the 2D gate and in the shape scene.
3. The **shape scene** drops from 4 draws and 10,000 vertices to at most 2 draws and 6,000 vertices, with work
   recorded before/after.
4. **Bind audit:** consecutive shapes do not produce a bind per shape; the frame's binds are the texture runs
   (zero or one for the solid path), not the object count.

## 2. Why now

Phase 7's exit criteria name sprites and shapes explicitly, and sprint 018 left shapes as the largest measured
2D cost. The window work (020) and text work (019) are closed; sprite rejection and the solid-quad path are the
last renderer changes before the particle and cache-strip sprints.

## 3. Scope

**In:** `psp-tests/scene_2d` sprite and shape modes; `src/rtextures.c` `DrawTexturePro` culling; `src/rshapes.c`
`DrawRectanglePro` culling and solid-quad submission; `src/Makefile` `PSP_CULL` and `PSP_RECT_QUADS`.

**Out:** text, windowing, particles, 3D, render targets (phase 10), the batch cap/arena, and any driver change.

## 4. Preconditions

- [ ] Sprint 020 closed at `9e34303d`; pspgu `b24`, gate `b69830b8…`
- [ ] 018 shape baseline (`4 draws, 10,000 vertices, 21.79 ms`) and capture are in `sprint018/`
- [ ] `batch_check` includes an untextured rectangle (it does) for the gate
- [ ] `GetScreenWidth()/GetScreenHeight()` are the render dimensions under PSP
- [ ] Device loop available; pspGL restorable (`2ab7378e`)

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Corpus**: `MODE=sprites COUNT OFFSCREEN` with deterministic positions; keep `MODE=shapes`; baseline both against `PSP_CULL=0 PSP_RECT_QUADS=0` | `todo` | objects/submitted vertices/work recorded; captures deterministic | |
| T2 | **API culling**: in `DrawTexturePro` and `DrawRectanglePro`, return before the corner math when `rotation == 0`, size is positive, the destination is entirely outside the screen, and no render target is active; `PSP_CULL=0\|1` | `todo` | 10,000 sprites at 50 % off-screen submit ~10,000 vertices not 20,000; work drops; capture identical | |
| T3 | **Solid rectangle path**: `DrawRectanglePro` on PSP emits `RL_QUADS` without a texture for solid rectangles when `PSP_RECT_QUADS=1`; the batcher turns each into one sprite; `=0` keeps the 6-vertex path | `todo` | 1,000 rectangles submit 2,000 vertices not 6,000; gate and shape capture byte-identical | |
| T4 | **Bind/run audit**: count binds vs object count in the shape and sprite scenes | `todo` | shapes: binds do not scale with 1,000 objects (0 or 1 per run); sprite scene: one bind per run; draws recorded | |
| T5 | **Regression**: 018/019/020 captures, `batch_check`, mixed, VRAM/residency | `todo` | all hashes unchanged; gate `b69830b8…`; 0 evictions | |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable.

### T1 design

- `MODE=sprites`: `COUNT` sprites (default 10,000) on a deterministic grid; `OFFSCREEN` is the fraction placed
  outside the 480×272 screen (default 0.5), alternating left/right/top/bottom so culling cannot be order-biased.
  Draw with `DrawTexturePro` at 32×32, one texture. Marker: `objects=COUNT visible=… submitted=…`.
- `MODE=shapes`: keep the 018 scene (1,000 each of rectangles, triangles, lines), group order rects → triangles
  → lines. The rectangle implementation is chosen by the **library** switch, not an app flag.
- Baseline with `PSP_CULL=0 PSP_RECT_QUADS=0`; the 018 numbers are the reference, and the same-session arm
  removes the code-layout doubt.

### T2 design

- `src/rtextures.c` `DrawTexturePro`, after the destination rectangle is normalised and before the corner math:
  ```c
  #if defined(PLATFORM_PSP) && (PSP_CULL == 1)
      if ((rotation == 0.0f) && (dstrec.width > 0.0f) && (dstrec.height > 0.0f))
      {
          float x = dstrec.x - origin.x;
          float y = dstrec.y - origin.y;
          if ((x + dstrec.width <= 0.0f) || (x >= (float)GetScreenWidth()) ||
              (y + dstrec.height <= 0.0f) || (y >= (float)GetScreenHeight())) return;
      }
  #endif
  ```
  `rlGetActiveFramebuffer() == 0` is the condition for "no render target"; add it so phase 10 does not inherit a
  wrong cull. The function must not touch texture state before returning.
- `src/rshapes.c` `DrawRectanglePro`, at the top with the same `rotation == 0`, positive size, screen-rect test,
  and the same guard. This also covers rectangles that are not solid? A gradient rectangle is a separate
  function; this path is solid only.
- `PSP_CULL` default is 1 in `src/Makefile`; `=0` compiles the cull out for the A/B. Do not put the cull in the
  batcher: that is after the four vertices the phase task asks to avoid.
- Note: text glyphs call `DrawTexturePro`, so off-screen glyphs are skipped too; tiles likewise. That is a
  benefit, but the sprite scene is the evidence.

### T3 design

- `DrawRectanglePro` on `PLATFORM_PSP` with `PSP_RECT_QUADS == 1` emits `rlSetTexture(0)`, `rlBegin(RL_QUADS)`,
  one colour, TL/BL/BR/TR, `rlEnd()`. The batcher's sprite test passes (axis-aligned, one colour, z=0, UVs all
  zero) and emits two vertices.
- `PSP_RECT_QUADS == 0` keeps the current `RL_TRIANGLES` branch. This is the A/B: two library builds, same app.
- Rotated rectangles can use the same quad emission; a rotated quad is not sprite-eligible, so the batcher
  chooses triangles and the result is the current 6-vertex path. Negative/degenerate sizes keep the old branch.
- `DrawRectangleGradientEx` and `DrawPoly` keep their current paths; this sprint changes solid rectangles only.
- The 2D gate contains an untextured rectangle, so pixel parity is checked directly. If the sprite rasterisation
  differs, revert T3 (`PSP_RECT_QUADS=0`) and record the null; T2 still stands.

### T4 design

- The `--wrap` counters already report binds/unbinds and array vertices. The app marker adds `visible` computed
  from `COUNT × (1 − OFFSCREEN)`; the submitted count must match the visible count (2 vertices each).
- Expect the shape scene's binds to be **0** for the solid path, or 1 per frame if the shapes texture is bound;
  in neither case 1,000. A bind per object is a bug.
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
| rectangle vertices | 2 per solid rect | the target; 6 is the baseline |
| cull decision | fewer submitted vertices and lower work | a vertex-only win with flat work is recorded separately |
| gate | byte-identical | the untextured rectangle is in the gate |

## 7. Measurements

Filled by T1–T5.

**Sprites:** total / visible / submitted vertices / draws / work, with `PSP_CULL=0` and `=1`.

**Shapes:** `PSP_RECT_QUADS=0` vs `=1`: vertices, draws, binds, work; capture hashes.

**Regression:** 018/019/020 hashes, gate, residency.

## 8. Exit criteria

- [ ] the sprite scene records total, visible and submitted vertices, and work with culling on and off
- [ ] culling reduces submitted vertices to the visible count and lowers work; capture identical
- [ ] a solid rectangle submits 2 vertices, not 6, with identical pixels at the gate and in the scene
- [ ] the shape scene is at most 2 draws / 6,000 vertices, with work recorded
- [ ] binds do not scale with object count in either scene
- [ ] 018/019/020 captures and the gate are byte-identical
- [ ] no driver (`tools/pspgu/`) change is in the sprint diff

## 9. Result

Not run yet.

## 10. Follow-ups

- **Sprint 022 — particles**: one dynamic stream per texture run, simulation separate, cap boundary.
- **Sprint 023 — cache strips, regression, phase close**.
- If T3 is a pixel null, `PSP_RECT_QUADS` stays 0 and the reason is recorded; the culling win (T2) is
  independent.
