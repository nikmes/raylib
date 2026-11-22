# Phase 13 — Samples porting

| | |
|---|---|
| State | **in progress** — scope is the whole upstream corpus (227 samples, 8 categories); Tier A/B core/shapes/textures 68 ported; audio 9; text/models/shaders/others not yet audited; P13-S7 T1b `batch_check` gate blocked |
| Depends on | nothing for wave 0; later waves consume Phase 6 (texture budget), Phase 7 (oversize assets), Phase 8 (input), Phase 9 (audio), Phase 10 (3D/models) |
| Exit criterion | every sample in every upstream category — `core`, `shapes`, `textures`, `text`, `models`, `shaders`, `audio`, `others` — is either ported with a build marker and two identical captures, or marked `dropped`/deferred with a reason; the per-category tables match the device runs |

## What this phase is about

The upstream example corpus is the only broad regression suite raylib has. Our current corpus — `basic_shapes`,
`collision_area`, `logo_raylib_anim`, `bunnymark`, `cubicmap` plus the purpose-built probes — covers the paths
that mattered for phases 1–5, not the API surface a game will use. A port is also a capability test: a sample
that renders proves a path works, and a sample that does not names the missing capability precisely.

This phase is **continuous and capability-gated**, not a chronological successor to Phase 12. Its waves consume
the outputs of the renderer, input and audio phases. It never blocks them, and no other phase should wait for it.
It is placed after Phase 12 only because it spans every phase before it.

This phase is also the permanent home for the **whole** upstream example corpus — all **227** samples in eight
categories, not only the first three. A category enters the tables only after an audit, and a sample counts as
ported only when a device run exists; everything else stays explicitly unaudited or deferred.

Scope: `examples/core` (49), `shapes` (45), `textures` (33), `text` (17), `models` (32), `shaders` (36),
`audio` (11), `others` (4) — **227 samples**. `examples/examples_template.c` is the template, not a sample.

| category | samples | audit | home |
|---|---|---|---|
| `core` | 49 | audited 2026-11-21 | P13-S1, S3–S4, S11 |
| `shapes` | 45 | audited 2026-11-21 | P13-S1, S5–S6 |
| `textures` | 33 | audited 2026-11-21 | P13-S2, S7–S10 |
| `audio` | 11 | audited 2026-11-22 | P13-S12–S13 |
| `text` | 17 | not audited | P13-S14 (`G1`) |
| `models` | 32 | not audited | P13-S15, after Phase 10 (`G8`) |
| `shaders` | 36 | not audited | P13-S16, rewrite/drop (`G9`) |
| `others` | 4 | not audited | P13-S17, per sample |
| **total** | **227** | 4 of 8 categories | |

The four audited categories are tabulated below. The remaining 89 samples are counted as not ported until their
category is audited; they are not silently folded into a tier.

Per-sample state is kept in one place: the [sample ledger](ledger.md) lists all **227** rows by raylib category
with `ported`/`pending` status and a `possible`/`blocked`/`drop` verdict. Every porting sprint updates it.

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
No sample was built or run for this audit; wave 0 exists to validate the classification. This audit covers the
127 core/shapes/textures samples **only**; text, models, shaders and others were added to the phase scope on
2026-11-22 and are **unaudited**.

**Already ported (4):** `shapes_basic_shapes`, `shapes_collision_area`, `shapes_logo_raylib_anim`,
`textures_bunnymark`. `models/cubicmap` was validated in sprint 002 in the external sample tree, but it is not in
`psp-tests/samples/`; it counts only after it is re-homed, so models remain 0 ported here.

**Ported since the audit (1):** `core_input_gamepad` at 480×272 in
`psp-tests/samples/core/input_gamepad/`, with a primitive layout. Device-validated on the PSP-1000: all 12
button enums, 1800 frames, 60 fps, log and capture in `/home/psp/evidence/sprint041/`. It moves from Tier B to
ported (Tier B 60, ported 5) without changing the audit's total.

| tier | core | shapes | textures | total |
|---|---|---|---|---|
| ported | 1 | 3 | 1 | **5** |
| A — drop-in | 4 | 5 | 4 | **13** |
| B — renders, input needs remap | 19 | 33 | 8 | **60** |
| C — blocked or not meaningful | 25 | 4 | 20 | **49** |
| **total** | **49** | **45** | **33** | **127** |

Audio adds **11 samples** outside these tiers (9 portable, 2 deferred). The four unaudited categories add **89**
more: `text` 17, `models` 32, `shaders` 36, `others` 4. The whole corpus is **227 samples**.

Progress: **77/227 ported** — 4 bring-up + `core_input_gamepad` (P8-S1) + 13 Tier A + 8 Tier B textures
(P13-S2) + 7 Tier C `G2` samples (P13-S7) + 24 Tier B core/shapes (S3/S5) + 11 Tier B core cameras/UI (S4) +
**9 audio** (P13-S12/S13). Remaining: **17 Tier B core/shapes**, **42 Tier C**, **2 audio deferred**, and
**89 unaudited** (`text` 17, `models` 32, `shaders` 36, `others` 4).

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
- **Audio is implemented.** Phase 9 closed route A on 2026-11-22: `Sound`, `Music`, `AudioStream`, callbacks and
  stream processors all run on the device, and gate `G5` is closed. The nine portable `examples/audio` samples
  port in P13-S12/S13; two are deferred for 3D or shader reasons.
- **Text is implemented; models are not.** Phase 7's font, atlas and dynamic-text path is done, so the 17 `text`
  samples need an audit rather than a new capability. `DrawMesh` still produces no pixels, so the 32 `models`
  samples wait for `G8` (Phase 10).

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
- **C — blocked or not meaningful.** Either a backend feature is missing (render targets, clipboard,
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

## Tier B — renders now, interaction needs a remap (60)

The frame is valid; only the interaction is dead. `G1` is the Phase 8 mapping of the nub to
`CORE.Input.Mouse` and the pad to `CORE.Input.Keyboard`, or per-sample pad handling.

**core (19):** `core_2d_camera`, `core_2d_camera_mouse_zoom`, `core_2d_camera_platformer`,
`core_3d_camera_first_person`, `core_3d_camera_fps`, `core_3d_camera_free`, `core_3d_picking`,
`core_basic_screen_manager`, `core_compute_hash`, `core_custom_frame_control`, `core_delta_time`,
`core_directory_files`, `core_input_actions`, `core_random_sequence`,
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

## Tier C — blocked or not meaningful (42 remaining; 7 ported in P13-S7)

**Render targets / FBO (16).** The target texture stays blank under GL11:
`core_2d_camera_split_screen`, `core_3d_camera_split_screen`, `core_render_texture`,
`core_smooth_pixelperfect`, `core_viewport_scaling`, `core_window_letterbox`, `core_vr_simulator`;
`shapes_bullet_hell`, `shapes_double_pendulum`, `shapes_lines_drawing`, `shapes_top_down_lights`;
`textures_fog_of_war`, `textures_framebuffer_rendering`, `textures_magnifying_glass`,
`textures_mouse_painting`, `textures_portal_window`. `core_vr_simulator` also needs shaders.

**Texture size / VRAM (4 remaining; 7 ported in P13-S7).**
- The seven `G2` samples — `image_kernel`, `image_processing`, `image_text`, `polygon_drawing`,
  `raw_data`, `background_scrolling`, `blend_modes` — and the 45° `image_rotate` case were ported in P13-S7.
- Beyond the 1024 buffer, or needs a downscale/rewrite; `G3`: `textures_image_channel` (six textures),
  `textures_image_drawing` (non-binary font atlas), `textures_sprite_stacking` (11468-tall sheet),
  `textures_image_generation` (nine 1 MiB textures plus the heap).

**Audio gate closed, input remains (2).** Phase 9 shipped audio on 2026-11-22, so audio no longer blocks these.
`textures_sprite_button` now needs only `G1` (mouse); `textures_sprite_explosion` needs `G1` plus the
1024×1024 = 4 MiB asset (`G3`). They stay here until their remaining gate opens; `G1` will move `sprite_button`
to Tier B.

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

## Audio corpus (11)

The scope was extended on 2026-11-22, when Phase 9 closed `G5`. The audio samples are a fifth corpus, not a tier:
nine are ported in P13-S12/S13 and two are deferred with a reason and a trigger.

| sample | API path | verdict |
|---|---|---|
| `audio_sound_loading` | `LoadSound` WAV + OGG | ported (S12) |
| `audio_sound_multi` | `LoadSound` + `LoadSoundAlias` | ported (S12) |
| `audio_music_stream` | `LoadMusicStream` MP3 | ported (S12) |
| `audio_mixed_processor` | `Music` + `Sound` + `AttachAudioStreamProcessor` | ported (S12) |
| `audio_module_playing` | `LoadMusicStream` XM | ported (S12) |
| `audio_raw_stream` | `LoadAudioStream` + `UpdateAudioStream` | ported (S13) |
| `audio_stream_callback` | `SetAudioStreamCallback` | ported (S13) |
| `audio_amp_envelope` | envelope + `IsAudioStreamProcessed` | ported (S13) |
| `audio_stream_effects` | `AttachAudioStreamProcessor` LPF + delay | ported (S13) |
| `audio_sound_positioning` | `SetSoundVolume` + `SetSoundPan` | deferred — Phase 10 |
| `audio_spectrum_visualizer` | `LoadWave` + FFT + `BeginShaderMode` | deferred — rewrite or drop |

**Costs closed by P9-S8.** MP3 and XM now decode on the feeder; `audio_stream_effects` went 50 → 58 fps,
and the remaining 2 fps is sample-side (drawing/effects). `audio_module_playing` stays 30 fps because its 64
circles, not audio, are the bound; the XM audio path is clean (0 underruns). The 30 fps entries in S12/S13
are upstream `SetTargetFPS(30)` caps, not regressions. See
[P9-S8](../../sprints/068-phase9-music-format-coverage.md).

Keyboard input is still dead (`G1` is not done), so the ports call the gamepad API directly; P8-S1 already fills
it. One row per upstream key:

| upstream key | PSP button |
|---|---|
| `KEY_SPACE` (play/pause) | CROSS |
| `KEY_ENTER` (second sound) | SQUARE |
| `KEY_P` (stop) | CIRCLE |
| `KEY_UP` / `KEY_DOWN` | D-pad up/down |
| `KEY_LEFT` / `KEY_RIGHT` | D-pad left/right |
| effect toggles (`F`, `D`, …) | TRIANGLE |

**Deferred.** `audio_sound_positioning` is a 3D scene (`BeginMode3D`, `DrawSphere`, `DrawGrid`); its audio path
already works, so it reopens with Phase 10. `audio_spectrum_visualizer` loads `fft.fs` and draws through
`BeginShaderMode`; the GU backend has no programmable shaders, so it is a CPU-FFT/2D rewrite or an explicit drop
at the P13-S16 shader policy. No audio sample needs capture/recording.

Acceptance is stricter than a screenshot: each sample needs the backend's `callbacks=… errors=0` line, a non-zero
`GetMusicTimePlayed` where it exists, and a listening note naming what was heard. Two identical captures still
prove the frame, not the sound.

## Capability gates

| gate | capability | owner phase | unlocks |
|---|---|---|---|
| `G0` | harness, 480×272, asset staging | this phase | 13 Tier A + 8 small-asset texture samples |
| `G1` | nub→`Mouse`, pad→`Keyboard`, gamepad name | Phase 8 | 17 remaining Tier B core/shapes (35 ported with the sample-local pad/nub layer), 5 input testbeds |
| `G2` | depth-off build and/or `PSP_GU_TEXFMT=auto`, larger heap | Phase 6, Phase 11 | 7 texture samples + 45° `image_rotate` — **done in P13-S7** |
| `G3` | downscale or slice assets beyond 1024 or over budget | Phase 7 (windows already done in sprint 020) | 4 texture samples + pad art + explosion |
| `G4` | render targets / FBO on the native backend | Phase 10 (with Phase 5) | 16 render-target samples |
| `G5` | `sceAudio` output | Phase 9 | **closed 2026-11-22** — the 2 texture samples' audio blocker is cleared (`G1`/`G3` remain); 9 audio samples in P13-S12/S13 |
| `G6` | clipboard | Phase 8 / platform | 2 clipboard samples |
| `G7` | texture readback (`glGetTexImage` equivalent) | Phase 6 | 1 readback sample |
| `G8` | 3D mesh and model path (`DrawMesh`, model load, lighting) | Phase 10 | 32 model samples + the 3D Tier B core/shapes |
| `G9` | shader policy: CPU effect rewrite or explicit drop | none — the GU backend has no programmable shaders | 36 shader samples |

`G2` is closed by P13-S7 and `G5` by Phase 9. `G8` follows Phase 10; `G9` has no owner on purpose — the policy
is decided in the shader sprint, sample by sample. `G2` and `G3` are separate on purpose: a 1 MiB padded texture
fits the 1024 buffer and still does not fit the 8888 VRAM budget, while a 11468-tall sheet fits neither.

## Wave plan

| wave | gate | samples | exit |
|---|---|---|---|
| W0 — Tier A | `G0` | 13 | all 13 render; captures archived; harness reusable by copy + Makefile include |
| W0b — small-asset textures | `G0` | 8 | eight texture paths render (`UpdateTexture`, GIF, NPatch, blend, wrap/filter, rlgl curve, sheet windows); interaction deferred |
| W1 — input remap | `G1` | 17 | 35 Tier B samples ported with the sample-local pad/nub layer; 17 remain plus 5 testbeds |
| W2 — texture budget | `G2`/`G3` | 11 | each asset's padded size, PSM and build flags recorded; visual delta counted where an asset is downscaled |
| W3 — render targets | `G4` | 16 | render-to-texture path works; the 16 samples render |
| W4 — remaining features | `G1`, `G3`, `G6`, `G7` | 5 | mouse, asset, clipboard and readback samples either render or are dropped |
| W5 — audio | `G5` (closed) | 11 | 9 render and are audible; 2 deferred with a reason and a trigger |
| W6 — text | `G1` | 17 | audit, then port; font/atlas paths already exist |
| W7 — models | `G8` | 32 | audit and port after Phase 10's 3D path |
| W8 — shaders | `G9` | 36 | each is rewritten with a CPU effect or dropped with a reason |
| W9 — others | per sample | 4 | embedded files, direct GL, interop and rlgl assessed one by one |
| W10 — all-corpus close | — | — | exit table over 227; every sample ported or dropped |
| dropped | — | 12 | each carries a one-line reason in this page; 4 touch-only, 8 desktop-only |

5 ported + 13 + 8 + 57 + 11 + 16 + 5 + 12 = 127 core/shapes/textures; + 11 audio + 17 text + 32 models +
36 shaders + 4 others = **227**.

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
5. Use the shared `psp-tests/samples/Makefile.inc` (`TARGET = <name>` + `include ../../Makefile.inc`); it keeps
   the pspgu link line and appends `-lpspaudio -lpthread -latomic` after `build.mak`. Set `PSP_HEAP_SIZE_KB`
   explicitly when the sample holds full-screen images.
6. Stage assets with `usbhostfs_pc` rooted at `examples/<category>` and address them as `host0:/resources/...`,
   or copy them beside the sample.
7. Capture per [§6](../../memory-map.md#6-measurement-protocol): two identical captures, a visible build marker, and
   the frame log stopped before the capture.
8. Update the [sample ledger](ledger.md) row and the summary counts in the same commit. A sprint that changes a
   sample's state and leaves the ledger stale is not done.

## What we already know

- **A single remap unlocks most of Tier B.** The memory map's Phase 8 already asks whether to feed
  `CORE.Input.Keyboard` from the pad; the audit makes the payoff explicit: 57 samples.
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

- **The denominator is 227, not 138.** Quote a fraction with the categories it covers: 33/227 is the whole corpus;
  a smaller denominator silently excludes the unaudited categories.
- **Shaders are the largest block with no backend.** 36 samples need a policy — CPU rewrite for a few, explicit
  drop for the rest — before that wave can be estimated.
- **Models wait on Phase 10's `DrawMesh` fix.** `models/cubicmap`'s early port lives in the external sample tree,
  not `psp-tests/samples/`; re-home it before it is counted.
- **`text` may hide input and asset work.** The 17 samples are mostly font atlases, but they must be audited
  before a sprint count is promised.
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
  **done**; 10/10 with the sample-local pad/nub layer; evidence `/home/psp/evidence/p13-possible/` and
  `/home/psp/evidence/p13-core11/`
- [P13-S4 — Core cameras and UI](../../sprints/027-phase13-core-cameras-and-ui.md) —
  **done**; 10/10; evidence `/home/psp/evidence/p13-core11/`
- [P13-S5 — Shapes sweep A](../../sprints/028-phase13-shapes-sweep-a.md) —
  **in progress**; 16/17; `shapes_kaleidoscope` remains; evidence `/home/psp/evidence/p13-possible/`
- [P13-S6 — Shapes sweep B and testbeds](../../sprints/029-phase13-shapes-sweep-b.md) —
  placeholder; gate `G1`; 16 samples + 5 testbeds
- [P13-S7 — Texture budget](../../sprints/030-phase13-texture-budget.md) —
  **in progress**; gate `G2`; 7 samples + 45° `image_rotate` captured, two identical each; T1b `batch_check`
  gate blocked by a shared `libraylib.a` rebuild; T2 (`image_channel`) dropped to S8
- [P13-S8 — Oversize assets](../../sprints/031-phase13-oversize-assets.md) —
  placeholder; gate `G3`; 4 samples + asset prep
- [P13-S9 — Render targets: core and shapes](../../sprints/032-phase13-render-targets-core-shapes.md) —
  placeholder; gate `G4`; 11 samples
- [P13-S10 — Render targets: textures](../../sprints/033-phase13-render-targets-textures.md) —
  placeholder; gate `G4`; 5 samples
- [P13-S11 — Remaining features](../../sprints/034-phase13-remaining-features.md) —
  placeholder; gates `G1`/`G3`/`G6`/`G7`; 5 samples + 12 drops
- [P13-S12 — Audio: Sound and Music batch](../../sprints/063-phase13-audio-sound-and-music.md) — **done**;
  gate `G5` (closed); 5 samples; evidence `/home/psp/evidence/sprint063/`; next is P13-S13
- [P13-S13 — Audio: AudioStream batch](../../sprints/064-phase13-audio-stream.md) —
  **done**; gate `G5` (closed); 4 samples; evidence `/home/psp/evidence/sprint064/`; the phase stays open for
  W6–W10

Future waves, not written (the global `NNN` is assigned when each sprint opens):

- **P13-S14 — Text corpus** — 17 samples; gate `G1`
- **P13-S15 — Models corpus** — 32 samples; gate `G8`, after Phase 10
- **P13-S16 — Shader policy** — 36 samples; gate `G9`, each rewritten or dropped
- **P13-S17 — Others** — 4 samples, assessed individually
- **P13-S18 — All-corpus close** — the 227-sample exit audit

- [P13-S19 — Corpus ledger](../../sprints/075-phase13-corpus-ledger.md) — **continuous**; owns the
  [sample ledger](ledger.md) and keeps its 227 rows and summary current. It sits outside the wave order and
  never closes while Phase 13 is open.

## Related

- [Memory map §5 — roadmap](../../memory-map.md#5-roadmap)
- [Phase 5 — Native GU backend](../05-native-gu-backend/index.md) and [Phase 6 — Textures and VRAM](../06-textures-vram/index.md)
- [Phase 7 — 2D specialisation](../07-2d-specialisation/index.md) for the 512-pixel window split
- [Phase 8 — Input](../08-input/index.md) for the nub/pad mapping
- [Phase 9 — Audio](../09-audio/index.md) and [Phase 10 — 3D and advanced](../10-3d-advanced/index.md) for the remaining gates
- [Sprint template](../../templates/sprint.md) — fill a copy for each wave slice; leave the template intact
