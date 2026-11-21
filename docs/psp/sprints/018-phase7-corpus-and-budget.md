# Sprint 018 — 2D workload corpus, counters and boot-time VRAM budget

| | |
|---|---|
| Roadmap phase | [Phase 7 — 2D specialisation](../phases/07-2d-specialisation.md) |
| Branch / base commit | `master` @ `01325cf8` (sprint 017 closed; phase 6 done) |
| Started / closed | — / — |
| Build number(s) | `b23` (no driver change expected; the sprint is a harness) |
| Tasks | 0/7 done |

## 0. Handover

**Situation.** Phase 6 closed with pspgu owning VRAM: 512-byte block allocator, per-texture PSM, direct
8-row-band upload, residency/eviction (sprint 017: formats and sizes measured; CLUT and mipmaps dropped). The
renderer is fast on bunnymark (2,375 sprites at 60 fps) but **there is no workload for the four 2D shapes real
games are made of** — text runs, tile maps, shapes and particles — and no boot-time budget for a 2D working set.
Every Phase 7 fast-path claim would be unattributable without those scenes, and the phase's own preconditions say
so.

**Read before acting**, in this order:

1. `../phases/07-2d-specialisation.md` — the exit criterion ("text submissions scale with atlas runs, not glyph
   count; tile maps with atlas runs, not tile count"), the four workloads, and the 512-pixel rule.
2. `../memory-map.md` §3 and §“VRAM: 2 MB is the whole budget” — the framebuffer/list/arena map and the allocator
   the budget line is read from.
3. `../sprints/016-phase6-residency-eviction.md` T4 — the marker-file pattern and the `pspguVramReport` /
   `pspguResidencyStats` symbols this harness reuses.
4. `psp-tests/bunnymark/main.c` and `psp-tests/bunnymark/psp_counters.{c,h}` — the `SPLIT work/swap` timing and
   the `--wrap` counters at the GL boundary.
5. `psp-tests/batch_check/main.c` — the deterministic-scene and capture pattern; the 2D gate this sprint must not
   move.

**First action.** Create `psp-tests/scene_2d/` by copying the `batch_check` Makefile skeleton (including the
`../bunnymark/psp_counters` rule and `--wrap` flags) and write `main.c` with `MODE=text`, a fixed 78-character
line and a `DONE` marker. Do not touch `src/` or `tools/pspgu/` in this sprint.

**Hard rules.**

- **This sprint is measurement-only.** No renderer change, no driver change, no fast path. If a scene exposes a
  bug, record it and fix it in the sprint that owns that path.
- **Determinism is the instrument.** No `GetRandomValue`, no wall-clock animation and no time-based seed. Every
  scene is a pure function of `MODE`, `COUNT` and the frame index. Two runs of one build must produce identical
  captures for stationary scenes and identical vertex counts for moving ones.
- **Say which counter is the submission count.** At the GL boundary `ends` is one `glEnd` and `drawArrays` is one
  `glDrawArrays`; under pspgu the two are not interchangeable. Record both, state in the evidence which one the
  scene treats as a submission, and do not silently pick the flattering one.
- **Counters live at the boundary, not inside rlgl or pspgu.** The `--wrap` harness is the whole instrument; adding
  a counter inside a path being measured changes the path.
- **No mid-frame upload or eviction is a pass condition.** Compare `pspguResidencyStats` before and after the
  measured frames: `evictions`, `reloads` and `failed` must not move in a scene whose working set fits.
- **The 2D gate stays at 0 differing pixels.** `batch_check` on `b23` is byte-identical to `par_pspgl.raw`
  (`b69830b8…`); re-run it after the harness lands because a new test can still change the linked library use.
- **Capture after `DONE` only.** `scrshot` while the app renders freezes it; two identical captures are required
  before a screenshot is trusted. Never send `exit` to PSPLink.
- **Stage only this sprint's paths.** Other sessions edit the tree; `git status` before every commit and never
  `git add -A`.
- Commit messages carry the task ID: `sprint 018 T2: ...`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
./tools/pspgu-build.sh                  # default 8888, b23
./tools/pspgu-build.sh --verify-install
./tools/pspgl-build.sh --check          # only after --restore; must say 2ab7378e

cd psp-tests/scene_2d
make clean && make MODE=text COUNT=78 LINES=1
make clean && make MODE=tiles
make clean && make MODE=particles COUNT=2000
make clean && make MODE=shapes COUNT=1000
cp scene_2d.prx /tmp/s3/app.prx
cd /tmp/s3 && timeout 20 pspsh -e "reset"; sleep 6
( printf './app.prx\n'; sleep 40 ) | timeout 60 pspsh
# after DONE only:
pspsh -e "scrshot host0:/scene2d.raw"
pspsh -e "scrshot host0:/scene2d2.raw"
cmp /tmp/s3/scene2d.raw /tmp/s3/scene2d2.raw
```

**Do not touch.** `src/`, `tools/pspgu/`, the batcher, the parity scenes, the allocator/eviction policy, and
`DrawMesh`. The 512-pixel audit is a reading task in this sprint; the slicing code is Phase 7's later sprint.

**Done means** §8: five scenes run deterministically, each writes a marker with objects/submissions/binds/work,
the boot budget and in-frame residency are recorded, the 512 audit table is complete, and the 2D gate is 0.

## 1. Objective

Five falsifiable results:

1. A deterministic `scene_2d` app with `MODE=text|tiles|particles|shapes|mixed`, driven by `COUNT`/`LINES`/
   `FRAMES`, that writes a one-line `DONE` marker and sleeps.
2. Per-scene **logical objects** (glyphs, tiles, particles, shapes), **state runs** (expected texture changes) and
   the boundary counters (`ends`, `drawArrays`, `binds`, `unbinds`, `arrayVertices`) recorded at a fixed count.
3. A **boot-time 2D VRAM map**: free bytes and largest block after the font, atlas and particle texture are
   resident, with a floor below which the run logs `BUDGET FAIL`.
4. **Zero residency events** across the measured frames of every scene whose working set fits.
5. A **512-pixel audit**: every 2D path that can exceed 512×512 named with file/line, current behaviour and the
   windowing plan that a later sprint will implement.

## 2. Why now

Phase 7's preconditions name a representative workload per category and a boot-time budget, and its exit criteria
are all count-based ("submissions proportional to atlas runs"). Phase 6 closed the VRAM and residency machinery
this harness reads; Phase 5 closed the backend and left the batcher default-on. Without this sprint, the text,
tile-map, particle and shape paths cannot be A/B'd, and Phase 10 stays blocked behind Phase 7 by recorded
dependency.

## 3. Scope

**In:** a new `psp-tests/scene_2d/` app, the five scenes, reuse of the `--wrap` counters and `SPLIT` timing, the
boot VRAM/residency report, and the 512-pixel source audit written into §7.

**Out:** any `rtext.c`/`rshapes.c`/rlgl change (sprints 019–022), window-slicing implementation (020), the
texture-cache strip experiment (023), mipmaps/CLUT (phase 10 / closed in 017), and pspGL as a comparison driver
(the A/B driver switch stays available, but this sprint's baseline is the shipping `b23` build).

## 4. Preconditions

- [ ] Phase 6 closed at `01325cf8`; pspgu `b23` builds and is pixel-gated
- [ ] `psp-tests/bunnymark/psp_counters.c` compiles against the current `libraylib.a` with `--wrap`
- [ ] `pspguVramReport` and `pspguResidencyStats` are callable from a test app (the `texture_residency` pattern)
- [ ] `/tmp/s3/par_pspgl.raw` is present (`b69830b8…`) for the gate
- [ ] Device loop available: `usbhostfs_pc` rooted at `/tmp/s3`, `pspsh`, `reset` between runs
- [ ] pspGL archive restorable (`2ab7378e`)

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Harness**: `psp-tests/scene_2d` with `MODE`, `COUNT`, `LINES`, `FRAMES`, `TARGET_FPS=0`; fixed LCG; per-frame times; `SPLIT work/swap`; marker file | `todo` | two runs of one build give identical counters and captures; marker has mode/count/frames/work/swap | |
| T2 | **Text scene**: fixed 78-character line(s) at fontSize 10; `LINES=1|8|24`; count glyphs, runs, `ends`, `drawArrays`, `binds`; capture | `todo` | submissions and binds reported per glyph count; two runs identical; screenshot stable | |
| T3 | **Tile scene**: 128×128 atlas of 16×16 tiles; 30×17 visible grid from a 120×68 logical map; optional `SCROLL=1`; count tiles/runs/binds | `todo` | 510 visible tiles, one atlas; counters and capture recorded; scrolling build deterministic per frame count | |
| T4 | **Particle scene**: 2,000 deterministic particles, one 32×32 texture; simulation timed separately from rendering with `sceKernelGetSystemTimeWide`; wrap-around motion | `todo` | `sim` and `work` reported separately; two runs at the same frame count have identical counters and pixels | |
| T5 | **Shape scene**: 1,000 rectangles, 1,000 triangles, 1,000 lines in compatible contiguous groups; count runs/submissions/vertices | `todo` | three groups, counts recorded; no texture binds except the default | |
| T6 | **Boot budget and in-frame residency**: after assets, `pspguVramReport("boot2d")` + `pspguResidencyStats`; `BUDGET_FLOOR_KB` (default 128); re-read after the measured frames; `BUDGET FAIL` if free < floor or residency events moved | `todo` | free/largest printed; floor verdict printed; evictions/reloads/failed unchanged over the frames | |
| T7 | **512-pixel audit**: every 2D path that can exceed 512×512, with file/line, current behaviour, whether `tbw` is full-stride, and the window plan | `todo` | a table in §7 with no "unknown" rows for the four workloads | |

Status vocabulary: `todo` / `doing` / `done` / `blocked` / `dropped`. IDs are stable.

### T1 design

- Files: `psp-tests/scene_2d/main.c`, `Makefile`. The Makefile copies `batch_check`'s pattern: `OBJS = main.o
  psp_counters.o`, `-I../bunnymark`, the seven `--wrap` flags (`glBegin`, `glEnd`, `glVertex2f`, `glVertex2i`,
  `glBindTexture`, `glDrawArrays`, `glDrawElements`, `eglSwapBuffers`), and the `psp_counters.o` rule pointing at
  `../bunnymark/psp_counters.c`.
- `MODE` selects one scene at compile time (one macro, as `texture_residency` does); `COUNT`, `LINES`, `FRAMES`
  and `BUDGET_FLOOR_KB` have defaults. `SCROLL=1` is a tile-scene compile flag.
- One LCG (`uint32_t s; s = s*1664525u + 1013904223u;`) seeded with a constant; all positions come from it.
- Timing: copy `bunnymark`'s `frameTimes[]`, sort, print `STAT n min median p95 worst`; `SPLIT n work swap
  swapcalls` with `work = avg_frame - avg_swap`. `TARGET_FPS=0` for every timing run.
- Marker: `host0:/scene2d_done.txt`, one line:
  `DONE mode=<m> objects=<n> runs=<r> begins=<b> ends=<e> draws=<d> binds=<bi> unbinds=<u> verts=<v> work=<ms>
  swap=<ms> median=<ms> p95=<ms> free=<B> largest=<B> evict=<n> reload=<n> failed=<n>`, then
  `sceKernelSleepThread()`.
- Capture: render `FRAMES` frames, log, sleep; keep the last frame on screen.

### T2 design

- Default font (`GetFontDefault()`), `fontSize = 10`, `spacing = 0`, `position = (4, 4)`, line height 10 px.
- The line is a fixed 78-character ASCII sentence with no tabs; `LINES` repeats it at `y + 10*line`. 24 lines
  is 240 px, inside 272.
- Drawn glyphs are counted by walking the string with `GetCodepointNext` and skipping space/tab/newline, so the
  reported `objects` is the number that reaches `DrawTextCodepoint`; state the formula in the evidence.
- Texture runs expected: **1** (the default font atlas). If `binds` exceeds 1, that is a finding, not a failure.
- Record the font texture line from `[PSPGU] texture … PSM … (B)`; at `b23` the 128×128 default font is 65,536 B
  (8888).

### T3 design

- Build a 128×128 atlas with `GenImageColor` + 16×16 cells (64 cells, eight colours), `LoadTextureFromImage`.
- Fixed map coordinates: `tile = (x*7 + y*13) % 64`; draw the 30×17 visible grid at 16 px, `DrawTexturePro` per
  tile. `objects = 510`, expected runs = 1.
- `SCROLL=1` offsets the map by one tile every 60 frames; the measured counters must be identical to the
  stationary build at the same `FRAMES` (same tile count, same bind count), and the capture is deterministic for
  a fixed `FRAMES`.
- The logical map is 120×68 = 8,160 tiles; only the viewport is submitted in this sprint. Sprint 020 measures a
  scrolled traversal and the culling path.

### T4 design

- Texture: `GenImageColor(32, 32, ...)` with a four-quadrant pattern so a wrong UV is visible.
- Particle state: `pos`, `vel`, `colour` from the LCG; each frame `pos += vel`, wrap at the 480×272 edges.
  `COUNT` particles drawn with `DrawTexturePro` at 32×32 (overlap is expected; this is a fill workload too).
- Simulation timing: two `sceKernelGetSystemTimeWide()` reads around the update loop, summed per frame; `sim`
  printed as a separate average. `work` includes simulation; do not subtract it silently.
- Run at `COUNT=500` and `COUNT=2000`, `FRAMES=300`, two runs each.

### T5 design

- Three contiguous groups: 1,000 `DrawRectangle`, 1,000 `DrawTriangle`, 1,000 `DrawLineEx` (or `DrawLine`),
  positions from the LCG, fixed colours, no rotation.
- Record `begins`, `ends`, `drawArrays`, `arrayVertices`, `binds`. The shapes texture is the default white texel;
  a non-zero `binds` here is a finding (rshapes should not rebind per shape).
- The existing PSP guard at `src/rshapes.c:1489` (`SUPPORT_QUADS_DRAW_MODE && !defined(PLATFORM_PSP)`) is read,
  not changed; the sprint records which primitives take which path.

### T6 design

- After all assets are loaded, declare the driver's test symbols exactly as `texture_residency` does:
  `void pspguVramReport(const char *); void pspguResidencyStats(PspGuResidencyStats *);`.
- Print `VRAM [boot2d]` and `RESIDENCY [boot2d]`; the existing boot/assets lines stay.
- Floor: `free >= BUDGET_FLOOR_KB*1024` and `largest >= 32*1024`; otherwise `BUDGET FAIL free=… largest=…
  floor=…` in the marker and on the console. A failure is evidence for the CLUT revisit, not a reason to evict.
- After the measured frames, read `pspguResidencyStats` again; require the same `evictions`, `reloads`, `failed`.
  Any movement means the scene does not fit and the budget must be reduced before a fast-path claim is made.
- A `mixed` mode draws all four scenes in one frame to check that combining runs does not create an upload and
  that the bind count is the sum of the runs.

### T7 design

- Audit list, at minimum: `DrawTexturePro`/`DrawTextureEx`/`DrawTextureRec`/`DrawTextureV` (destination larger than
  512), `DrawTextEx`/`DrawTextCodepoint` (atlas size), `DrawTextureNPatch`/`DrawRectangleGradientEx` (any), tile
  sheets loaded through `LoadTexture` (the upload path rejects width > 512), and `BeginScissorMode` (does it clip
  before vertices?). For each: file:line, whether the path can exceed 512 in x/y, whether `tbw` is full stride
  today, and the window split (x/y tiles) a later sprint must implement.
- Do not implement slicing or change the texture-size rejection in this sprint; the audit is the input to
  sprint 020.

## 6. Budgets

| item | budget | why |
|---|---|---|
| measured frames per timed point | 300 (120 for the first pass) | enough for a median and p95, short enough to keep a device run under 40 s |
| runs per point | 2 | the project's repeatability rule; report both |
| scene app size | no target | it is a test, not shipped code |
| VRAM floor (boot, 2D set) | 128 KiB free, 32 KiB largest | below this the working set is tight enough to revisit T4/CLUT; the number is recorded, not enforced by eviction |
| residency movement in a fitting scene | 0 evictions, 0 reloads, 0 failed | a mid-frame upload invalidates the timing |

## 7. Measurements

Filled by T1–T7. The tables must carry the build marker (`[PSPGU] GU up: pspgu b23 …`) and the run command.

**Baseline counters (to fill):**

| scene | objects | runs | ends | drawArrays | binds | unbinds | verts | work ms | swap ms | median ms |
|---|---|---|---|---|---|---|---|---|---|---|
| text LINES=1 | | | | | | | | | | |
| text LINES=8 | | | | | | | | | | |
| text LINES=24 | | | | | | | | | | |
| tiles 30×17 | 510 | 1 | | | | | | | | |
| particles 500 | | | | | | | | | | |
| particles 2000 | | | | | | | | | | |
| shapes 1000×3 | | | | | | | | | | |
| mixed | | | | | | | | | | |

**Boot budget (to fill):** `free=… largest=… floor=…` after the font + atlas + particle texture; residency
before/after the measured frames.

**512-pixel audit (to fill):**

| path | file:line | can exceed 512? | `tbw` today | window plan |
|---|---|---|---|---|
| | | | | |

**Gate:** `batch_check` on the sprint build vs `par_pspgl.raw`, byte comparison and SHA-256.

## 8. Exit criteria

- [ ] `scene_2d` builds with `MODE=text|tiles|particles|shapes|mixed` and writes a complete `DONE` marker
- [ ] every scene is deterministic: two runs at the same `COUNT`/`FRAMES` have identical counters and captures
- [ ] objects, runs and boundary counters are recorded for each scene at the stated counts
- [ ] the boot-time VRAM map is printed, with free/largest and the floor verdict
- [ ] every fitting scene has 0 evictions, 0 reloads and 0 failed allocations across the measured frames
- [ ] the 512-pixel audit table is complete for the four workloads and the texture upload path
- [ ] `batch_check` is byte-identical to `par_pspgl.raw` on the sprint build
- [ ] no `src/` or `tools/pspgu/` file is in the sprint diff

## 9. Result

Not run yet.

## 10. Follow-ups

- **Sprint 019 — text runs and layout cache**: consecutive glyphs of one atlas in one run; cache glyph layout
  where raylib semantics allow; verify short labels and paragraphs, screenshot parity.
- **Sprint 020 — tile maps and 512-pixel windowing**: per-atlas-run tile rendering, scrolled traversal, window
  slicing from the T7 audit with full-stride `tbw`, seam screenshots.
- **Sprint 021 — sprites and shapes**: offscreen rejection before vertex generation, unrotated/uniform-scale fast
  paths, shapes texture held across compatible runs.
- **Sprint 022 — particles**: one dynamic stream per texture run, double/triple buffered, simulation cost kept
  separate from rendering.
- **Sprint 023 — texture-cache strips, regression, phase close**: 16/32/64/128 px strip A/B on the repeated-draw
  case only; full sample screenshot comparison; phase 7 exit table. If the strips are a null result (R2 predicts
  they may be), 023 collapses into 022 and the phase closes after 022.

The CLUT decision from sprint 017 is revisited only if T6 reports free below the floor or a later scene cannot fit
its working set; the measured 8,256 vs 32,768 B and 0.080 vs 0.532 ms are the inputs.
