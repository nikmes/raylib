# Sprint 025 (P13-S2) — Tier A textures and the texture paths

| | |
|---|---|
| Roadmap phase | [Phase 13 — Samples porting](../phases/13-samples-porting/index.md) |
| Sprint | P13-S2 |
| Branch / base commit | `master` @ `595535df` (drafted 2026-11-21; set to the open sha when started) |
| Started / closed | — / — |
| Build number(s) | pspgu build at open; no driver change expected |
| Tasks | 0/13 done |

## 0. Handover

**Situation.** Phase 13's Tier A has four texture samples that fit today, and eight Tier B texture samples whose
frame renders without input. This sprint ports all twelve. It is the first independent exercise of sprint 020's
window split (a 768-wide sheet and a 768-tall stacked GIF), of `UpdateTexture`, `LoadImageAnim`, NPatch, blend
modes, wrap/filter and direct rlgl quads. No driver change is in scope.

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
size/PSM, and the three window-split cases (768×128 sheet, 768×128 second sheet, 128×768 stacked GIF) show no
seam. No sample in this sprint loads a texture larger than 524,288 B.

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
| T1 | `textures_image_loading` — `raylib_logo.png` 256×256, 262,144 B | `todo` | texture log shows 256×256; two captures identical | — |
| T2 | `textures_logo_raylib` — 256×256, 262,144 B | `todo` | logo renders; two captures identical | — |
| T3 | `textures_screen_buffer` — 240×136 → 256×256, 262,144 B; `UpdateTexture` | `todo` | fire effect animates; no shear; two captures after `DONE` | — |
| T4 | `textures_srcrec_dstrec` — `scarfy.png` 768×128 → 1024×128, 524,288 B | `todo` | sheet samples through two 512 windows; no seam; two captures identical | — |
| T5 | `textures_cellular_automata` — tie `imageWidth`/`imageHeight` to 480×240 → 512×256, 524,288 B | `todo` | simulation runs; `UpdateTexture` path correct; two captures after `DONE` | — |
| T6 | `textures_gif_player` — `scarfy_run.gif` 6 frames → 128×768 → 128×1024, 524,288 B | `todo` | `LoadImageAnim` frame count 6; animation advances; two height windows, no seam | — |
| T7 | `textures_image_rotate` — 256×256, 262,144 B | `todo` | unrotated frame renders; rotation is interaction-only and deferred | — |
| T8 | `textures_npatch_drawing` — `ninepatch_button.png` 64×256, 65,536 B | `todo` | three NPatch draws render; no distortion | — |
| T9 | `textures_particles_blending` — `spark_flame.png` 128×128, 65,536 B | `todo` | particles animate with the designed blend mode | — |
| T10 | `textures_sprite_animation` — `scarfy.png` 768×128 → 1024×128, 524,288 B | `todo` | animation cycles; two windows, no seam | — |
| T11 | `textures_textured_curve` — `road.png` 64×64, 16,384 B; rlgl quads | `todo` | curve renders with the texture; no missing symbols | — |
| T12 | `textures_tiled_drawing` — `patterns.png` 256×256, 262,144 B | `todo` | bilinear filter and wrap behave as designed | — |
| T13 | Evidence archive and classification verdict | `todo` | twelve captures + logs; Phase 13 Tier A/B either confirmed or corrected; `git diff -- examples/` empty | — |

### Task log (append-only)

- YYYY-MM-DD — sprint opened, all tasks `todo`

## 6. Budgets

| budget | limit | measured | tag |
|---|---|---|---|
| frame time | no target; record median for animated samples | | |
| heap | record boot free; `textures_cellular_automata` holds a 480×240 RGBA image | | |
| VRAM | each texture ≤ 524,288 B; record the boot line after load | | |
| captures | 2 identical per sample, after `DONE` | | |

## 7. Measurements

Protocol: device only; one sample per run; `texture_pattern` before the sweep and again at the end. Every row
carries the build marker, the padded size and the PSM from the texture log.

| run | build | workload | metric | value | notes |
|---|---|---|---|---|---|
| control | | `texture_pattern` | pixel gate / boot VRAM | | |
| port | | `textures_image_loading` | 256×256 / 262,144 B / `DONE` | | |
| port | | `textures_logo_raylib` | 256×256 / 262,144 B / `DONE` | | |
| port | | `textures_screen_buffer` | 256×256 / 262,144 B / `DONE` | | |
| port | | `textures_srcrec_dstrec` | 1024×128 / 524,288 B / windows | | |
| port | | `textures_cellular_automata` | 512×256 / 524,288 B / `DONE` | | |
| port | | `textures_gif_player` | 128×1024 / 524,288 B / 6 frames | | |
| port | | `textures_image_rotate` | 256×256 / 262,144 B / `DONE` | | |
| port | | `textures_npatch_drawing` | 64×256 / 65,536 B / `DONE` | | |
| port | | `textures_particles_blending` | 128×128 / 65,536 B / `DONE` | | |
| port | | `textures_sprite_animation` | 1024×128 / 524,288 B / windows | | |
| port | | `textures_textured_curve` | 64×64 / 16,384 B / `DONE` | | |
| port | | `textures_tiled_drawing` | 256×256 / 262,144 B / `DONE` | | |
| control | | `texture_pattern` | pixel gate / boot VRAM | | |

## 8. Exit criteria

- [ ] all twelve samples build with `PSP_LARGE_MEMORY=0` and reach their `DONE` marker
- [ ] every texture's padded size and PSM are recorded and match the expected bytes within the padding rule
- [ ] the three window-split cases show no seam, duplicated line or blank window
- [ ] each sample has two identical captures, archived with a build marker
- [ ] `git diff -- examples/` is empty
- [ ] no sample loads an asset above 524,288 B
- [ ] the Phase 13 Tier A/B classification is confirmed or corrected, with the owning phase for every failure
- [ ] no build artifacts are left in the tree

## 9. Result

—

## 10. Follow-ups

- Interaction in `image_rotate`, `npatch`, `particles`, `textured_curve` and `tiled_drawing` belongs to P13-S3.
- A window seam is a Phase 7 bug: quote the window and the sampled edge, and do not work around it in the sample.
- The 1 MiB image samples are P13-S7; the oversize/downscale cases are P13-S8.
