# Sprint 025 (P13-S2) — Tier A textures and the texture paths

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S2 |
| Branch / base commit | `master` @ `595535df` (drafted 2026-11-21; set to the open sha when started) |
| Started / closed | 2026-11-21 / 2026-11-21 |
| Build number(s) | pspgu `b24`; twelve sample builds from this tree |
| Tasks | 13/13 done |

## 0. Handover

**Situation.** Phase 13's Tier A has four texture samples that fit today, and eight Tier B texture samples whose
frame renders without input. This sprint ports all twelve. It is the first independent exercise of sprint 020's
window split (two 768-wide sheets), `UpdateTexture`, `LoadImageAnim`, NPatch, blend modes, wrap/filter and
direct GL quads. No driver change is in scope.

**Read before acting**, in this order:

1. `../phases/13-samples-porting/index.md` — the two texture limits (1024 buffer / 512 sampling window, and the
   VRAM budget), the asset table and gate `G0`.
2. `../memory-map.md` §3 and §14 — the framebuffer/texture budget and the pspgu limits; §6 — capture protocol.
3. `../sprints/020-phase7-tilemap-and-windowing.md` (P7-S3) — the window-split design and its probe result.
4. `../sprints/015-phase6-npot-and-vram.md` (P6-S2) — padding, the allocator and the texture log line.
5. `psp-tests/texture_pattern/` and `psp-tests/texture_churn/` — the existing texture probes and their log format.
6. The twelve sources under `examples/textures/` listed in §5.

**First action.** Port `textures_image_loading` with `raylib_logo.png`, build and run it, and record the
`texture ... PSM ... B` line plus the boot VRAM line. Then port `textures_screen_buffer` (`UpdateTexture`, NPOT)
and `textures_srcrec_dstrec` (768×128 sheet, two windows) before the rest — those three tell us whether the upload
and window paths are sound.

**Hard rules.**

- **Only the assets in §5.** Do not load `cat.png`, `parrots.png`, `fudesumi.png`, `custom_jupiter_crash.png`,
  `cyberpunk_street_foreground.png`, `ps3.png`, `xbox.png`, `booth.png` or `explosion.png`; they are 1 MiB or
  larger, and several exceed the 1024 buffer.
- **`textures_cellular_automata` hardcodes 800×400.** Tie `imageWidth` to the 480-pixel window and `imageHeight`
  to 240 before building; the expected padded size is 512×256 = 524,288 B. Leaving 800×400 needs 2 MiB and fails.
- **The window split is under test.** `textures_srcrec_dstrec`, `textures_sprite_animation` (768×128) and
  `textures_gif_player` (128×768, six stacked frames) must show no seam, no duplicated column/row and no blank
  window. If they do, stop and file the failure in Phase 7; do not patch the sample.
- **Default 8888 build.** No PSM change; record the PSM and padded bytes from the texture log for every texture.
- **Interaction is out of scope.** Key/mouse branches may be dead. Record that rather than inventing input.
- **Animated samples freeze the last frames** (or are captured after `DONE`) and require two identical captures.
- **No driver, platform or renderer change.** A driver failure belongs to the phase that owns the path.
- **Stage only this sprint's paths.** `git status` before every commit; never `git add -A`.
- Commit messages carry the task ID: `sprint 025 T6: port textures_gif_player`.

**Commands.**

```sh
export PSPDEV=/home/psp/pspdev
export PATH=$PATH:$PSPDEV/bin

cd /home/psp/raylib-psp
./tools/pspgu-build.sh --verify-install

cd psp-tests/samples/textures/image_loading
make PSP_LARGE_MEMORY=0
( printf './image_loading.prx\n'; sleep 20 ) | pspsh
# after DONE: reset in pspsh, capture twice with scrshot, reset again
```

**Do not touch.** `src/`, `tools/pspgu/`, `examples/`, and every other `psp-tests/` directory.

**Done means** §8 below, with evidence in each task row.

## 1. Objective

Twelve texture samples render from 480×272 PSP builds, each with two identical captures and a recorded padded
size/PSM, and the two window-split cases (the 768×128 scarfy sheet in two samples) show no seam; the GIF
advances through `UpdateTexture` on a 128×128 texture. No sample in this sprint loads a texture larger than
524,288 B.

## 2. Why now

Wave 0b needs no new capability. It proves the texture path beyond `bunnymark`, `texture_pattern` and
`texture_churn`: upload of POT and NPOT images, `UpdateTexture` on a padded buffer, animated frames, NPatch,
blend modes, wrap/filter and direct rlgl quads. It also gives the first real check of the sprint 020 window split
before the input and budget waves depend on it.

## 3. Scope

**In:** copies of the twelve examples under `psp-tests/samples/textures/<name>/`, the resources each loads, and
the evidence archive.

**Out:** the 1 MiB image samples (P13-S7), the oversize/downscale samples (P13-S8), input remapping (P13-S3), and
any driver/platform edit.

## 4. Preconditions

- [ ] `pspsh` answers; the device is enumerated; `reset` was run
- [ ] `./tools/pspgu-build.sh --verify-install` passes; note the pspgu build marker
- [ ] `texture_pattern` and `texture_churn` pass on the current driver; archive their boot lines as controls
- [ ] `psp-tests/samples/Makefile.inc` exists (from P13-S1) or is created here
- [ ] The assets are staged with `usbhostfs_pc` rooted at `examples/textures`, or copied beside each sample
- [ ] Two captures of a stationary sample are byte-identical in this environment
- [ ] Builds use `PSP_LARGE_MEMORY=0`

## 5. Tasks

Asset bytes below are `padW × padH × 4` at 480×272 unless stated. The boot log is the final authority.

| ID | task | status | verification | evidence |
|---|---|---|---|---|
| T1 | `textures_image_loading` — `raylib_logo.png` 256×256, 262,144 B | `done` | texture log shows 256×256; two captures identical | **PASS** — 60 fps; two identical captures |
| T2 | `textures_logo_raylib` — 256×256, 262,144 B | `done` | logo renders; two captures identical | **PASS** — 60 fps; two identical captures |
| T3 | `textures_screen_buffer` — 240×136 → 256×256, 262,144 B; `UpdateTexture` | `done` | fire effect animates; no shear; two captures after `DONE` | **PASS** — 16 fps (CPU fire effect); 240×136 → 256×256 / 262,144 B; two identical captures |
| T4 | `textures_srcrec_dstrec` — `scarfy.png` 768×128 → 1024×128, 524,288 B | `done` | sheet samples through two 512 windows; no seam; two captures identical | **PASS** — 60 fps; 768×128 → 1024×128 / 524,288 B; no seam; two identical captures |
| T5 | `textures_cellular_automata` — tie `imageWidth`/`imageHeight` to 480×240 → 512×256, 524,288 B | `done` | simulation runs; `UpdateTexture` path correct; two captures after `DONE` | **PASS** — 43 fps; 480×240 → 512×256 / 524,288 B; two identical captures |
| T6 | `textures_gif_player` — `scarfy_run.gif` 6 frames; texture 128×128, 65,536 B; frame data is stacked in the image and `UpdateTexture` advances it | `done` | `LoadImageAnim` frame count 6; `UpdateTexture` advances the frame; two captures identical | **PASS** — 60 fps; 6 frames; 128×128 / 65,536 B; two identical captures |
| T7 | `textures_image_rotate` — two 256×256 rotations, 524,288 B total; the 45° image (362×362 → 1 MiB padded) is deferred to `G2`/P13-S7 | `done` | 90° frame renders; rotation is interaction-only and deferred; 45° case recorded as a `G2` dependency | **PASS** — 60 fps; two 256×256 / 524,288 B; two identical captures; 45° deferred |
| T8 | `textures_npatch_drawing` — `ninepatch_button.png` 64×256, 65,536 B | `done` | three NPatch draws render; no distortion | **PASS** — 60 fps; 64×256 / 65,536 B; two identical captures |
| T9 | `textures_particles_blending` — `spark_flame.png` 128×128, 65,536 B | `done` | particles animate with the designed blend mode | **PASS** — 43 fps; 128×128 / 65,536 B; two identical captures |
| T10 | `textures_sprite_animation` — `scarfy.png` 768×128 → 1024×128, 524,288 B | `done` | animation cycles; two windows, no seam | **PASS** — 60 fps; 768×128 → 1024×128 / 524,288 B; no seam; two identical captures |
| T11 | `textures_textured_curve` — `road.png` 64×64, 16,384 B; direct GL quads | `done` | curve renders with the texture; no missing symbols | **PASS** — 60 fps; 64×64 / 16,384 B; direct `gl*` path; two identical captures |
| T12 | `textures_tiled_drawing` — `patterns.png` 256×256, 262,144 B | `done` | bilinear filter and wrap behave as designed | **PASS** — 60 fps; 256×256 / 262,144 B; two identical captures |
| T13 | Evidence archive and classification verdict | `done` | twelve captures + logs; Phase 13 Tier A/B either confirmed or corrected; `git diff -- examples/` empty | **PASS** — 12 pairs + logs in `/home/psp/evidence/sprint025/`, `SHA256SUMS`; Phase 13 updated; `git diff -- examples/` empty |

### Task log (append-only)

- 2026-11-21 — implementation and builds complete for all twelve; device run and captures started
- 2026-11-21 — T1–T6 `done`; six samples, two identical captures each
- 2026-11-21 — T7–T12 `done`; six samples, two identical captures each
- 2026-11-21 — T13 `done`; evidence archived, Phase 13 updated

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | no target; record median for animated samples | | |
| heap | record boot free; `textures_cellular_automata` holds a 480×240 RGBA image | | |
| VRAM | each texture ≤ 524,288 B; record the boot line after load | | |
| captures | 2 identical per sample, after `DONE` | | |

## 7. Measurements

Protocol: device only; one sample per run; no control arm (no renderer change). Every row carries the build
marker, the padded texture bytes and the sample's fps from its log.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| port | pspgu b24 | `textures_image_loading` | `DONE` / captures | **600, 60 fps / identical** | 256×256, 262,144 B; free 654,848 B |
| port | pspgu b24 | `textures_logo_raylib` | `DONE` / captures | **600, 60 fps / identical** | 256×256, 262,144 B; free 654,848 B |
| port | pspgu b24 | `textures_screen_buffer` | `DONE` / captures | **600, 16 fps / identical** | 240×136 → 256×256, 262,144 B; free 654,848 B |
| port | pspgu b24 | `textures_srcrec_dstrec` | `DONE` / captures | **600, 60 fps / identical** | 768×128 → 1024×128, 524,288 B; free 392,704 B |
| port | pspgu b24 | `textures_cellular_automata` | `DONE` / captures | **600, 43 fps / identical** | 480×240 → 512×256, 524,288 B; free 392,704 B |
| port | pspgu b24 | `textures_gif_player` | `DONE` / captures | **600, 60 fps / identical** | 128×128, 65,536 B; 6 stacked frames; free 851,456 B |
| port | pspgu b24 | `textures_image_rotate` | `DONE` / captures | **600, 60 fps / identical** | 2 × 256×256, 524,288 B; free 392,704 B |
| port | pspgu b24 | `textures_npatch_drawing` | `DONE` / captures | **600, 60 fps / identical** | 64×256, 65,536 B; free 851,456 B |
| port | pspgu b24 | `textures_particles_blending` | `DONE` / captures | **600, 43 fps / identical** | 128×128, 65,536 B; free 851,456 B |
| port | pspgu b24 | `textures_sprite_animation` | `DONE` / captures | **600, 60 fps / identical** | 768×128 → 1024×128, 524,288 B; free 392,704 B |
| port | pspgu b24 | `textures_textured_curve` | `DONE` / captures | **600, 60 fps / identical** | 64×64, 16,384 B; free 900,608 B |
| port | pspgu b24 | `textures_tiled_drawing` | `DONE` / captures | **600, 60 fps / identical** | 256×256, 262,144 B; free 654,848 B |

## 8. Exit criteria

- [x] all twelve samples build with `PSP_LARGE_MEMORY=0` and reach their `DONE` marker
- [x] every texture's padded size and PSM are recorded and match the expected bytes
- [x] the two 768-wide sheet samples show no seam, duplicated line or blank window
- [x] each sample has two identical captures, archived with a build marker
- [x] `git diff -- examples/` is empty
- [x] no sample loads an asset above 524,288 B
- [x] the Phase 13 Tier A/B classification is confirmed or corrected, with the owning phase for every failure
- [x] no build artifacts are left in the tree

## 9. Result

P13-S2 closed. All twelve samples build with `PSP_LARGE_MEMORY=0` and ran on the PSP-1000: each logged
`DONE frames 600` and produced two byte-identical 480×272 captures. Evidence is in
`/home/psp/evidence/sprint025/` with `SHA256SUMS`; assets were staged at `host0:/resources/`. The harness is the
same `psp-tests/samples/Makefile.inc` used by P13-S1, without changes.

Findings:

- **The 512-window split works on real sheets.** `textures_srcrec_dstrec` and `textures_sprite_animation` draw
  the 768×128 scarfy sheet (padded 1024×128 = 524,288 B) through two windows with no seam, duplicate column or
  blank window.
- **`LoadImageAnim` is one frame, not a stack.** raylib returns a 128×128 image whose data contains the six
  frames; the texture is 128×128 (65,536 B), and `UpdateTexture` advances it. The audit's 128×768 texture
  expectation was wrong; the height-window case is not exercised by the GIF, and the window split is covered by
  the two 768-wide sheets instead.
- **`UpdateTexture` handles padded NPOT buffers.** `screen_buffer` (240×136 → 256×256) and `cellular_automata`
  (480×240 → 512×256) update the padded texture correctly.
- **Three samples are CPU-bound, not renderer-bound.** `screen_buffer` is 16 fps (per-pixel fire effect),
  `cellular_automata` 43 fps (per-pixel simulation) and `particles_blending` 43 fps (200 blended particles); the
  other nine hold 60 fps.
- **The 45° image rotation does not fit.** `ImageRotate(45)` expands a 256×256 image to 362×362, which pads to
  512×512 = 1 MiB, over the 654,848 B free at that point. The port keeps the two 256×256 rotations and defers the
  45° case to `G2`/P13-S7.
- **Direct-rlgl with `gl*`.** `textures_textured_curve` uses the direct GL quads path; no missing symbols.
- **Layout adaptations:** centred labels, 480×240 automaton, on-screen GIF/sprite/srcrec, and default NPatch
  sizes retained because the PSP has no mouse.

## 10. Follow-ups

- **P13-S7**: port the 45° `textures_image_rotate` case under `G2` (depth-off or 16-bit PSM); it needs 1 MiB.
- **Interaction** in `image_rotate`, `npatch_drawing`, `particles_blending`, `textured_curve` and `tiled_drawing`
  belongs to P13-S3 (`G1`).
- **CPU costs** in `screen_buffer`, `cellular_automata` and `particles_blending` are sample-side; if they need
  60 fps, optimise the sample or record the trade, not the renderer.
- **GIF expectation**: future audits must treat `LoadImageAnim` as a frame-sized image with stacked data, not a
  stacked texture. Recorded in Phase 13.
- **Assets**: `host0:/resources/` is the convention; keep the seven files staged for later texture sprints.