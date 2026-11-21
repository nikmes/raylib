# Sprint 018 — 2D workload corpus, counters and boot-time VRAM budget

| | |
|---|---|
| Roadmap phase | [Phase 7 — 2D specialisation](../phases/07-2d-specialisation/index.md) |
| Branch / base commit | `master` @ `01325cf8` (sprint 017 closed; phase 6 done) |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | `b23` (driver unchanged; the sprint is a harness) |
| Tasks | 7/7 done |

## 0. Handover

**Situation.** Phase 6 closed with pspgu owning VRAM: 512-byte block allocator, per-texture PSM, direct
8-row-band upload, residency/eviction (sprint 017: formats and sizes measured; CLUT and mipmaps dropped). The
renderer is fast on bunnymark (2,375 sprites at 60 fps) but **there is no workload for the four 2D shapes real
games are made of** — text runs, tile maps, shapes and particles — and no boot-time budget for a 2D working set.
Every Phase 7 fast-path claim would be unattributable without those scenes, and the phase's own preconditions say
so.

**Read before acting**, in this order:

1. `../phases/07-2d-specialisation/index.md` — the exit criterion ("text submissions scale with atlas runs, not glyph
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

- [x] Phase 6 closed at `01325cf8`; pspgu `b23` builds and is pixel-gated
- [x] `psp-tests/bunnymark/psp_counters.c` compiles against the current `libraylib.a` with `--wrap`
- [x] `pspguVramReport` and `pspguResidencyStats` are callable from a test app (the `texture_residency` pattern)
- [x] `/tmp/s3/par_pspgl.raw` is present (`b69830b8…`) for the gate
- [x] Device loop available: `usbhostfs_pc` rooted at `/tmp/s3`, `pspsh`, `reset` between runs
- [x] pspGL archive restorable (`2ab7378e`)

## 5. Tasks

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Harness**: `psp-tests/scene_2d` with `MODE`, `COUNT`, `LINES`, `FRAMES`, `TARGET_FPS=0`; fixed LCG; per-frame times; `SPLIT work/swap`; marker file | `done` | two runs of one build give identical counters and captures; marker has mode/count/frames/work/swap | Five modes, nine configurations, **18 runs**, all `PASS`; every marker is one line with mode/objects/runs/counters/work/swap/sim/median/p95/free/largest/residency/verdict; two runs per point agree to ≤0.1 % on work and byte-for-byte on captures. `/home/psp/evidence/sprint018/` (`*_run{1,2}.txt|raw|log`, `SHA256SUMS`) |
| T2 | **Text scene**: fixed 78-character line(s) at fontSize 10; `LINES=1|8|24`; count glyphs, runs, `ends`, `drawArrays`, `binds`; capture | `done` | submissions and binds reported per glyph count; two runs identical; screenshot stable | **63 / 548 / 1,637 glyphs → 1 draw/frame at all three** (126 / 1,096 / 3,274 vertices, under the 4,096 cap); binds **1 per run**, unbinds 0 (the PSP batcher defers `rlSetTexture`); work **1.226 / 6.827 / 19.680 ms**. Text24 misses 60 fps: median **33.367 ms**. Captures identical across runs |
| T3 | **Tile scene**: 128×128 atlas of 16×16 tiles; 30×17 visible grid from a 120×68 logical map; optional `SCROLL=1`; count tiles/runs/binds | `done` | 510 visible tiles, one atlas; counters and capture recorded; scrolling build deterministic per frame count | 510 tiles, **1 draw/frame**, 1,020 vertices, binds **1 per run**; work **3.228 ms**. `SCROLL=1`: work **3.244 ms**, same counters, view differs from the stationary capture; two scroll runs identical |
| T4 | **Particle scene**: `COUNT` deterministic particles, one 32×32 texture; simulation timed separately with `GetTime()` (the `SPLIT` clock); wrap-around motion | `done` | `sim` and `work` reported separately; two runs at the same frame count have identical counters and pixels | 500: work **3.285 ms**, sim **0.443 ms**; 2,000: work **11.844 ms**, sim **1.775 ms**. **1 draw/frame** at both (1,000 / 4,000 vertices; 4,000 is just under the cap). Captures identical across runs |
| T5 | **Shape scene**: 1,000 rectangles, 1,000 triangles, 1,000 lines in compatible contiguous groups; count runs/submissions/vertices | `done` | three groups, counts recorded; bind behaviour recorded, including the default texture | 3,000 objects, **4 draws/frame**, **10,000 vertices/frame**, **1 bind + 1 unbind per frame** for the default shapes texture; work **21.789 ms**, median **33.367 ms**. The bind is per frame, not per shape |
| T6 | **Boot budget and in-frame residency**: after assets, `pspguVramReport("boot2d")` + `pspguResidencyStats`; `BUDGET_FLOOR_KB` (default 128); re-read after the measured frames; `BUDGET FAIL` if free < floor or residency events moved | `done` | free/largest printed; floor verdict printed; evictions/reloads/failed unchanged over the frames | Boot free: **916,992 B** (text, shapes), **851,456 B** (tiles), **912,896 B** (particles), **847,360 B** (mixed); largest = free in every case; floor 131,072 B → **OK** everywhere. **0 evictions, 0 reloads, 0 failed** across all 300-frame runs |
| T7 | **512-pixel audit**: every 2D path that can exceed 512×512, with file/line, current behaviour, whether `tbw` is full-stride, and the window plan | `done` | a table in §7 with no "unknown" rows for the four workloads | Audit in §7. **Finding:** width > 512 is not rejected — the texture is allocated and marked used, but the swizzled upload returns early (`gl_texture.c:231`), so it samples zero/uninitialised VRAM. `tbw = nextPow2(width)` today (`gl_texture.c:566`) |

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
- Simulation timing: two `GetTime()` reads around the update loop, summed per frame; `sim`
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

Build marker on every run: `[PSPGU] GU up: pspgu b23, 480x272 stride 512, texfmt 8888, fb 272 KiB x3, depth
yes (272 KiB), lists 2 x 240 KiB, arenas 2 x 128 KiB`. Nine configurations, **two runs each**, `FRAMES=300`,
`TARGET_FPS=0`, marker plus two captures per run. Evidence: `/home/psp/evidence/sprint018/`
(`<tag>_run{1,2}.txt|raw|log`, `gate_b23.raw`, `SHA256SUMS`).

Two runs of one build agree to ≤0.1 % on work; every pair of captures is byte-identical, and for the stationary
scenes the captures are identical across runs too. A post-cleanup rebuild (preprocessor guards only) reproduced
the text-8 counters exactly, work 6.856 vs 6.827/6.829 ms — a 0.4 % run-to-run difference. `ends` and `begins` are
0 in every scene: the PSP batcher is default-on, so all drawing reaches the boundary as `glDrawArrays`, not
immediate calls.

**Per-frame submissions (totals below are for 300 frames):**

| scene | objects/frame | declared runs | draws/frame | binds/run | unbinds/run | vertices/frame |
|---|---:|---:|---:|---:|---:|---:|
| text LINES=1 | 63 | 1 | **1** | 1 | 0 | 126 |
| text LINES=8 | 548 | 1 | **1** | 1 | 0 | 1,096 |
| text LINES=24 | 1,637 | 1 | **1** | 1 | 0 | 3,274 |
| tiles 30×17 | 510 | 1 | **1** | 1 | 0 | 1,020 |
| tiles 30×17 `SCROLL=1` | 510 | 1 | **1** | 1 | 0 | 1,020 |
| particles 500 | 500 | 1 | **1** | 1 | 0 | 1,000 |
| particles 2,000 | 2,000 | 1 | **1** | 1 | 0 | 4,000 |
| shapes 1,000×3 | 3,000 | 1 | **4** | 300 | 300 | 10,000 |
| mixed | 2,158 | 4 | **4** | 900 | 300 | 3,916 |

Reading: text, tiles and particles already produce **one submission per atlas run** in this range — 1,637
glyphs is 3,274 vertices, inside the 4,096-vertex batch cap. Text therefore already meets the phase's
submission criterion up to about 2,048 glyphs; its cost is vertex generation, not submission count. Shapes are
**cap-bound** (10,000 vertices → 4 draws) and pay one bind and one unbind per frame for the default texture,
not one per shape. Mixed is **run-bound** (4 draws for four textures) and pays three binds per frame.

**Timing (run 1 / run 2):**

| scene | work ms | swap ms | median ms | sim ms |
|---|---|---|---|---|
| text LINES=1 | 1.226 / 1.226 | 15.504 / 15.503 | 16.683 / 16.683 | 0.021 |
| text LINES=8 | 6.827 / 6.829 | 9.901 / 9.899 | 16.684 / 16.683 | 0.020 |
| text LINES=24 | 19.680 / 19.673 | 13.734 / 13.738 | 33.367 / 33.367 | 0.021 |
| tiles | 3.228 / 3.229 | 13.558 / 13.556 | 16.684 / 16.684 | 0.020 |
| tiles `SCROLL=1` | 3.244 / 3.246 | 13.541 / 13.537 | 16.684 / 16.683 | 0.023 |
| particles 500 | 3.285 / 3.285 | 13.443 / 13.446 | 16.683 / 16.683 | **0.443** |
| particles 2,000 | 11.844 / 11.843 | 4.940 / 4.940 | 16.683 / 16.683 | **1.775** |
| shapes 1,000×3 | 21.789 / 21.781 | 11.625 / 11.631 | 33.367 / 33.372 | 0.021 |
| mixed | 14.745 / 14.743 | 2.096 / 2.096 | 16.684 / 16.683 | 0.442 |

The median stays pinned to 16.683 ms while work is under the vblank: `eglSwapBuffers` is still paced by the
display even at `TARGET_FPS=0`, so work and swap trade against each other. Only text 24 lines and shapes miss
60 fps (median 33.367 ms); at those points the work already exceeds 16.683 ms.

**Boot budget (after the font and each scene's assets, before the measured frames):**

| scene | assets | free B | largest B | floor B | verdict | evictions / reloads / failed |
|---|---|---:|---:|---:|---|---|
| text | default font 65,536 | 916,992 | 916,992 | 131,072 | OK | 0 / 0 / 0 |
| shapes | default font 65,536 | 916,992 | 916,992 | 131,072 | OK | 0 / 0 / 0 |
| tiles | font + 128² atlas 65,536 | 851,456 | 851,456 | 131,072 | OK | 0 / 0 / 0 |
| particles | font + 32² texture 4,096 | 912,896 | 912,896 | 131,072 | OK | 0 / 0 / 0 |
| mixed | font + atlas + 32² texture 135,168 | 847,360 | 847,360 | 131,072 | OK | 0 / 0 / 0 |

The fixed reservations are `reserved=1,114,112` B (3 × 272 KiB colour + 272 KiB depth); the font is already
resident by `boot2d`, so the free figure is after it. Largest equals free in every case: one span, no
fragmentation at boot. Residency counters are unchanged across all 300 measured frames.

**512-pixel audit:**

| path | file:line | can exceed 512? | `tbw` today | window plan |
|---|---|---|---|---|
| `DrawTexturePro` (all `DrawTexture*` delegate here) | `src/rtextures.c:4876` | **Destination** may be >512, but that is not a GE limit — UVs stay inside the source. Source is limited by the upload path below | n/a | No destination slicing. Enforce source ≤512 |
| `DrawTextureNPatch` | `src/rtextures.c:5001` | Nine quads from one source; destination may be >512; source as above | n/a | No destination slicing; split the source texture if it is oversized |
| `DrawTextEx` / `DrawTextCodepoint` | `src/rtext.c:1228`, `src/rtext.c:1282` | Default atlas 128×128 is safe. `LoadFontEx` builds the atlas with `GenImageFontAtlas` (`src/rtext.c:810`), which grows width to a POT and doubles height on overflow — a large charset can exceed 512 | `nextPow2(width)` | Cap the atlas at 512, or per-atlas-run windows; sprint 019 |
| Tile / background sheet upload | `tools/pspgu/gl_texture.c:566` (`tbw = nextPow2(width)`), `:231` (swizzle guard) | **Yes.** `tbw > 512` is not rejected: the block is allocated, `t->used = 1`, then `uploadShadowToVram` logs and returns — the texture samples zero/uninitialised VRAM | `nextPow2(width)` | Split the sheet at load, or sample 512-pixel windows with full-stride `tbw`; sprint 020 |
| `BeginScissorMode` / `EndScissorMode` | `src/rcore.c:1092`, `src/rlgl.h:2256` | Not a texture limit; the scissor clips at the GE, but both calls flush the batch and reject no vertices | n/a | Optional: reject draws wholly outside the scissor rect before vertex generation |
| Shapes quads on PSP | `src/rshapes.c:1489` | No texture limit: the PSP path is per-triangle because unsigned 16-bit UVs wrap; the default 1×1 white texel is used | n/a | None for size; the per-triangle path is the recorded reason, not a bug |

**Finding for sprint 020.** The dangerous case is a wide texture that *fits VRAM*: for example a 640×64 8888
texture has `tbw = 1024` and 262,144 B, so the allocation succeeds, but the swizzled upload is skipped and the
GE samples whatever was in the block. This contradicts the memory-map's "textures wider than 512 are rejected"
wording — they are allocated and marked used, not rejected. Sprint 020 owns the fix (split or fail loudly); the
audit also gives sprint 019 the custom-font atlas case.

**Gate:** `batch_check` (`WRAP=0`) on `b23` vs `/tmp/s3/par_pspgl.raw`: **byte-identical**, SHA-256
`b69830b86a899cc083146402e6333df43007286dd1278dbdbf05db6d9e357e7c`; two captures in the same run also
identical. Evidence `/home/psp/evidence/sprint018/gate_b23.raw`.

## 8. Exit criteria

- [x] `scene_2d` builds with `MODE=text|tiles|particles|shapes|mixed` and writes a complete `DONE` marker
- [x] every scene is deterministic: two runs at the same `COUNT`/`FRAMES` have identical counters and captures
- [x] objects, runs and boundary counters are recorded for each scene at the stated counts
- [x] the boot-time VRAM map is printed, with free/largest and the floor verdict
- [x] every fitting scene has 0 evictions, 0 reloads and 0 failed allocations across the measured frames
- [x] the 512-pixel audit table is complete for the four workloads and the texture upload path
- [x] `batch_check` is byte-identical to `par_pspgl.raw` on the sprint build
- [x] no `src/` or `tools/pspgu/` file is in the sprint diff

## 9. Result

Sprint 018 closed 7/7 with nine device configurations, two runs each, and no renderer or driver change. The
corpus now exists and every later Phase 7 claim has a baseline:

- **Text already submits per atlas run.** 63, 548 and 1,637 glyphs each produce **1 draw/frame** (3,274 vertices
  at the top point, inside the 4,096 cap) and no per-glyph bind. Work is the cost: **1.226 / 6.827 / 19.680 ms**.
  Sprint 019 should target vertex/layout cost, not submission count; above about 2,048 glyphs the cap will add
  a second draw, which 019 can measure with `LINES=32`.
- **Tiles and particles are also one draw/frame** at 510 tiles and 2,000 particles. Tile work is **3.228 ms**;
  particle work is **3.285 ms at 500** and **11.844 ms at 2,000**, with simulation **0.443 / 1.775 ms** measured
  separately.
- **Shapes are the outlier**: 3,000 objects make **4 draws and 10,000 vertices per frame**, and cost **21.79 ms**,
  plus one bind and one unbind per frame for the default texture. That is the case sprint 021 is for.
- **Mixed** (four textures, 2,158 objects) is run-bound at **4 draws/frame, 3 binds/frame, 14.745 ms**, which
  confirms that texture changes, not vertex count, set the submission floor in a combined scene.
- **VRAM is comfortable**: boot free is **847,360–916,992 B**, largest equals free, and every 300-frame run has
  **0 evictions, 0 reloads, 0 failed**. The CLUT revisit is not triggered.
- **512 finding:** width > 512 is allocated and marked used but silently not uploaded (`gl_texture.c:231`), so a
  wide texture that fits VRAM samples uninitialised memory. This corrects the memory-map wording ("rejected")
  and is the first task of sprint 020.

The 2D gate is unchanged: byte-identical to pspGL (`b69830b8…`). No `src/` or `tools/pspgu/` file changed.

## 10. Follow-ups

- **Sprint 019 — text runs and layout cache**: the baseline already has one draw per atlas run, so the sprint's
  target is **work**: cached glyph layout, no re-shaping per frame, and the batch-cap boundary above 2,048
  glyphs. Verify short labels and a 32-line paragraph, screenshot parity.
- **Sprint 020 — tile maps and 512-pixel windowing**: fix the silent >512 upload first (split at load or fail
  loudly), then per-atlas-run tile rendering, scrolled traversal, full-stride `tbw` windows and seam screenshots.
  The custom-font atlas case is shared with 019.
- **Sprint 021 — sprites and shapes**: offscreen rejection before vertex generation; the shape scene's 4 draws
  and 10,000 vertices are the before/after; unrotated/uniform-scale sprite paths; keep the shapes texture bound
  across compatible runs.
- **Sprint 022 — particles**: one dynamic stream per texture run, double/triple buffered, simulation kept
  separate. The baseline is already 1 draw at 2,000, so the target is vertex-generation cost, not submission.
- **Sprint 023 — texture-cache strips, regression, phase close**: 16/32/64/128 px strip A/B on the repeated-draw
  case only; full sample screenshot comparison; phase 7 exit table. If the strips are a null result (R2 predicts
  they may be), 023 collapses into 022 and the phase closes after 022.

The CLUT decision from sprint 017 is revisited only if T6 reports free below the floor or a later scene cannot fit
its working set; the measured 8,256 vs 32,768 B and 0.080 vs 0.532 ms are the inputs.
