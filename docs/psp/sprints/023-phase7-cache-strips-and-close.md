# Sprint 023 (P7-S6) — Texture-cache strips, full regression and phase 7 close

| | |
|---|---|
| Roadmap phase | [Phase 7 — 2D specialisation](../phases/07-2d-specialisation/index.md) |
| Sprint | P7-S6 |
| Branch / base commit | `master` @ `a696b30e` (sprint 022 closed) |
| Started / closed | — / — |
| Build number(s) | pspgu `b24`; `libraylib.a` `PSP_CULL=1 PSP_TRI_FAST=1`; `scene_2d` cache build |
| Tasks | 0/4 done |

## 0. Handover

**Situation.** Phase 7's renderer work is closed through sprint 022: text (019), 512-pixel windows and the wide
tile atlas (020), sprites/shapes (021), particles (022). What remains is the phase's measurement half and its exit
table. The **texture cache** has never been measured: the GE has 8 KiB, a 480×256 8888 row is 1,920 B (about four
rows), and recipe R2 claims strips pay only when texels repeat. This sprint prices the 16/32/64/128-px strip
widths on a single draw and on a repeated draw, adopts a width only if the repeated case moves, then runs the
whole phase's regression and fills the exit table.

**The background must be 480×256, not 480×272.** pspgu pads to powers of two: 480×272 8888 allocates
`512×512×4 = 1,048,576 B`, which does not fit beside the font (~913 KiB free). A **480×256 logical** image pads
to `512×256×4 = 524,288 B` and fits. The scene draws the 480×256 region 1:1; the bottom 16 rows stay black.

**Read before acting**, in this order:

1. `../phases/07-2d-specialisation/index.md` — R2 (strips), the exit criteria, the boot-budget and 512 tasks.
2. `../sprints/018-phase7-corpus-and-budget.md` (P7-S1) §7 — the corpus and the boot budget.
3. `../sprints/019`–`022` (P7-S2–P7-S5) — the text, window, sprite/shape and particle results this close depends on.
4. `../memory-map.md` §4 — the 8 KiB cache and the band arithmetic; §5 — the phase roadmap.
5. `psp-tests/scene_2d/main.c` — the scene harness and marker format.

**First action.** Add `MODE=cache STRIP=16|32|64|128 REPEAT=1|4` to `scene_2d`: a **480×256** 8888 background
drawn in vertical strips, strip-major order, repeated once or four times at the same position. Run the ten
combinations (16/32/64/128/480 px × repeat 1/4) before deciding anything.

**Hard rules.**

- **A strip is an ordering change, not a new texture.** One texture, one bind per frame; the strips change the
  order texels are read within the same draw. If the submission count changes, the scene is wrong.
- **Adopt only on the repeated case.** A 1:1 single draw reads every texel once; strips cannot reduce compulsory
  misses. If `REPEAT=4` does not move, record the null and keep the natural order.
- **Pixels are the contract.** Every strip width must produce the same capture as `STRIP=480` (the natural order).
- **The phase close is evidence, not a checkbox.** Every exit criterion is answered with a sprint number and a
  number; a deferred criterion names the phase and the reason.
- **Do not change the driver.** pspgu stays `b24`; no cap/arena change.
- **Stage only this sprint's paths.** Other sessions edit the tree; `git status` before every commit.
- Commit messages carry the task ID: `sprint 023 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp/psp-tests/scene_2d
make clean && make MODE=cache STRIP=16 REPEAT=4 MARKER=s23_s16_r4.txt
make clean && make MODE=cache STRIP=64 REPEAT=4 MARKER=s23_s64_r4.txt
make clean && make MODE=cache STRIP=480 REPEAT=4 MARKER=s23_s480_r4.txt
cp scene_2d.prx /tmp/s3/s23_app.prx
cd /tmp/s3 && timeout 20 pspsh -e "reset"; sleep 6
( printf './s23_app.prx\n'; sleep 20 ) | timeout 40 pspsh
pspsh -e "scrshot host0:/s23.raw"          # after DONE only
```

Use a unique PRX (`s23_app.prx`) and the per-scene `MARKER`; other sessions share `/tmp/s3`.

**Instrument check.** The gate reference depends on the original gray+alpha
`host0:/textures/bunnymark/wabbit_alpha.png` (md5 `5618cb98…`). Another session replaced it during sprint 022 and
the gate failed; check it before the regression. The boot line must say `pspgu b24` and
`RLGL: PSP_CULL=1 PSP_TRI_FAST=1`.

**Do not touch.** The text/window/shape/particle paths, the allocator, `DrawMesh`, and the 3D path.

**Done means** §8: the strip table, the decision, the full phase regression, and the Phase 7 page marked done with
every criterion answered.

## 1. Objective

Three falsifiable results:

1. A **cache-strip table** for 16/32/64/128/480 px at `REPEAT=1` and `REPEAT=4`, with work and captures; a width is
   adopted only if the repeated case moves and pixels match.
2. **Full regression**: every corpus capture from 018–022, `batch_check`, `basic_shapes` (284 colours), VRAM and
   residency, with no mid-frame upload or eviction in a fitting scene.
3. **Phase 7 closed**: every exit criterion answered with evidence or a named deferral; the phase page and index
   say done, and the next phase is identified.

## 2. Why now

R2 is the last unmeasured recipe in Phase 7 and the phase cannot close on a hypothesis. The regression is also
the point at which the drivers, fonts, windows and shapes are all in one build; a later phase would mix the
result with new changes.

## 3. Scope

**In:** `scene_2d MODE=cache`; the strip measurement and decision; the full capture/gate regression; the Phase 7
exit table and page/index updates; a recipe if a width is adopted.

**Out:** any renderer change beyond the cache scene, the driver, the cap/arena, 3D, render targets, and the
sample-porting phase.

## 4. Preconditions

- [ ] Sprints 018–022 closed; their captures are in `/home/psp/evidence/sprint0{18..22}/`
- [ ] pspgu `b24`; `libraylib.a` `PSP_CULL=1 PSP_TRI_FAST=1`; gate `b69830b8…`; pspGL restorable (`2ab7378e`)
- [ ] The gray+alpha `host0:/textures/bunnymark/wabbit_alpha.png` is restored (md5 `5618cb98…`), not the RGBA copy
- [ ] `basic_shapes` build and capture procedure available (phase 5 gate, 284 colours)
- [ ] Device loop available with reset between runs; unique PRX and `MARKER` per point

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Cache scene**: `MODE=cache STRIP REPEAT`, **480×256** 8888 background, strip-major, one texture, one bind | `todo` | captures identical across strip widths; one bind/frame; draws fixed | |
| T2 | **Strip measurement and decision**: 16/32/64/128/480 × repeat 1/4; adopt only if repeat moves | `todo` | work table; decision with the repeated case | |
| T3 | **Full regression**: 018–022 captures, gate, `basic_shapes` colours, VRAM/residency | `todo` | all hashes; 284 colours; 0 evictions; no mid-frame upload | |
| T4 | **Phase close**: exit table, Phase 7 page/index done, next phase named | `todo` | every criterion answered; docs strict build passes | |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable.

### T1 design

- `MODE=cache` in `scene_2d`: Makefile `STRIP ?= 480`, `REPEAT ?= 1`, both compiled as defines; `MARKER` is
  already supported. `sceneObjects = REPEAT × ceil(480/STRIP)`.
- Generate a **480×256** 8888 background (deterministic pattern: 32-px checker plus a gradient, so both repeated
  and varying texels exist); load one texture. pspgu pads it to **512×256 = 524,288 B**, which fits beside the
  font. The 480×256 region is drawn 1:1; the bottom 16 rows of the screen stay black.
- Draw in vertical strips of `STRIP` px: for `x0` in 0..480 step STRIP (the last strip is the remainder), draw a
  quad of `STRIP×256` with source `{x0,0,STRIP,256}` and destination `{x0,0,STRIP,256}`. The batcher merges the
  strips into one run (same texture); the vertex order is strip-major.
- `REPEAT=1` draws once; `REPEAT=4` draws the same order four times at the same position (overdraw), so the
  second/third/fourth passes can reuse cache lines.
- Marker: `cache strip=… repeat=… quads=… draws=… binds=… work=…`.

### T2 design

- Run 16/32/64/128/480 at repeat 1 and 4, two runs each, 300 frames.
- Decision rule: adopt a width only if `REPEAT=4` work is at least 5 % below the `STRIP=480` repeated case **and**
  the capture is byte-identical. A single-draw improvement is not enough (compulsory misses dominate).
- If no width wins, record the null with the table; the natural order stays. This is the expected result from R2
  and is a valid close.

### T3 design

- Rebuild the current library with `PSP_CULL=1 PSP_TRI_FAST=1`; record the boot line. Run text 1/8/24, tiles,
  particles 500/2,000, shapes, mixed, oversize 1024×128 and the wide tile atlas; compare to the 018–022 captures.
- `batch_check WRAP=0` vs `par_pspgl.raw`; check the wabbit texture md5 first.
- Build and run `basic_shapes`:
  ```sh
  cd psp-tests/basic_shapes
  make clean && make
  cp basic_shapes.prx /tmp/s3/s23_bs.prx
  # reset, run, DONE, capture to s23_bs.raw
  ```
  Count unique RGB triples after the 54-byte capture header; the phase 5 baseline is **284**. A different count
  must be explained before the phase closes.
- Record boot free/largest and residency before/after each fitting scene: 0 evictions/reloads/failures, no
  mid-frame upload.

### T4 design

- Fill the Phase 7 exit table:

  | criterion | evidence |
  |---|---|
  | text submissions ∝ atlas runs | 018/019: 1 draw/run at 1,637 glyphs, 9.069 ms |
  | tile map submissions ∝ atlas runs | 020: 8 window draws/28 tiles, 1 bind |
  | particles one stream per run | 022: 1 draw at 2,000; cap table |
  | every >512 path windows correctly | 020: byte-identical to pre-sliced refs |
  | cache behaviour measured | 023 T2 table |
  | no mid-frame upload/eviction | 018–023 residency counters |
  | boot 2D VRAM map | 018: free/largest/floor |
  | no visual regression | 018–023 captures + gate + 284 colours |

- Update `../phases/07-2d-specialisation/index.md` state to **done** (or record a named deferral), the sprint table, and
  `../phases/index.md`. Record the parallel state rather than inventing a successor: Phases 8 and 9 are already in
  progress by other sessions, and Phase 13 is continuous. Phase 7's close does not reorder or gate them.

## 6. Budgets

| item | budget | why |
|---|---|---|
| strip widths | 16/32/64/128/480 | the R2 set plus the natural order |
| repeats | 1 and 4 | single vs reused texels |
| background | 480×256 8888 logical; padded 512×256 = 524,288 B | 480×272 padded to 512×512 would be 1 MiB and would not fit |
| frames | 300, two runs | project rule |
| decision | ≥5 % on repeat, pixels identical | below that the reordering is not worth it |

## 7. Measurements

Filled by T1–T4.

**Strip table:** width × repeat, work run 1/run 2, draws/binds, capture match.

**Regression:** 018–022 hashes, gate `b69830b8…`, `basic_shapes` colours, VRAM/residency.

**Phase exit:** the table above, every row with a number or a named deferral.

## 8. Exit criteria

- [ ] the strip table is complete for 16/32/64/128/480 at repeat 1 and 4, with identical captures
- [ ] a width is adopted only if the repeated case moves; otherwise the null is recorded
- [ ] every 018–022 capture and the 2D gate are byte-identical
- [ ] `basic_shapes` is at 284 colours
- [ ] boot VRAM and residency show 0 evictions and no mid-frame upload in every fitting scene
- [ ] the Phase 7 exit table is filled; the page and index say done or name the deferral
- [ ] docs `mkdocs build --strict` passes

## 9. Result

Not run yet.

## 10. Follow-ups

- **Phase 8 — input** is already in progress in parallel, as is Phase 9; Phase 13 is continuous. Phase 7's close
  records the current roadmap state rather than naming a serial successor.
- The strip result, positive or null, is recorded here and in the memory map; no recipe is promoted on a single
  draw.
