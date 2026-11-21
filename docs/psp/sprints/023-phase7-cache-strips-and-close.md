# Sprint 023 — Texture-cache strips, full regression and phase 7 close

| | |
|---|---|
| Roadmap phase | [Phase 7 — 2D specialisation](../phases/07-2d-specialisation/index.md) |
| Branch / base commit | `master` @ the sprint 022 close (this doc is written before 021/022 run) |
| Started / closed | — / — |
| Build number(s) | pspgu `b24` (unchanged); `scene_2d` builds |
| Tasks | 0/4 done |

## 0. Handover

**Situation.** Phase 7's renderer work is in place or scheduled: text (019), 512-pixel windows and the wide tile
atlas (020), sprites/shapes (021), particles (022). What remains is the phase's measurement half and its exit
table. The **texture cache** has never been measured: the GE has 8 KiB, a 480×272 8888 row is 1,920 B (about four
rows), and recipe R2 claims strips pay only when texels repeat. This sprint prices the 16/32/64/128-px strip
widths on a single draw and on a repeated draw, adopts a width only if the repeated case moves, then runs the
whole phase's regression and fills the exit table.

**Read before acting**, in this order:

1. `../phases/07-2d-specialisation/index.md` — R2 (strips), the exit criteria, the boot-budget and 512 tasks.
2. `../sprints/018-phase7-corpus-and-budget.md` §7 — the corpus and the boot budget.
3. `../sprints/019`–`022` — the text, window, sprite/shape and particle results this close depends on.
4. `../memory-map.md` §4 — the 8 KiB cache and the band arithmetic; §5 — the phase roadmap.
5. `psp-tests/scene_2d/main.c` — the scene harness and marker format.

**First action.** Add `MODE=cache STRIP=16|32|64|128 REPEAT=1|4` to `scene_2d`: a 480×272 8888 background drawn in
vertical strips, strip-major order, repeated once or four times at the same position. Run the eight combinations
before deciding anything.

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
make clean && make MODE=cache STRIP=16 REPEAT=4
make clean && make MODE=cache STRIP=64 REPEAT=4
make clean && make MODE=cache STRIP=480 REPEAT=4
cp scene_2d.prx /tmp/s3/app.prx
cd /tmp/s3 && timeout 20 pspsh -e "reset"; sleep 6
( printf './app.prx\n'; sleep 20 ) | timeout 40 pspsh
pspsh -e "scrshot host0:/s23.raw"          # after DONE only
```

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
- [ ] pspgu `b24`; gate `b69830b8…`; pspGL restorable (`2ab7378e`)
- [ ] `basic_shapes` build and capture procedure available (phase 5 gate, 284 colours)
- [ ] Device loop available with reset between runs

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Cache scene**: `MODE=cache STRIP REPEAT`, 480×272 8888 background, strip-major, one texture, one bind | `todo` | captures identical across strip widths; one bind/frame; draws fixed | |
| T2 | **Strip measurement and decision**: 16/32/64/128/480 × repeat 1/4; adopt only if repeat moves | `todo` | work table; decision with the repeated case | |
| T3 | **Full regression**: 018–022 captures, gate, `basic_shapes` colours, VRAM/residency | `todo` | all hashes; 284 colours; 0 evictions; no mid-frame upload | |
| T4 | **Phase close**: exit table, Phase 7 page/index done, next phase named | `todo` | every criterion answered; docs strict build passes | |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable.

### T1 design

- Generate a 480×272 8888 background (deterministic pattern: 32-px checker plus a gradient, so both repeated
  and varying texels exist); load one texture. VRAM 522,240 B, fits beside the font (~394 KiB free).
- Draw in vertical strips of `STRIP` px: for `x0` in 0..480 step STRIP, for `y` 0..272 step 1 (a quad per strip,
  272 tall), `DrawTexturePro` with source `{x0,0,STRIP,272}` and destination `{x0,0,STRIP,272}`. The batcher
  merges the strips into one run (same texture); the vertex order is strip-major.
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

- Rebuild the current library; run text 1/8/24, tiles, particles 500/2,000, shapes, mixed, oversize 1024×128 and
  the wide tile atlas; compare to the 018–022 captures.
- `batch_check WRAP=0` vs `par_pspgl.raw`.
- Build and run `basic_shapes`; capture and count colours against the phase 5 baseline **284**.
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
  `../phases/index.md`. Name the next phase (8 — input, unless the recorded order changes).

## 6. Budgets

| item | budget | why |
|---|---|---|
| strip widths | 16/32/64/128/480 | the R2 set plus the natural order |
| repeats | 1 and 4 | single vs reused texels |
| background | 480×272 8888 = 522,240 B | fits with the font |
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

- **Phase 8 — input** after Phase 7 closes (or the recorded order if a deferral changes it).
- The strip result, positive or null, is recorded here and in the memory map; no recipe is promoted on a single
  draw.
