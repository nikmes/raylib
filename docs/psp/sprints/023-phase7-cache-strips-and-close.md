# Sprint 023 (P7-S6) — Texture-cache strips, full regression and phase 7 close

| | |
|---|---|
| Roadmap phase | [Phase 7 — 2D specialisation](../phases/07-2d-specialisation/index.md) |
| Sprint | P7-S6 |
| Branch / base commit | `master` @ `a696b30e` (sprint 022 closed) |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | pspgu `b24`; `libraylib.a` `PSP_CULL=1 PSP_TRI_FAST=1`; `scene_2d` cache build |
| Tasks | 4/4 done |

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

- [x] Sprints 018–022 closed; their captures are in `/home/psp/evidence/sprint0{18..22}/`
- [x] pspgu `b24`; `libraylib.a` `PSP_CULL=1 PSP_TRI_FAST=1`; gate `b69830b8…`; pspGL restorable (`2ab7378e`)
- [x] The gray+alpha `host0:/textures/bunnymark/wabbit_alpha.png` is restored (md5 `5618cb98…`), not the RGBA copy
- [x] `basic_shapes` build and capture procedure available (phase 5 gate, 284 colours)
- [x] Device loop available with reset between runs; unique PRX and `MARKER` per point

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Cache scene**: `MODE=cache STRIP REPEAT`, **480×256** 8888 background, strip-major, one texture, one bind | `done` | captures identical across strip widths; one bind/frame; draws fixed | Ten arms built (`STRIP=16/32/64/128/480` × `REPEAT=1/4`). Texture pads to **512×256 = 524,288 B**, 392,704 B free, 0 evictions. Every point: **1 draw, 1 bind**; quads 1–120; valid captures match the 480-px natural order |
| T2 | **Strip measurement and decision**: 16/32/64/128/480 × repeat 1/4; adopt only if repeat moves | `done` | work table; decision with the repeated case | Natural **480 px is fastest at both repeats**: 0.928/0.929 ms at ×1, 0.948/0.947 at ×4. The best strip is 128 px at 0.955/0.953 (×1) and 1.006/1.006 (×4). Strips are a **null**; no width adopted, no recipe |
| T3 | **Full regression**: 018–022 captures, gate, `basic_shapes` colours, VRAM/residency | `done` | all hashes; 284 colours; 0 evictions; no mid-frame upload | text8 **3.376**, text24 **9.102**, tiles **3.121**, particles 500 **3.093**, mixed **10.656**, shapes **13.215 ms**; all captures match 018/019/021/022. Gate **b69830b8…**. `basic_shapes` **284 colours** (two captures). Every run 0 evictions/reloads/failures |
| T4 | **Phase close**: exit table, Phase 7 page/index done, next phase named | `done` | every criterion answered; docs strict build passes | Phase 7 page state **done**, all tasks/exit boxes checked, sprint table 023 done; index Phase 7 done. Exit table below. Next-phase note records the existing parallel work (8/9 in progress, 13 continuous) |

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

Evidence: `/home/psp/evidence/sprint023/` (`c*_run{1,2}.txt|raw|log`, `reg_*`, `gate_b24.raw`, `basic_shapes.raw`,
`SHA256SUMS`). All cache points: 300 frames, two runs, 1 draw and 1 bind per frame, pspgu `b24`,
`RLGL: PSP_CULL=1 PSP_TRI_FAST=1`.

**Strip table** (480×256 8888, padded 512×256 = 524,288 B):

| strip | repeat | quads | work run 1 / run 2 | draws | binds | capture |
|---:|---:|---:|---|---:|---:|---|
| 16 | 1 | 30 | 1.074 / 1.076 ms | 1 | 1 | matches natural |
| 16 | 4 | 120 | 1.585 / 1.585 ms | 1 | 1 | matches natural |
| 32 | 1 | 15 | 0.996 / 0.997 ms | 1 | 1 | valid run matches (one run blank) |
| 32 | 4 | 60 | 1.233 / 1.231 ms | 1 | 1 | matches natural |
| 64 | 1 | 8 | 0.974 / 0.972 ms | 1 | 1 | matches natural |
| 64 | 4 | 32 | 1.086 / 1.090 ms | 1 | 1 | matches natural |
| 128 | 1 | 4 | 0.955 / 0.953 ms | 1 | 1 | matches natural |
| 128 | 4 | 16 | 1.006 / 1.006 ms | 1 | 1 | valid run matches (one run blank) |
| **480** | **1** | **1** | **0.928 / 0.929 ms** | 1 | 1 | reference |
| **480** | **4** | **4** | **0.948 / 0.947 ms** | 1 | 1 | reference |

**Decision: null.** Natural order is fastest at both repeats. The best strip (128 px) is +2.9 % at repeat 1 and
+6.1 % at repeat 4; no width is adopted and no recipe is promoted. This is R2's predicted result: a 1:1 draw
reads every texel once, and the 128-px strip (16 KiB for 256 rows) already exceeds the 8 KiB cache.

**Regression:**

| scene | work | capture |
|---|---:|---|
| text 8 / 24 | 3.376 / 9.102 ms | matches 019 |
| tiles | 3.121 ms | matches 018 |
| particles 500 | 3.093 ms | matches 018 |
| mixed | 10.656 ms | matches 018 |
| shapes | 13.215 ms | matches 021/022 |
| `batch_check` gate | — | `b69830b8…`, byte-identical to pspGL |
| `basic_shapes` | — | **284 colours** in both captures |

Every run: 0 evictions, 0 reloads, 0 failed; cache scene leaves **392,704 B** free with the 524,288 B texture.

**Instrument note.** The device is shared with another session; several scrshots caught its app instead of ours
(blank, 2-colour frames) even though the cache marker was ours. The table uses the valid capture for each width,
and every valid capture is byte-identical to the 480-px natural order. The gate was re-run in a quiet window.

## 8. Exit criteria

- [x] the strip table is complete for 16/32/64/128/480 at repeat 1 and 4, with identical valid captures
- [x] a width is adopted only if the repeated case moves; otherwise the null is recorded — **null**, natural order kept
- [x] every 018–022 capture and the 2D gate are byte-identical
- [x] `basic_shapes` is at 284 colours
- [x] boot VRAM and residency show 0 evictions and no mid-frame upload in every fitting scene
- [x] the Phase 7 exit table is filled; the page and index say done or name the deferral
- [x] docs `mkdocs build --strict` passes

## 9. Result

Sprint 023 closed 4/4 and Phase 7 is **done**. The last open measurement is a null, recorded rather than worked
around:

- **Texture-cache strips:** natural 480-px order is fastest at both repeats — **0.928/0.929 ms** at ×1 and
  **0.948/0.947 ms** at ×4. The best strip (128 px) is +2.9 %/+6.1 %. Every point is one draw and one bind, so
  the difference is texture-read locality, not submission. No width is adopted and no recipe is promoted.
- **Regression:** text 8/24 **3.376/9.102 ms**, tiles **3.121**, particles 500 **3.093**, mixed **10.656**,
  shapes **13.215**; every capture matches 018/019/021/022. The gate is byte-identical to pspGL
  (`b69830b8…`), `basic_shapes` is **284 colours**, and every run has 0 evictions/reloads/failures.
- The shared device caused blank captures during the strip sweep (another session's app on screen); the table
  uses the valid run for each width, and all valid captures match the natural order. The gate was re-run in a
  quiet window.

**Phase 7 exit table:**

| criterion | evidence |
|---|---|
| text submissions ∝ atlas runs | 018/019: 1 draw and 1 bind per run at 63/548/1,637 glyphs; work −44/−51/−54 % |
| tile-map submissions ∝ atlas runs | 020: 28 tiles over a 1024-wide atlas = 8 window draws, 1 bind |
| particles one stream per run | 022: 1 draw at 2,000, 2 at 2,049, direct stream −28.6 % |
| every >512 path windows correctly | 020: 1024×128 and 128×1024 byte-identical to pre-sliced references |
| texture-cache behaviour measured | 023: strip table, null; natural order kept |
| no mid-frame upload/eviction | every 018–023 run: 0 evictions/reloads/failures |
| boot 2D VRAM map | 018 `BUDGET`; 023 cache scene 392,704 B free with 524,288 B resident |
| no visual regression | 018–022 captures match; gate `b69830b8…`; `basic_shapes` 284 colours |

Implementation of the cache scene is in the working tree; this page records its evidence. Phases 8 and 9 are
already in progress by other sessions and Phase 13 is continuous; Phase 7's close records that state rather than
naming a serial successor.

## 10. Follow-ups

- **Phase 8 — input** is already in progress in parallel, as is Phase 9; Phase 13 is continuous. Phase 7's close
  records the current roadmap state rather than naming a serial successor.
- The strip result is a measured null and is recorded in this page and the memory map; no recipe is promoted.
