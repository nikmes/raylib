# Phase 13 — Samples porting

| | |
|---|---|
| State | **in progress** — P13-S1/S2 done (21 samples, two identical captures each); Tier A complete; P13-S7 written; P13-S3–S6/S8–S11 placeholders |
| Depends on | nothing for wave 0; later waves consume Phase 6 (texture budget), Phase 7 (oversize assets), Phase 8 (input), Phase 9 (audio), Phase 10 (render targets) |
| Exit criterion | every sample in `core`, `shapes` and `textures` is either ported with a build marker and two identical captures, or marked `dropped` with a reason; the tier table matches the device runs |

## What this phase is about

The upstream example corpus is the only broad regression suite raylib has. Our current corpus — `basic_shapes`,
`collision_area`, `logo_raylib_anim`, `bunnymark`, `cubicmap` plus the purpose-built probes — covers the paths
that mattered for phases 1–5, not the API surface a game will use. A port is also a capability test: a sample
that renders proves a path works, and a sample that does not names the missing capability precisely.

This phase is **continuous and capability-gated**, not a chronological successor to Phase 12. Its waves consume
the outputs of the renderer, input and audio phases. It never blocks them, and no other phase should wait for it.
It is placed after Phase 12 only because it spans every phase before it.

Scope: `examples/core` (49 samples), `examples/shapes` (45), `examples/textures` (33) — **127 samples**. Models,
shaders, audio and the other categories are out of scope for now; `models/cubicmap` is already ported as part of
the bring-up corpus.

## What has to be true before starting

- [x] A working platform layer and a device run loop (`DONE` + `sceKernelSleepThread`, never `sceKernelExitGame`)
- [x] A capture protocol that produces trustworthy screenshots ([memory map §6](../../memory-map.md#6-measurement-protocol))
- [x] The PSP backend's actual capability list, read from `tools/pspgu/` and `src/platforms/rcore_psp.c`
- [x] 512-pixel window sampling with a buffer up to 1024 — sprint 020
- [ ] A reusable sample harness: one copy of an example plus a shared Makefile include
- [ ] A decision on asset staging (`host0:/resources/...` with `usbhostfs` rooted at the category directory, or
      resources copied beside the sample)
- [ ] The Phase 8 decision on whether the pad is also mapped to `CORE.Input.Keyboard` and the nub to the mouse

## Findings — the 2026-11-21 audit

Method: every `.c` file under `examples/{core,shapes,textures}` was scanned for raylib calls, then checked against
what the driver actually implements. Counts are `[derived: source scan, 2026-11-21]`, not `[measured: device]`.
No sample was built or run for this audit; wave 0 exists to validate the classification.

**Already ported (4):** `shapes_basic_shapes`, `shapes_collision_area`, `shapes_logo_raylib_anim`,
`textures_bunnymark`. Plus `models/cubicmap`, outside this scope.

**Ported since the audit (1):** `core_input_gamepad` at 480×272 in
`psp-tests/samples/core/input_gamepad/`, with a primitive layout. Device-validated on the PSP-1000: all 12
button enums, 1800 frames, 60 fps, log and capture in `/home/psp/evidence/sprint041/`. It moves from Tier B to
ported (Tier B 60, ported 5) without changing the audit's total.

| tier | core | shapes | textures | total |
|---|---|---|---|---|
| ported | 0 | 3 | 1 | **4** |
| A — drop-in | 4 | 5 | 4 | **13** |
| B — renders, input needs remap | 20 | 33 | 8 | **61** |
| C — blocked or not meaningful | 25 | 4 | 20 | **49** |
| **total** | **49** | **45** | **33** | **127** |

### What the driver supports today

Read from the code, with the path that decides it:

- **Immediate-mode GL11 drawing, blending, scissor, depth, culling.** `GL_BLEND`, `GL_DEPTH_TEST`, `GL_CULL_FACE`
  and `GL_SCISSOR_TEST` all map to GU state (`tools/pspgu/gl_state.c:75-95`). `core_3d_camera_mode` is the
  cheapest 3D probe that follows from this.
- **No render textures.** Under GL11, `rlLoadFramebuffer()` returns 0 (`src/rlgl.h:4162`); the attachment calls
  are compiled out. `BeginTextureMode` draws to the screen and the target texture stays blank.
- **No shaders.** `libGL.a` exports no shader symbols and rlgl has no GL11 shader path.
- **Gamepad and nub only.** `PollInputEvents()` fills `CORE.Input.Gamepad`; `CORE.Input.Keyboard`,
  `CORE.Input.Mouse` and `CORE.Input.Touch` are never populated (`src/platforms/rcore_psp.c:404`).
- **Screen readback works, texture readback does not.** `glReadPixels` expands the 5650 framebuffer
  (`tools/pspgu/gl_query.c:72`). `LoadImageFromTexture` is deliberately blank on PSP (`src/rlgl.h:4053`).
- **GIF decodes.** `SUPPORT_FILEFORMAT_GIF` is 1 (`src/config.h:242`), so `LoadImageAnim` works.
- **Audio is not implemented yet.** `raudio.c` has no PSP backend; samples that need sound wait for Phase 9.

### The texture limits are two numbers, not one

The GE samples through **512-pixel windows**. Sprint 020 implemented the split: a texture's buffer dimension may
be up to **1024** (`tbw` up to 1024, `padW`/`padH` up to 1024), the upload is chunked into 512-column bands, and
the draw path emits one ≤512 window per piece. Anything whose padded size exceeds 1024 in either dimension is
rejected at upload (`tools/pspgu/gl_texture.c:611-615`). So the old “wider than 512 is silently wrong” finding is
closed; the remaining limit is **1024 buffer / 512 sampling window**.

The second limit is VRAM. Sprint 015 pads every NPOT texture to powers of two in both dimensions. The boot line
reports **982,528 B free** before any sample asset `[measured: sprint 015 T3]`;
[§3](../../memory-map.md#3-hardware-facts-that-bound-the-design) reports **913,344 B** after the corpus' textures. A
512×512 RGBA texture is **1,048,576 B**, so it cannot allocate against either figure — and the failure is a
logged allocation failure (`tools/pspgu/gl_texture.c:429`), not a crash.

| asset | logical | padded (8888) | bytes | verdict |
|---|---|---|---|---|
| `raylib_logo.png` | 256×256 | 256×256 | 262,144 | fits |
| `patterns.png` | 256×256 | 256×256 | 262,144 | fits |
| screen buffer at 480×272 (`SCALE_FACTOR=2`) | 240×136 | 256×256 | 262,144 | fits |
| `scarfy.png` (sprite sheet / srcrec) | 768×128 | 1024×128 | 524,288 | **fits** — two 512 windows |
| `scarfy_run.gif` (6 frames) | 128×128 texture; frame data stacked | 128×128 | 65,536 | fits; `UpdateTexture` per frame |
| `cat.png` | 384×512 | 512×512 | 1,048,576 | **over budget** |
| `parrots.png` | 512×384 | 512×512 | 1,048,576 | **over budget** |
| `fudesumi.png` | 384×512 | 512×512 | 1,048,576 | **over budget** |
| `custom_jupiter_crash.png` | 426×284 | 512×512 | 1,048,576 | **over budget** |
| `cyberpunk_street_background.png` | 512×192 | 512×256 | 524,288 | fits |
| `cyberpunk_street_foreground.png` | 704×192 | 1024×256 | 1,048,576 | **over budget** |
| `button.png` | 224×318 | 256×512 | 524,288 | fits |
| `ps3.png` / `xbox.png` | 800×450 | 1024×512 | 2,097,152 | **over budget** |
| `booth.png` | 112×11468 | 128×16384 | 8,388,608 | **rejected — height > 1024** |
| `explosion.png` | 1024×1024 | 1024×1024 | 4,194,304 | **over budget** |

`[derived: padW × padH × 4, sprint 015 padding and sprint 020 windowing]`

`textures_cellular_automata` is a special case: it hardcodes an **800×400** image, which pads to 1024×512 =
2,097,152 B and fails. Tied to the 480×272 window (480×240) it pads to 512×256 = 524,288 B and fits. That is a
port-time size change, not a capability.

Two build levers move the budget without touching a sample:

- `PSP_GU_DEPTH=0` returns **278,528 B** (272 KiB): the fresh free figure rises to about **1.26 MiB**, and §3's
  after-corpus figure to **1,191,872 B** — enough for one 1 MiB texture, not for a sample that holds three.
- `PSP_GU_TEXFMT=auto` (sprint 015 T2) halves an opaque `GL_RGB` texture to 5650, so `cat.png` and `parrots.png`
  become 524,288 B. RGBA sources without binary alpha stay 8888.

`textures_image_generation` is blocked on both counts: it allocates nine full-screen CPU images and nine textures.
At 480×272 that is ~4.7 MB of CPU heap and ~9 MiB of padded VRAM. It is only viable as a one-at-a-time variant
with an explicit `PSP_HEAP_SIZE_KB`.

## Tier definitions

- **A — drop-in.** The sample's intended frame needs no input and no missing backend feature; its assets fit.
  Only the standard harness changes are required.
- **B — renders, input needs a remap.** The frame draws today, but the demo's mouse, keyboard or touch interaction
  does nothing. One platform-level remap can unlock most of these without editing the samples.
- **C — blocked or not meaningful.** Either a backend feature is missing (render targets, audio, clipboard,
  texture readback), the asset cannot fit or exceed the 1024 buffer, the input hardware does not exist on a PSP,
  or the sample demonstrates a desktop-only concept.

## Tier A — port now (13)

Standard harness only: 480×272, `PSP_MODULE_INFO`, a bounded loop ending in `DONE` + `sceKernelSleepThread()`,
assets staged under `host0:`.

- [x] `core_basic_window` — window and text smoke test
- [x] `core_custom_logging` — logging path
- [x] `core_random_values` — CPU only
- [x] `core_3d_camera_mode` — static cube and grid; first real 3D probe
- [x] `shapes_logo_raylib` — shapes and text
- [x] `shapes_math_angle_rotation` — trig and rotation
- [x] `shapes_outlines_thickness` — outline paths
- [x] `shapes_circle_sector_drawing` — arcs and sectors
- [x] `shapes_rectangle_advanced` — direct `rlBegin`/`rlVertex` path
- [x] `textures_image_loading` — load image, upload a 256×256 texture
- [x] `textures_logo_raylib` — texture draw
- [x] `textures_screen_buffer` — procedural buffer and `UpdateTexture`
- [x] `textures_srcrec_dstrec` — 768×128 sheet through two windows; no input

## Tier B — renders now, interaction needs a remap (61)

The frame is valid; only the interaction is dead. `G1` is the Phase 8 mapping of the nub to
`CORE.Input.Mouse` and the pad to `CORE.Input.Keyboard`, or per-sample pad handling.

**core (20):** `core_2d_camera`, `core_2d_camera_mouse_zoom`, `core_2d_camera_platformer`,
`core_3d_camera_first_person`, `core_3d_camera_fps`, `core_3d_camera_free`, `core_3d_picking`,
`core_basic_screen_manager`, `core_compute_hash`, `core_custom_frame_control`, `core_delta_time`,
`core_directory_files`, `core_input_actions`, `core_input_gamepad`, `core_random_sequence`,
`core_scissor_test`, `core_screen_recording`, `core_storage_values`, `core_text_file_loading`,
`core_world_screen`.

`core_input_gamepad` is the natural input test. It is ported at 480×272 with a primitive layout;
`GetGamepadName()` is populated by P8-S1 and the 800×450 pad art is not used. `core_screen_recording` uses
`LoadImageFromScreen()` and writes a GIF; the readback exists, the output path needs a writable location.

**shapes (33):** `shapes_ball_physics`, `shapes_bouncing_ball`, `shapes_clock_of_clocks`,
`shapes_colors_palette`, `shapes_dashed_line`, `shapes_digital_clock`, `shapes_drag_puzzle`,
`shapes_easings_ball`, `shapes_easings_box`, `shapes_easings_rectangles`, `shapes_easings_testbed`,
`shapes_ellipse_collision`, `shapes_following_eyes`, `shapes_hilbert_curve`, `shapes_kaleidoscope`,
`shapes_lines_bezier`, `shapes_math_sine_cosine`, `shapes_mouse_trail`, `shapes_outlines_testbed`,
`shapes_penrose_tile`, `shapes_pie_chart`, `shapes_polygon_lines`, `shapes_rectangle_scaling`,
`shapes_recursive_tree`, `shapes_ring_drawing`, `shapes_rlgl_color_wheel`, `shapes_rlgl_triangle`,
`shapes_rounded_rectangle_drawing`, `shapes_simple_particles`, `shapes_splines_drawing`,
`shapes_starfield_effect`, `shapes_triangle_strip`, `shapes_vector_angle`.

**textures (8, assets fit):** `textures_cellular_automata` (tie its image to 480×240), `textures_gif_player`,
`textures_image_rotate`, `textures_npatch_drawing`, `textures_particles_blending`,
`textures_sprite_animation`, `textures_textured_curve`, `textures_tiled_drawing`.

These eight were ported in P13-S2; only their interaction waits for `G1`.

## Tier C — blocked or not meaningful (49)

**Render targets / FBO (16).** The target texture stays blank under GL11:
`core_2d_camera_split_screen`, `core_3d_camera_split_screen`, `core_render_texture`,
`core_smooth_pixelperfect`, `core_viewport_scaling`, `core_window_letterbox`, `core_vr_simulator`;
`shapes_bullet_hell`, `shapes_double_pendulum`, `shapes_lines_drawing`, `shapes_top_down_lights`;
`textures_fog_of_war`, `textures_framebuffer_rendering`, `textures_magnifying_glass`,
`textures_mouse_painting`, `textures_portal_window`. `core_vr_simulator` also needs shaders.

**Texture size / VRAM (11).**
- Padded to 1 MiB, over the 982,528 B budget; needs `G2`: `textures_image_kernel`,
  `textures_image_processing`, `textures_image_text`, `textures_polygon_drawing`,
  `textures_raw_data`, `textures_background_scrolling`, `textures_blend_modes`; plus the 45°
  `textures_image_rotate` case.
- Beyond the 1024 buffer, or needs a downscale/rewrite; `G3`: `textures_image_channel` (six textures),
  `textures_image_drawing` (non-binary font atlas), `textures_sprite_stacking` (11468-tall sheet),
  `textures_image_generation` (nine 1 MiB textures plus the heap).

**Audio, Phase 9 (2).** `textures_sprite_button`, `textures_sprite_explosion`.
`textures_sprite_explosion` also carries a 1024×1024 = 4 MiB asset and needs `G3`.

**Clipboard (2).** `core_clipboard_text`, `textures_clipboard_image`.

**Texture readback (1).** `textures_to_image` — `LoadImageFromTexture` returns a blank buffer.

**Input hardware, mostly (9).** Keyboard/mouse testbeds that `G1` can rescue: `core_input_keys`,
`core_input_mouse`, `core_input_mouse_wheel`, `core_keyboard_testbed`, `core_undo_redo`. Touch-only samples with
no PSP equivalent: `core_input_gestures`, `core_input_gestures_testbed`, `core_input_multitouch`,
`core_input_virtual_controls`.

**Desktop-only concept (8).** Drop unless a reason appears: `core_highdpi_demo`, `core_highdpi_testbed`,
`core_monitor_detector`, `core_window_flags`, `core_window_web`, `core_drop_files`,
`core_window_should_close`, `core_automation_events`. `core_window_should_close` can be adapted in one line if
the exit-key path is ever worth testing.

## Capability gates

| gate | capability | owner phase | unlocks |
|---|---|---|---|
| `G0` | harness, 480×272, asset staging | this phase | 13 Tier A + 8 small-asset texture samples |
| `G1` | nub→`Mouse`, pad→`Keyboard`, gamepad name | Phase 8 | 53 Tier B core/shapes, 5 input testbeds |
| `G2` | depth-off build and/or `PSP_GU_TEXFMT=auto`, larger heap | Phase 6, Phase 11 | 7 texture samples + 45° `image_rotate` |
| `G3` | downscale or slice assets beyond 1024 or over budget | Phase 7 (windows already done in sprint 020) | 4 texture samples + pad art + explosion |
| `G4` | render targets / FBO on the native backend | Phase 10 (with Phase 5) | 16 render-target samples |
| `G5` | `sceAudio` output | Phase 9 | 2 audio samples |
| `G6` | clipboard | Phase 8 / platform | 2 clipboard samples |
| `G7` | texture readback (`glGetTexImage` equivalent) | Phase 6 | 1 readback sample |

`G2` and `G3` are separate on purpose: a 1 MiB padded texture fits the 1024 buffer and still does not fit the
8888 VRAM budget, while a 11468-tall sheet fits neither.

## Wave plan

| wave | gate | samples | exit |
|---|---|---|---|
| W0 — Tier A | `G0` | 13 | all 13 render; captures archived; harness reusable by copy + Makefile include |
| W0b — small-asset textures | `G0` | 8 | eight texture paths render (`UpdateTexture`, GIF, NPatch, blend, wrap/filter, rlgl curve, sheet windows); interaction deferred |
| W1 — input remap | `G1` | 58 | mapping decision recorded; a representative set exercised end to end; remaining B samples swept |
| W2 — texture budget | `G2`/`G3` | 11 | each asset's padded size, PSM and build flags recorded; visual delta counted where an asset is downscaled |
| W3 — render targets | `G4` | 16 | render-to-texture path works; the 16 samples render |
| W4 — remaining features | `G5`–`G7` | 5 | audio, clipboard and readback samples either render or are dropped |
| dropped | — | 12 | each carries a one-line reason in this page; 4 touch-only, 8 desktop-only |

4 ported + 13 + 8 + 58 + 11 + 16 + 5 + 12 = 127.

## Deriving sprints from this page

- **One gate per sprint.** A sprint that mixes `G1` and `G2` cannot attribute a failure. A sprint belongs to
  exactly one phase ([§13](../../memory-map.md#13-working-method-sprints-features-recipes)); for a samples sprint
  that phase is this one, and the gate names the dependency it consumes.
- **Take a feature slice, not a random subset.** For example W0's core/shapes, then the texture paths, then the
  input slice, then the shapes sweep. A slice that shares a path produces one diagnosis when it fails.
- **Every sprint opens with a §0 handover** naming the gate, the sample list, the asset root, the exact `pspsh`
  command and the build marker.
- **Tasks carry `T` IDs and evidence**, as in every sprint. A sample is `done` only with a build marker, the boot
  log, `DONE frames N`, and two identical captures. If it has interaction, that interaction was exercised on the
  device.
- **Do not edit `examples/` in place.** Copy into `psp-tests/samples/<category>/<name>/` and keep the upstream
  file untouched, so the corpus stays updatable.
- **A driver bug found by a sample is a task in the owning phase**, not a workaround in the copied sample.

## Porting recipe

1. Copy `examples/<category>/<name>/<name>.c` and only the resources that sample uses into
   `psp-tests/samples/<category>/<name>/`.
2. Set the window to 480×272; keep the layout arithmetic if it still reads well. If a sample hardcodes an image
   size (cellular automata), tie it to the new window rather than leaving the 800-wide constant.
3. Add the PSP module header, `PSP_MODULE_INFO`, `PSP_MAIN_THREAD_ATTR`, and the `PSP_LARGE_MEMORY=0` build flag.
4. Replace `while (!WindowShouldClose())` with a bounded loop (600 frames by default; longer or input-scripted for
   interactive samples). Log `FRAME` every 60 and end with `DONE` + `sceKernelSleepThread()`.
5. Use the `psp-tests/basic_shapes/Makefile` link line as the template (`-lraylib -lpng -lz -lglut -lGLU -lGL
   -lpspgu -lpspvfpu`), and set `PSP_HEAP_SIZE_KB` explicitly when the sample holds full-screen images.
6. Stage assets with `usbhostfs_pc` rooted at `examples/<category>` and address them as `host0:/resources/...`,
   or copy them beside the sample.
7. Capture per [§6](../../memory-map.md#6-measurement-protocol): two identical captures, a visible build marker, and
   the frame log stopped before the capture.

## What we already know

- **A single remap unlocks most of Tier B.** The memory map's Phase 8 already asks whether to feed
  `CORE.Input.Keyboard` from the pad; the audit makes the payoff explicit: 58 samples.
- **The 512-pixel window split exists.** Sprint 020 raised the buffer to 1024 and implemented the per-draw window
  split. A 768-wide sheet (scarfy) now fits and samples correctly through two windows (P13-S2, no seam); >1024
  is rejected. A GIF is a frame-sized image with stacked data, not a stacked texture.
- **`LoadImageAnim` returns one frame.** The six GIF frames live in the image data; the texture is 128×128 and
  `UpdateTexture` advances it. The audit's 128×768 texture expectation was wrong. `[measured: P13-S2]`
- **Three texture samples are CPU-bound.** `screen_buffer` 16 fps, `cellular_automata` 43 fps and
  `particles_blending` 43 fps; the other nine hold 60 fps. These are sample costs, not renderer regressions.
  `[measured: PSP-1000, P13-S2]`
- **Direct-rlgl samples need the GL entry points, not the `rl*` wrappers.** The installed `libraylib.a` exports the
  high-level `rl*` API but not the GL11 vertex-level `rlBegin`/`rlVertex2f`/`rlColor4ub`. A sample that includes
  `rlgl.h` must build with `-DPLATFORM_PSP -DGRAPHICS_API_OPENGL_11` and call `glBegin`/`glVertex2f`/`glColor4ub`
  (as P13-S1 does) or link the rlgl implementation itself. `[measured: build, 2026-11-21]`
- **`scrshot` returns before hostfs has written the BMP.** A 0-byte, 128 KiB partial or black frame is a capture
  race. Wait for the full 391,734-byte file and require two identical captures; P13-S1 hit this three times before
  adding the wait. `[measured: P13-S1, 2026-11-21]`
- **`InitWindow` installs the platform `CustomLog`, replacing a pre-init `SetTraceLogCallback`.** A custom logger
  must be installed after `InitWindow` on PSP; `core_custom_logging` does that and flushes each line.
  `[measured: P13-S1, 2026-11-21]`
- **A sample can be code-correct and still fail on assets.** The image samples are the clearest case: their code
  path already works, but `cat.png` pads to 1 MiB and cannot allocate. Quote the boot line with every texture
  result.
- **FBO is the largest single blocker** after input: 16 samples, and `core_vr_simulator` needs shaders on top.
- **The classification is static.** No sample in this phase has been built or run. Wave 0 is also the validation
  of the tier definitions: if a Tier A sample fails, the failure belongs in the owning phase, not here.

## Risks and open questions

- Does `G1` remap the nub to a mouse, to gamepad axes, or both? Feeding both is ambiguous; the decision belongs in
  Phase 8 and must be recorded before W1 starts.
- Does redefining `IsKeyDown` on PSP (pad-as-keyboard) break the gamepad path or change documented semantics?
- Downscaling an asset changes the picture. Every downscale needs a before/after pixel count, as sprint 015 did
  for NPOT padding; do not hide it in the capture.
- The window split is new. `textures_srcrec_dstrec` and `textures_sprite_animation` are the first independent
  checks of a 768-wide sheet, and P13-S2 found no seam. If a seam or a wrong window appears later, the failure
  belongs to Phase 7, not to the sample.
- Direct-rlgl samples: the library does not export the GL11 vertex-level `rl*` wrappers; the P13-S1 port calls the
  `gl*` entry points they map to. Any future rlgl sample must do the same or the link fails.
- The corpus grows with raylib. New samples must be appended to the tables, never renumbered or silently dropped.
- `textures_image_generation` may be better dropped than adapted: it needs an explicit heap and a one-at-a-time
  rewrite, which makes it no longer the upstream sample.

## Sprints

- [P13-S1 — Harness and Tier A core/shapes](../../sprints/024-phase13-harness-and-tier-a.md) — **done**;
  gate `G0`; 9 samples; evidence `/home/psp/evidence/sprint024/`; next is P13-S2
- [P13-S2 — Tier A textures and texture paths](../../sprints/025-phase13-texture-paths.md) — **done**;
  gate `G0`; 12 samples; evidence `/home/psp/evidence/sprint025/`; Tier A complete; next is P13-S3
- [P13-S3 — Input remap and core input slice](../../sprints/026-phase13-input-remap-and-core-slice.md) —
  placeholder; gate `G1`; 10 samples
- [P13-S4 — Core cameras and UI](../../sprints/027-phase13-core-cameras-and-ui.md) —
  placeholder; gate `G1`; 10 samples
- [P13-S5 — Shapes sweep A](../../sprints/028-phase13-shapes-sweep-a.md) —
  placeholder; gate `G1`; 17 samples
- [P13-S6 — Shapes sweep B and testbeds](../../sprints/029-phase13-shapes-sweep-b.md) —
  placeholder; gate `G1`; 16 samples + 5 testbeds
- [P13-S7 — Texture budget](../../sprints/030-phase13-texture-budget.md) —
  **written**; gate `G2`; 7 samples + 45° `image_rotate`; T2 (`image_channel`) dropped to S8
- [P13-S8 — Oversize assets](../../sprints/031-phase13-oversize-assets.md) —
  placeholder; gate `G3`; 4 samples + asset prep
- [P13-S9 — Render targets: core and shapes](../../sprints/032-phase13-render-targets-core-shapes.md) —
  placeholder; gate `G4`; 11 samples
- [P13-S10 — Render targets: textures](../../sprints/033-phase13-render-targets-textures.md) —
  placeholder; gate `G4`; 5 samples
- [P13-S11 — Remaining features and close](../../sprints/034-phase13-remaining-features-and-close.md) —
  placeholder; gates `G5`–`G7`; 5 samples + 12 drops + phase close

## Related

- [Memory map §5 — roadmap](../../memory-map.md#5-roadmap)
- [Phase 5 — Native GU backend](../05-native-gu-backend/index.md) and [Phase 6 — Textures and VRAM](../06-textures-vram/index.md)
- [Phase 7 — 2D specialisation](../07-2d-specialisation/index.md) for the 512-pixel window split
- [Phase 8 — Input](../08-input/index.md) for the nub/pad mapping
- [Phase 9 — Audio](../09-audio/index.md) and [Phase 10 — 3D and advanced](../10-3d-advanced/index.md) for the remaining gates
- [Sprint template](../../templates/sprint.md) — fill a copy for each wave slice; leave the template intact
