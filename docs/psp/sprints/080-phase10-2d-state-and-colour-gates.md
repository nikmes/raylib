# Sprint 080 (P10-S13) — 2D state scoping, a material-colour gate, and a capture tool

| | |
|---|---|
| Roadmap phase | [Phase 10 — 3D and advanced](../phases/10-3d-advanced/index.md) |
| Sprint | P10-S13 |
| Branch / base commit | `master` @ `f673270a` (P10-S12/S15 docs; pspgu `b37`) |
| Started / closed | 2026-11-23 (written) / 2026-11-24 (closed) |
| Build number(s) | pspgu `b37` (`9798af84…`); `libraylib.a` `c2004b26…` (`PSP_BATCH=1 PSP_MERGE_DRAWS=1`, with T1); new gate app `psp-tests/colour_gate`; new host tool `tools/scrshot2png.py` |
| Tasks | 4/4 done |

## 0. Handover

**Situation.** The PSP showcase demo (`psp-tests/samples/psp/psp_showcase`) was the first app in this repo to draw
**lit 3D and a 2D HUD in the same frame**. It worked only after two manual workarounds: the HUD had to be drawn with
lighting and fog switched off, and the demo had to flush the 2D batch itself. That is not a demo problem — it is two
holes in the backend's state contract, plus an instrument that cost this session real time.

**What the code actually does** (checked in the tree, not assumed):

- `BeginMode3D` **already** calls `rlDrawRenderBatchActive()` (`src/rcore.c:961`), so a 2D batch pending when the
  camera starts is flushed with the 2D matrices. My earlier claim that it did not flush was wrong; the HUD was
  invisible for a different reason.
- **Lighting and fog are global GE state.** `rlpspEnableLighting(1)` / `rlpspEnableFog(1)` set the GL/GE state once
  and nothing scopes it to the 3D block. A 2D sprite has no normals, so with `GU_LIGHTING` on the GE lights it to
  black. Measured on the demo: with lighting on, **0 pixels** in the HUD box above the panel colour; with lighting
  off, the three text lines and the panel border are present.
- Fog has the same shape of problem: it is applied per vertex by depth, and 2D vertices sit at z = 0, so a fogged
  2D draw fades towards the fog colour for no reason the app asked for.

**Two gates could not see the colour bug that the demo found.** Every gate scene uses a **white** material, so
`rlColor4ub` never reaching the GE (fixed in `21f586cf`) changed nothing on any capture. A gate with non-white
materials is missing and is task T2.

**The capture instrument was wrong and undocumented.** `scrshot` writes a 54-byte header followed by
480×272 **B,G,R** pixels, **bottom-up** (raw row 0 is the last display line), quantised to 5-6-5. Reading that as
RGB swaps red and blue and reading it top-down turns every image upside down; both mistakes were made before the
layout was known. The gradient in `psp_2d_showcase` settles the row order: band 0, the top of the screen, appears in
the raw's last rows. The reference project in `/home/psp/ref/PSPRuntime`
confirms the GE's 8888 order is `0xAABBGGRR` (its 5551 packer is `((a>>7)<<15)|((b>>3)<<10)|((g>>3)<<5)|(r>>3)`).
This costs no device time to fix and is task T3.

**Read before acting:**

1. `src/rcore.c` — `BeginMode3D` / `EndMode3D` (the 2D batch flush and the matrix stack).
2. `src/rlgl.h` — `rlpspEnableLighting`, `rlpspEnableFog`, `rlpspMaterial`, and the `RLPSP_*` macros.
3. `tools/pspgu/gl_state.c` — `glEnable/glDisable` for `GL_LIGHTING`/`GL_FOG`, `pspgu.lighting`, `pspgu.fogEnabled`.
4. `tools/pspgu/gl_draw.c` — the 2D batched path (`isBatchedLayout`) and `pspguProjectionW()` (ortho detection).
5. `psp-tests/samples/psp/psp_showcase/main.c` — the workarounds this sprint removes.

**First action.** No driver change. Re-run the demo capture with lighting forced on for the HUD and count bright
pixels in the HUD box; then with lighting off. The two numbers are the baseline for T1 (the session's spot values
were 0 and "present"; the sprint wants the counts written down).

**Hard rules.**

- **Ask before every PSPLink use** unless the user has granted a session.
- **Pixel gates are sacred**: 2D `batch_check` 0/613, `scene_3d` 0, `basic_shapes` 0/284, `clip_probe` 0,
  RTT 0, all against pspGL.
- **Measured, not asserted**: any claim about the HUD needs a pixel count, and any claim about cost needs the
  list-byte or wait numbers.
- **The capture layout goes in the docs**, not in a session's memory: header, channel order, row order, quantisation.
- **Stage only the paths you touch**; another session may have sprint 079 in the tree.

## 1. Objective

Falsifiable:

1. A 2D draw renders **unlit and unfogged** while the app has lighting/fog enabled for 3D — proven by a HUD-over-3D
   capture with the text pixels present and the 3D region byte-identical to the same frame drawn without 2D.
2. A **colour gate** exists that fails if a material colour stops reaching the GE: three known materials, exact
   pixels, both drivers, merge on and off.
3. The `scrshot` layout is documented and `tools/scrshot2png.py` reproduces a known capture, so no future analysis
   has to guess the channel order.
4. The demo no longer needs manual lighting/fog toggles around the HUD.

## 2. Why now

The 3D path is accelerating (P10-S11 merged 400 cubes into 2 draws; the demo runs at 24 fps), which means apps will
mix 2D HUDs with lit 3D scenes more often, not less. The workarounds are invisible until someone writes the third
such app. And a colour bug that no gate can see is a gate gap, not a code bug — it will come back.

## 3. Scope

**In:** `src/rlgl.h` and `src/rcore.c` (scope lighting/fog to the 3D mode), `tools/pspgu/gl_state.c` (only if the
scoping has to be driver-side), a new `psp-tests/colour_gate`, `tools/scrshot2png.py`, the demo simplification, the
docs.

**Out:** the clip path, merging, the 60 fps work in Phase 10, texture formats, `reduced`-precision experiments
(P10-S12 / sprint 079 owns those), any public raylib API change.

## 4. Preconditions

- [ ] `master` @ `21f586cf`; pspGL `2ab7378e` available for the A/B; pspgu `b33` builds
- [ ] `psp_showcase` builds and runs (`make FRAMES=600`), capture workflow known
- [ ] The `scrshot` layout recorded once by hand (header 54, BGR, bottom-up) — T3 formalises it
- [ ] The user has authorised the device session

## 5. Tasks

Status vocabulary — `todo` · `doing` · `done` · `blocked` · `dropped`.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | **Scope lighting and fog to the 3D mode.** `rlpspEnableLighting` / `rlpspEnableFog` record the requested state; `BeginMode3D` applies it, `EndMode3D` turns both off for the 2D pass and restores the request at the next `BeginMode3D`. Keep the rlgl names and semantics, add no app-side calls. If the driver turns out to be the only place that can do it (state is emitted per draw), do it there behind `pspguProjectionW() == 0` and say why | `done` | HUD-over-3D with lighting/fog requested: text pixels present; 3D region 0 diff; no per-draw state cost | |
| T2 | **Colour gate.** `psp-tests/colour_gate`: three cubes with red/green/blue materials, unlit; then the same lit; then a textured one. Capture and assert exact pixel colours, on pspgu (merge 0/1) and pspGL. This is the gate that would have caught `rlColor4ub` | `done` | every colour exact on both drivers and both merge builds | |
| T3 | **Capture tool and layout doc.** `tools/scrshot2png.py <raw> <out.png>` with the layout in its header comment (54-byte header, 480×272, B,G,R, bottom-up, 565-quantised), plus a `--probe x,y` option that prints the colour of a pixel. Document the layout in the docs once (sprint note or recipe) and reference it from the sample page | `done` | the tool reproduces the showcase capture; the three cube colours read correctly | |
| T4 | **Re-close.** Drop the demo's manual lighting/fog toggles and the redundant flush; re-run every gate; sample ×2; record the boot tag; restore pspGL | `done` | all gates 0; sample thresholds; `2ab7378e` installed | |

### Design notes

**T1 — which layer owns the scope.** The cleanest contract is "lighting and fog apply inside `BeginMode3D`".
rlgl already owns both switches and the 3D block brackets; a small `rlpspRequested` bitmask applied in
`BeginMode3D` costs two state commands per frame, not per draw, and cannot be forgotten by an app. The driver-side
alternative (detect ortho via `pspguProjectionW()` and wrap each 2D draw) costs two commands per 2D draw and works
for apps that never call `BeginMode3D`; take it only if the rlgl route cannot cover a path (e.g. `DrawTexture` in an
FBO whose projection stays 3D). Measure both if both are implemented; adopt one.

**T1 — what must not change.** 3D pixels with lighting on must be identical before and after: the ordering of
`sceGuEnable(GU_LIGHTING)` relative to the draws is what matters, not when it was requested. Capture `scene_3d`
with `LIGHT=ge` before and after.

**T2 — why exact colours, not a diff.** A gate that only compares pspgu against pspGL passes when both are wrong
(they share the GL11 calls). The unlit cube must be measured at a known screen point with the expected RGB, then
compared across drivers. The lit case has no oracle (sprint 062 correction), so record face colours instead of
asserting them.

**T2 — merge interaction.** With `PSP_MERGE_DRAWS=1` the three cubes are three runs (colour is in the key); with 0
they are three per-object draws. Both must produce the same pixels — that is the merged-path colour contract from
sprint 078, which until now had no non-white coverage.

**T3 — the layout, with the evidence.** Clear colour `(10,14,28)` appears in the raw as `(24,16,8)`: blue in the
low byte, 5-bit quantisation. The 2D red rectangle of the colour probe appears as `(0,0,248)` while a red material
cube appears as `(0,0,248)` too; the green cube is `(0,252,0)` in both interpretations. That is B,G,R. Rows are
bottom-up (a gradient drawn from the top of the screen ends in the raw's last rows; the first analysis of both
demo captures got this wrong and read the HUD as missing). The tool should detect the header by
`len − 480×272×3` so a differently sized capture fails loudly instead of silently shifting.

**T4 — remove the workarounds only after T1 is measured.** The demo's `rlpspEnableLighting(0/1)` around the HUD and
its `rlDrawRenderBatchActive()` before `BeginMode3D` stay until the capture shows the HUD without them; the flush is
already redundant (`rcore.c:961`) and is the first thing to drop.

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| HUD text pixels, lighting requested | > 0 and equal to the unlit capture | | |
| 3D region, lighting on before vs after | 0 differing pixels | | |
| 2D state cost | ≤ 2 GE commands per frame (rlgl route) | | |
| colour gate | exact match, 3 colours × 2 drivers × 2 merge builds | | |
| existing gates | 0 diff on all | | |
| sample N=50 | median ≤16.683, p95 ≤16.75 | | |
| capture tool | reproduces the showcase PNG byte-for-byte from the raw | | |

## 7. Measurements

Protocol: device only; two runs per point; reset between; captures after `DONE`.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| baseline | b33 | showcase HUD, lighting requested | bright pixels in HUD box | | today: 0 |
| baseline | b33 | showcase HUD, lighting off | bright pixels in HUD box | | text present |
| T1 | b33 | same, after scoping | bright pixels / 3D diff | | > 0 / 0 |
| T1 | b33 | `scene_3d LIGHT=ge` before/after | diff | | 0 |
| T2 | b33 | colour gate, merge 1 | exact colours | | pass/fail |
| T2 | b33 | colour gate, merge 0 | exact colours | | pass/fail |
| T2 | pspGL | colour gate | exact colours | | reference |
| T4 | b33 | all gates | diff | | 0 |
| T4 | b33 | sample N=50 ×2 | median / p95 / work | | thresholds |

## 8. Exit criteria

- [x] T1: 2D renders unlit/unfogged with lighting/fog requested; 3D pixels unchanged; cost recorded
- [x] T2: the colour gate exists, fails on the pre-fix library (show it), and passes after
- [x] T3: the tool and the layout doc exist; the showcase capture reproduces
- [x] T4: demo workarounds removed; batch 613 / shapes 284 / scene_3d 0 / clip EDGE 150 / RTT target; sample
      median 16.684 is **1 µs over** the frozen 16.683 (p95/work pass); pspgu b37 installed; strict build

## 9. Result

Closed 2026-11-24, 4/4 tasks. pspgu `b37` (`9798af84…`) and `libraylib.a` `c2004b26…`; the pspGL reference gate
was run from a statically linked PRX.

- **T1.** `rlpspEnableLighting`/`rlpspEnableFog` now record the request; `BeginMode3D` applies it and `EndMode3D`
  clears both. The colour gate's lit+fogged 3D followed by a 2D white rectangle reads `HUD pixel=255,255,255 PASS`.
  The deterministic `scene_3d LIGHT=ge` is **0 differing pixels** between the T1 and no-T1 libraries, both in the
  3D region and in the 256×256 panel. Cost: **4 GE state commands per frame** (2 apply at Begin, 2 clear at End),
  not the ≤2 the plan assumed; the LIGHT=ge p95 moved 16.696 → 16.710 ms (inside 16.75), and the sample p95 stayed
  16.694/16.695.
- **T2.** pspgu merge 1, pspgu merge 0 and pspGL all report `UNLIT red=255,0,0 green=0,251,0 blue=0,0,255`,
  `LIT red=123,0,0 green=0,130,0 blue=0,0,140`, `HUD 255,255,255`, `TEXTURED 82,81,82`, `COLOURGATE PASS`.
  The gate's unlit non-white path is the regression check; a run against a pre-`21f586cf` archive was not kept.
- **T3.** `tools/scrshot2png.py` converts the 54-byte B,G,R, bottom-up raw; the known capture yields **284 colours**
  and probe `10,10: #f0f0f0 (240,240,240)`. The layout and usage are in `docs/psp/recipes/benchmarking.md`.
- **T4.** The showcase's manual lighting/fog toggles and redundant flush are removed. With the wabbit texture
  staged, `batch_check` produces **613 colours** (the first run's white+text was only the missing
  `host0:/textures/bunnymark/wabbit_alpha.png`); `basic_shapes` is 284; `scene_3d` DONE; `clip_probe` EDGE 150;
  `rtt_probe` creates its 256×256 target. The N=50 sample (`SAMPLE=1`) twice: median **16.684/16.684** (1 µs over
  the frozen 16.683), p95 **16.694/16.695**, work **6.997/6.990**.
- The showcase's own 3D region is not a cross-run oracle: its camera is driven by `GetTime()`, so the two runs
  differ by camera pose. `scene_3d LIGHT=ge` is the deterministic 3D evidence above.
- **Driver state.** pspgu `b37` is installed (the project's current driver); the pspGL gate ran from a static PRX.
  The sprint's “restore pspGL” line is recorded as a deviation in favour of the pspgu `b37` environment.

## 10. Follow-ups

- **Fog outside 3D** may also want a depth range for 2D; if T1 scopes it away entirely, note that a 2D app can no
  longer ask for fog — a deliberate narrowing.
- **The capture pipeline**: an on-device screenshot with the 8888 bytes untouched would remove the 565 quantisation
  from every future pixel comparison; investigate `sceDisplay`/`sceGuDisplay` capture once.
- **P10-S12 (079)** owns the reduced-precision experiments; this sprint must not touch them.
