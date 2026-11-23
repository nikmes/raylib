# Samples

Screenshots of every sample ported so far, captured on a PSP-1000 at 480×272. Each row keeps the sample name
beside its screenshot; the capture shown is the first of a byte-identical pair, and the second was required to
match before it was trusted. The page grows with [Phase 13](../psp/phases/13-samples-porting/index.md) and will
eventually cover all 227 upstream samples across the eight categories.

## PSP

Samples written for this port rather than ported from upstream raylib.

| Sample | Screenshot |
|---|---|
| `psp_showcase` | ![psp_showcase](images/psp_showcase.png) |
| `psp_2d_showcase` | ![psp_2d_showcase](images/psp_2d_showcase.png) |

**`psp_showcase`** — a stress demo for the 3D path as it stands after sprint 078. 400 lit, textured cubes ride a
sine wave under an orbiting camera, split into two merged colour runs (cool/warm), over a fogged checker ground,
with a 2D HUD. It draws about **24 fps** at 480×272 on a PSP-1000, with **2 merged draws** for the 400 cubes and
one per-object draw for the ground; the HUD reports the live count and frame rate.

Source: `psp-tests/samples/psp/psp_showcase/`. Build with `make` (runs until power-off) or `make FRAMES=600`
(stops after 600 frames, prints statistics and sleeps, which is what the capture workflow needs).

**`psp_2d_showcase`** — the 2D counterpart and the other half of the benchmark pair. 1,300 additive glow
particles in three arms, a 250-star field, a 160-particle hot core and 16 ring sprites, over a 40-band gradient:
about **1,566 sprites** drawn in **6 draws / 4 texture runs** per frame at **29.6 fps** (33.8 ms). The animation is
driven by the **frame index**, so frame N draws exactly the same scene on every build; the counters come from the
link-time `--wrap` harness (`draws`, `binds`, vertices), which is what makes a before/after comparison meaningful.

Source: `psp-tests/samples/psp/psp_2d_showcase/`. `make FRAMES=300` for the capture build, `make SPRITES=3000
STARS=0` to sweep the load.

## Core

| Sample | Screenshot |
|---|---|
| `core_basic_window` | ![core_basic_window](images/core_basic_window.png) |
| `core_custom_logging` | ![core_custom_logging](images/core_custom_logging.png) |
| `core_random_values` | ![core_random_values](images/core_random_values.png) |
| `core_3d_camera_mode` | ![core_3d_camera_mode](images/core_3d_camera_mode.png) |
| `core_2d_camera_mouse_zoom` | ![core_2d_camera_mouse_zoom](images/core_2d_camera_mouse_zoom.png) |
| `core_2d_camera_platformer` | ![core_2d_camera_platformer](images/core_2d_camera_platformer.png) |
| `core_2d_camera` | ![core_2d_camera](images/core_2d_camera.png) |
| `core_3d_camera_first_person` | ![core_3d_camera_first_person](images/core_3d_camera_first_person.png) |
| `core_3d_camera_fps` | ![core_3d_camera_fps](images/core_3d_camera_fps.png) |
| `core_3d_camera_free` | ![core_3d_camera_free](images/core_3d_camera_free.png) |
| `core_3d_picking` | ![core_3d_picking](images/core_3d_picking.png) |
| `core_basic_screen_manager` | ![core_basic_screen_manager](images/core_basic_screen_manager.png) |
| `core_compute_hash` | ![core_compute_hash](images/core_compute_hash.png) |
| `core_custom_frame_control` | ![core_custom_frame_control](images/core_custom_frame_control.png) |
| `core_delta_time` | ![core_delta_time](images/core_delta_time.png) |
| `core_directory_files` | ![core_directory_files](images/core_directory_files.png) |
| `core_input_actions` | ![core_input_actions](images/core_input_actions.png) |
| `core_random_sequence` | ![core_random_sequence](images/core_random_sequence.png) |
| `core_scissor_test` | ![core_scissor_test](images/core_scissor_test.png) |
| `core_screen_recording` | ![core_screen_recording](images/core_screen_recording.png) |
| `core_storage_values` | ![core_storage_values](images/core_storage_values.png) |
| `core_text_file_loading` | ![core_text_file_loading](images/core_text_file_loading.png) |
| `core_world_screen` | ![core_world_screen](images/core_world_screen.png) |

## Shapes

| Sample | Screenshot |
|---|---|
| `shapes_logo_raylib` | ![shapes_logo_raylib](images/shapes_logo_raylib.png) |
| `shapes_math_angle_rotation` | ![shapes_math_angle_rotation](images/shapes_math_angle_rotation.png) |
| `shapes_outlines_thickness` | ![shapes_outlines_thickness](images/shapes_outlines_thickness.png) |
| `shapes_circle_sector_drawing` | ![shapes_circle_sector_drawing](images/shapes_circle_sector_drawing.png) |
| `shapes_rectangle_advanced` | ![shapes_rectangle_advanced](images/shapes_rectangle_advanced.png) |
| `shapes_ball_physics` | ![shapes_ball_physics](images/shapes_ball_physics.png) |
| `shapes_basic_shapes` | ![shapes_basic_shapes](images/shapes_basic_shapes.png) |
| `shapes_bouncing_ball` | ![shapes_bouncing_ball](images/shapes_bouncing_ball.png) |
| `shapes_clock_of_clocks` | ![shapes_clock_of_clocks](images/shapes_clock_of_clocks.png) |
| `shapes_colors_palette` | ![shapes_colors_palette](images/shapes_colors_palette.png) |
| `shapes_dashed_line` | ![shapes_dashed_line](images/shapes_dashed_line.png) |
| `shapes_digital_clock` | ![shapes_digital_clock](images/shapes_digital_clock.png) |
| `shapes_drag_puzzle` | ![shapes_drag_puzzle](images/shapes_drag_puzzle.png) |
| `shapes_easings_ball` | ![shapes_easings_ball](images/shapes_easings_ball.png) |
| `shapes_easings_box` | ![shapes_easings_box](images/shapes_easings_box.png) |
| `shapes_easings_rectangles` | ![shapes_easings_rectangles](images/shapes_easings_rectangles.png) |
| `shapes_easings_testbed` | ![shapes_easings_testbed](images/shapes_easings_testbed.png) |
| `shapes_ellipse_collision` | ![shapes_ellipse_collision](images/shapes_ellipse_collision.png) |
| `shapes_following_eyes` | ![shapes_following_eyes](images/shapes_following_eyes.png) |
| `shapes_hilbert_curve` | ![shapes_hilbert_curve](images/shapes_hilbert_curve.png) |
| `shapes_lines_bezier` | ![shapes_lines_bezier](images/shapes_lines_bezier.png) |
| `shapes_math_sine_cosine` | ![shapes_math_sine_cosine](images/shapes_math_sine_cosine.png) |

## Textures

| Sample | Screenshot |
|---|---|
| `textures_image_loading` | ![textures_image_loading](images/textures_image_loading.png) |
| `textures_logo_raylib` | ![textures_logo_raylib](images/textures_logo_raylib.png) |
| `textures_screen_buffer` | ![textures_screen_buffer](images/textures_screen_buffer.png) |
| `textures_srcrec_dstrec` | ![textures_srcrec_dstrec](images/textures_srcrec_dstrec.png) |
| `textures_cellular_automata` | ![textures_cellular_automata](images/textures_cellular_automata.png) |
| `textures_gif_player` | ![textures_gif_player](images/textures_gif_player.png) |
| `textures_image_rotate` | ![textures_image_rotate](images/textures_image_rotate.png) |
| `textures_npatch_drawing` | ![textures_npatch_drawing](images/textures_npatch_drawing.png) |
| `textures_particles_blending` | ![textures_particles_blending](images/textures_particles_blending.png) |
| `textures_sprite_animation` | ![textures_sprite_animation](images/textures_sprite_animation.png) |
| `textures_textured_curve` | ![textures_textured_curve](images/textures_textured_curve.png) |
| `textures_tiled_drawing` | ![textures_tiled_drawing](images/textures_tiled_drawing.png) |
| `textures_image_kernel` | ![textures_image_kernel](images/textures_image_kernel.png) |
| `textures_image_processing` | ![textures_image_processing](images/textures_image_processing.png) |
| `textures_image_text` | ![textures_image_text](images/textures_image_text.png) |
| `textures_polygon_drawing` | ![textures_polygon_drawing](images/textures_polygon_drawing.png) |
| `textures_raw_data` | ![textures_raw_data](images/textures_raw_data.png) |
| `textures_background_scrolling` | ![textures_background_scrolling](images/textures_background_scrolling.png) |
| `textures_blend_modes` | ![textures_blend_modes](images/textures_blend_modes.png) |
| `textures_bunnymark` | ![textures_bunnymark](images/textures_bunnymark.png) |

`textures_image_rotate` shows the 45° case introduced in P13-S7; the 90° and -90° rotations were captured in
P13-S2. The P13-S2 and P13-S7 texture samples use different pspgu arms where needed; the sprint pages record which
arm produced each capture.

## Audio

Nine of the eleven `examples/audio` samples. `audio_sound_positioning` waits for Phase 10's 3D path;
`audio_spectrum_visualizer` waits for the P13-S16 shader policy.

| Sample | Screenshot |
|---|---|
| `audio_sound_loading` | ![audio_sound_loading](images/audio_sound_loading.png) |
| `audio_sound_multi` | ![audio_sound_multi](images/audio_sound_multi.png) |
| `audio_music_stream` | ![audio_music_stream](images/audio_music_stream.png) |
| `audio_mixed_processor` | ![audio_mixed_processor](images/audio_mixed_processor.png) |
| `audio_module_playing` | ![audio_module_playing](images/audio_module_playing.png) |
| `audio_raw_stream` | ![audio_raw_stream](images/audio_raw_stream.png) |
| `audio_stream_callback` | ![audio_stream_callback](images/audio_stream_callback.png) |
| `audio_amp_envelope` | ![audio_amp_envelope](images/audio_amp_envelope.png) |
| `audio_stream_effects` | ![audio_stream_effects](images/audio_stream_effects.png) |

Not in this gallery: `batch_check` (the deterministic render gate). Its captures are archived in
`/home/psp/evidence/sprint030/`.
