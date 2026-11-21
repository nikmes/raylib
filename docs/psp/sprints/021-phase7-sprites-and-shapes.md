# Sprint 021 — Sprites and shapes: culling and solid-quad submission

| | |
|---|---|
| Roadmap phase | [Phase 7 — 2D specialisation](../phases/07-2d-specialisation.md) |
| Branch / base commit | `master` @ `9e34303d` (sprint 020 closed) |
| Started / closed | — / — |
| Build number(s) | pspgu `b24` (unchanged); `libraylib.a` with the batcher/rshapes changes |
| Tasks | 0/5 done |

## 0. Handover

**Situation.** Sprint 018 measured the two 2D workloads that are still expensive: the shape scene (1,000
rectangles + 1,000 triangles + 1,000 lines) emits **10,000 vertices and 4 draws/frame at 21.79 ms**, and sprites
are already one draw per texture run but pay for quads that are entirely off-screen. The batcher chooses a
2-vertex GE sprite for an axis-aligned quad with one colour (`src/rlgl.h:rlPspIsSpriteQuad`), yet solid
rectangles on PSP take `src/rshapes.c:DrawRectanglePro`'s per-triangle branch — six vertices each, and the
`DrawRectanglePro` quads that raylib uses on other platforms are explicitly disabled for PSP. The phase's tasks
are therefore (a) reject off-screen sprites before vertex generation and (b) give solid rectangles the quad/sprite
path without the unsigned-TEXCOORD wrap that made the PSP guard necessary.

**Read before acting**, in this order:

1. `../sprints/018-phase7-corpus-and-budget.md` §7 — shapes and mixed counters, the 4-draw/10,000-vertex baseline.
2. `../sprints/020-phase7-tilemap-and-windowing.md` §9 — the current driver and gate state.
3. `src/rlgl.h:622-930` — `RLPspBatch`, `rlPspIsSpriteQuad`, `rlPspPushVertex`, the alternation budget.
4. `src/rshapes.c:618-710` — `DrawRectanglePro` and the PSP per-triangle branch; `src/rshapes.c:1489` — the
   `DrawPoly` guard and why it exists.
5. `psp-tests/scene_2d/main.c` — `MODE=shapes` and the text/scene harness.

**First action.** Add `MODE=sprites` to `scene_2d` (a fixed count with a known off-screen fraction) and
`RECT_IMPL=api|quad` to the shape scene; run both before changing `src/`. The culling claim is measured as
submitted vertices, not as a frame-time guess.

**Hard rules.**

- **The 2D gate is the contract.** `batch_check` draws an untextured rectangle; a rectangle change that moves one
  pixel there is rejected even if it is faster. The 018 shape capture must also stay byte-identical when the
  implementation is `api`.
- **Cull only what cannot contribute.** A quad is rejected only when it is axis-aligned, `z == 0`, has one
  colour, and all four corners lie outside the viewport. A rotated quad whose corners are outside can still cover
  the screen; never cull it.
- **Order is preserved.** Culling invisible quads and changing a rectangle from 6 triangles to 1 sprite must not
  reorder visible primitives.
- **The vertex format is unchanged.** A solid rectangle uses `RL_QUADS` with no texture; the batcher's existing
  sprite selection decides the primitive. Do not add a new vertex format or a new public API.
- **The viewport comes from `rlViewport`.** Do not hardcode 480×272 in the batcher; add `viewW/viewH` to
  `RLPspBatch`, set them in `rlViewport`, and leave culling off until they are set (render targets are phase 10).
- **No cap change.** `RLPSP_BATCH_CAP` stays 4,096. If a scene needs a second draw, record it.
- **Stage only this sprint's paths.** Other sessions edit the tree; `git status` before every commit.
- Commit messages carry the task ID: `sprint 021 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
cd src && make PLATFORM=PLATFORM_PSP -j8 && cp libraylib.a $PSPDEV/psp/lib/libraylib.a
./tools/pspgu-build.sh                  # b24, unchanged

cd psp-tests/scene_2d
make clean && make MODE=sprites COUNT=10000 OFFSCREEN=0.5
make clean && make MODE=shapes COUNT=1000 RECT_IMPL=api
make clean && make MODE=shapes COUNT=1000 RECT_IMPL=quad
cp scene_2d.prx /tmp/s3/app.prx
cd /tmp/s3 && timeout 20 pspsh -e "reset"; sleep 6
( printf './app.prx\n'; sleep 16 ) | timeout 35 pspsh
pspsh -e "scrshot host0:/s21.raw"          # after DONE only
```

**Do not touch.** The text path, the 512-window splitter, the particle path, the allocator, and `DrawMesh`.

**Done means** §8: culling removes the off-screen vertices, rectangles submit as one sprite each, the gate and
the 018/019/020 captures are byte-identical, and the shapes work/draw table is recorded.

## 1. Objective

Four falsifiable results:

1. A **sprite scene** with a known off-screen fraction submits vertices only for the visible quads, and work
   falls in proportion; the capture is unchanged.
2. A **solid rectangle** submits **2 vertices** instead of 6 (1,000 rectangles: 2,000 not 6,000), with identical
   pixels at the 2D gate and in the shape scene.
3. The **shape scene** drops from 4 draws and 10,000 vertices to at most 2 draws and 6,000 vertices, with work
   recorded before/after.
4. **Bind audit:** consecutive shapes do not produce a bind per shape; the frame's binds are the texture runs
   (one for the solid path), not the object count.

## 2. Why now

Phase 7's exit criteria name sprites and shapes explicitly, and sprint 018 left shapes as the largest measured
2D cost. The window work (020) and text work (019) are closed; sprite rejection and the solid-quad path are the
last renderer changes before the particle and cache-strip sprints.

## 3. Scope

**In:** `psp-tests/scene_2d` sprite and shape modes; `src/rlgl.h` batcher culling and viewport bounds;
`src/rshapes.c` solid-rectangle submission.

**Out:** text, windowing, particles, 3D, render targets (phase 10), the batch cap/arena, and any driver change.

## 4. Preconditions

- [ ] Sprint 020 closed at `9e34303d`; pspgu `b24`, gate `b69830b8…`
- [ ] 018 shape baseline (`4 draws, 10,000 vertices, 21.79 ms`) and capture are in `sprint018/`
- [ ] `batch_check` includes an untextured rectangle (it does) for the gate
- [ ] Device loop available; pspGL restorable (`2ab7378e`)

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Corpus**: `MODE=sprites COUNT OFFSCREEN` with deterministic positions; `MODE=shapes RECT_IMPL=api\|quad`; baseline both | `todo` | objects/submitted vertices/work recorded; captures deterministic | |
| T2 | **Off-screen culling**: `viewW/viewH` in `RLPspBatch`, set by `rlViewport`; drop an axis-aligned, one-colour, z=0 quad when all corners are outside; no other quad type | `todo` | 10,000 sprites at 50 % off-screen submit ~10,000 vertices not 20,000; capture identical; visible sprites unchanged | |
| T3 | **Solid rectangle path**: `DrawRectanglePro` on PSP emits `RL_QUADS` without a texture for solid rectangles; the batcher turns each into one sprite | `todo` | 1,000 rectangles submit 2,000 vertices not 6,000; gate and shape capture byte-identical | |
| T4 | **Bind/run audit**: count binds vs object count in the shape and sprite scenes; assert one bind per texture run | `todo` | shapes: binds do not scale with 1,000 objects; sprite scene: one bind per run; draws recorded | |
| T5 | **Regression**: 018/019/020 captures, `batch_check`, mixed, VRAM/residency | `todo` | all hashes unchanged; gate `b69830b8…`; 0 evictions | |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable.

### T1 design

- `MODE=sprites`: `COUNT` sprites (default 10,000) on a deterministic grid; `OFFSCREEN` is the fraction placed
  outside the 480×272 screen (default 0.5), alternating left/right/top/bottom so culling cannot be order-biased.
  Draw with `DrawTexturePro` at 32×32, one texture. Marker: `objects=COUNT visible=… submitted=…`.
- `MODE=shapes`: keep the 018 scene (1,000 each of rectangles, triangles, lines) and add
  `RECT_IMPL=api|quad`; the only difference is the rectangle submission. The group order stays rects → triangles
  → lines.
- Baseline both before any `src/` change; two runs each.

### T2 design

- `RLPspBatch` adds `int viewW, viewH; int culled;`. `rlViewport` sets `viewW/viewH` (and leaves them 0 until
  called, which disables culling).
- In `rlPspPushVertex`, after the fourth corner is assembled and before the mode decision:
  - `int sprite = rlPspIsSpriteQuad(rlpsp.quad);`
  - if `sprite` and `viewW>0` and `(maxX<0 || minX>viewW || maxY<0 || minY>viewH)` → `rlpsp.culled++`, reset
    `quadN`, return. No vertices are reserved.
  - otherwise continue with the existing alternation-budget decision.
- The submitted-vertex counter at the GL boundary (`--wrap` `arrayVerts`) is the evidence; culled quads never
  reach `glDrawArrays`.
- Do not cull when the texture is a render target (no FBO yet; the `viewW==0` state covers it) and do not cull
  rotated quads even if their corners are outside.

### T3 design

- `DrawRectanglePro` on `PLATFORM_PSP`:
  - If the rectangle is solid (no gradient) and the position/size are finite, emit `rlSetTexture(0)`,
    `rlBegin(RL_QUADS)`, one colour, TL/BL/BR/TR, `rlEnd()`. The batcher's sprite test passes (axis-aligned,
    one colour, z=0, UVs all zero) and emits two vertices.
  - Keep the existing per-triangle branch for anything that does not qualify (rotation? a rotated quad is not
    sprite-eligible, so it stays triangles; the same code path can emit a quad and the batcher chooses triangles).
  - `DrawRectangleGradientEx` and `DrawPoly` keep their current paths; this sprint changes solid rectangles only.
- The 2D gate contains an untextured rectangle, so pixel parity is checked directly. If the sprite rasterisation
  differs, revert T3 and record the null result; T2 still stands.

### T4 design

- The `--wrap` counters already report binds/unbinds and array vertices. Add a marker field `visible` computed
  by the app (count of quads intersecting the screen) so the submitted count can be compared.
- Assert in the evidence: shape binds are 1 per frame (or 1 per run) not 1,000; sprite binds are 1 per texture
  run; culled quads reduce `arrayVerts` by 2 per culled sprite.

### T5 design

- Rebuild `libraylib.a`; run text 8/24, tiles, particles, mixed, oversize and the wide tile atlas; compare to the
  018/019/020 captures. Run `batch_check WRAP=0` vs `par_pspgl.raw`. Record VRAM/residency.

## 6. Budgets

| item | budget | why |
|---|---|---|
| sprite scene | 10,000, 50 % off-screen | a large enough culling signal without a long frame |
| shape scene | 1,000 + 1,000 + 1,000 | the 018 corpus, unchanged |
| measured frames | 300, two runs | project rule |
| rectangle vertices | 2 per solid rect | the target; 6 is the baseline |
| gate | byte-identical | the untextured rectangle is in the gate |

## 7. Measurements

Filled by T1–T5.

**Sprites:** total / visible / submitted vertices / work / draws, with culling on and off.

**Shapes:** rectangles API vs quad: vertices, draws, work, binds; capture hashes.

**Regression:** 018/019/020 hashes, gate, residency.

## 8. Exit criteria

- [ ] the sprite scene records total, visible and submitted vertices, and work before/after culling
- [ ] culling reduces submitted vertices by 2 per fully off-screen axis-aligned sprite; capture identical
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
- If T3 is a pixel null, the rectangle path stays per-triangle and the reason is recorded; the culling win (T2)
  is independent.
