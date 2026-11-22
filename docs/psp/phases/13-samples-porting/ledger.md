# Sample ledger

One row per sample in the upstream `examples/` tree, grouped by category. This is the single place where a
sample's state and portability are recorded; every porting sprint updates its rows.

- **Status** — `ported` means a device-validated port with two identical captures. `pending` is everything else.
- **Portability** — what the remaining work needs:
  - `possible` — no missing backend capability; input may need the pad remap or the pending `G1` nub->mouse
    mapping, assets fit.
  - `blocked` — a named gate or backend feature is missing.
  - `drop` — no PSP equivalent (touch-only, desktop-only).
  - `—` — already ported.

**Totals: 227 samples — 77 ported, 29 possible, 108 blocked, 13 drop.**

| Category | Samples | Ported | Possible | Blocked | Drop |
|---|---:|---:|---:|---:|---:|
| `core` | 49 | 24 | 0 | 13 | 12 |
| `shapes` | 45 | 24 | 17 | 4 | 0 |
| `textures` | 33 | 20 | 1 | 12 | 0 |
| `text` | 17 | 0 | 10 | 7 | 0 |
| `models` | 32 | 0 | 0 | 32 | 0 |
| `shaders` | 36 | 0 | 0 | 36 | 0 |
| `audio` | 11 | 9 | 0 | 2 | 0 |
| `others` | 4 | 0 | 1 | 2 | 1 |
| **total** | **227** | **77** | **29** | **108** | **13** |

A `possible` row that says `G1` needs the Phase 8 input remap before it is interactive; the renderer and assets
are already sufficient. A blocked row names the gate that has to close first (`G3` assets, `G4` render targets,
`G6` clipboard, `G7` readback, `G8` 3D/models, `G9` shaders), or the missing backend feature.

## core (49)

| Sample | Status | Portability | Note |
|---|---|---|---|
| `core_2d_camera` | ported | — | S4 |
| `core_2d_camera_mouse_zoom` | ported | — | S4 |
| `core_2d_camera_platformer` | ported | — | S4 |
| `core_2d_camera_split_screen` | pending | blocked | G4 render target |
| `core_3d_camera_first_person` | ported | — | S4 |
| `core_3d_camera_fps` | ported | — | S4 |
| `core_3d_camera_free` | ported | — | S4 |
| `core_3d_camera_mode` | ported | — | S1 |
| `core_3d_camera_split_screen` | pending | blocked | G4 render target |
| `core_3d_picking` | ported | — | S4 |
| `core_automation_events` | pending | drop | desktop-only |
| `core_basic_screen_manager` | ported | — | S3 |
| `core_basic_window` | ported | — | S1 |
| `core_clipboard_text` | pending | blocked | G6 clipboard |
| `core_compute_hash` | ported | — | S3 |
| `core_custom_frame_control` | ported | — | S3 |
| `core_custom_logging` | ported | — | S1 |
| `core_delta_time` | ported | — | S3 |
| `core_directory_files` | ported | — | S4 |
| `core_drop_files` | pending | drop | desktop-only |
| `core_highdpi_demo` | pending | drop | desktop-only |
| `core_highdpi_testbed` | pending | drop | desktop-only |
| `core_input_actions` | ported | — | S3 |
| `core_input_gamepad` | ported | — | P8-S1 |
| `core_input_gestures` | pending | drop | touch-only |
| `core_input_gestures_testbed` | pending | drop | touch-only |
| `core_input_keys` | pending | blocked | G1 pointer |
| `core_input_mouse` | pending | blocked | G1 pointer |
| `core_input_mouse_wheel` | pending | blocked | G1 pointer |
| `core_input_multitouch` | pending | drop | touch-only |
| `core_input_virtual_controls` | pending | drop | touch-only |
| `core_keyboard_testbed` | pending | blocked | G1 pointer |
| `core_monitor_detector` | pending | drop | desktop-only |
| `core_random_sequence` | ported | — | S3 |
| `core_random_values` | ported | — | S1 |
| `core_render_texture` | pending | blocked | G4 render target |
| `core_scissor_test` | ported | — | S3 |
| `core_screen_recording` | ported | — | S4 |
| `core_smooth_pixelperfect` | pending | blocked | G4 render target |
| `core_storage_values` | ported | — | S3 |
| `core_text_file_loading` | ported | — | S3 |
| `core_undo_redo` | pending | blocked | G1 pointer |
| `core_viewport_scaling` | pending | blocked | G4 render target |
| `core_vr_simulator` | pending | blocked | G4 render target |
| `core_window_flags` | pending | drop | desktop-only |
| `core_window_letterbox` | pending | blocked | G4 render target |
| `core_window_should_close` | pending | drop | desktop-only |
| `core_window_web` | pending | drop | desktop-only |
| `core_world_screen` | ported | — | S4 |

## shapes (45)

| Sample | Status | Portability | Note |
|---|---|---|---|
| `shapes_ball_physics` | ported | — | S5 |
| `shapes_basic_shapes` | ported | — | bring-up |
| `shapes_bouncing_ball` | ported | — | S5 |
| `shapes_bullet_hell` | pending | blocked | G4 render target |
| `shapes_circle_sector_drawing` | ported | — | S1 |
| `shapes_clock_of_clocks` | ported | — | S5 |
| `shapes_collision_area` | ported | — | bring-up |
| `shapes_colors_palette` | ported | — | S5 |
| `shapes_dashed_line` | ported | — | S5 |
| `shapes_digital_clock` | ported | — | S5 |
| `shapes_double_pendulum` | pending | blocked | G4 render target |
| `shapes_drag_puzzle` | ported | — | S5 |
| `shapes_easings_ball` | ported | — | S5 |
| `shapes_easings_box` | ported | — | S5 |
| `shapes_easings_rectangles` | ported | — | S5 |
| `shapes_easings_testbed` | ported | — | S5 |
| `shapes_ellipse_collision` | ported | — | S5 |
| `shapes_following_eyes` | ported | — | S5 |
| `shapes_hilbert_curve` | ported | — | S5 |
| `shapes_kaleidoscope` | pending | possible | pad remap; G1 if pointer |
| `shapes_lines_bezier` | ported | — | S5 |
| `shapes_lines_drawing` | pending | blocked | G4 render target |
| `shapes_logo_raylib` | ported | — | S1 |
| `shapes_logo_raylib_anim` | ported | — | bring-up |
| `shapes_math_angle_rotation` | ported | — | S1 |
| `shapes_math_sine_cosine` | ported | — | S5 |
| `shapes_mouse_trail` | pending | possible | pad remap; G1 if pointer |
| `shapes_outlines_testbed` | pending | possible | pad remap; G1 if pointer |
| `shapes_outlines_thickness` | ported | — | S1 |
| `shapes_penrose_tile` | pending | possible | pad remap; G1 if pointer |
| `shapes_pie_chart` | pending | possible | pad remap; G1 if pointer |
| `shapes_polygon_lines` | pending | possible | pad remap; G1 if pointer |
| `shapes_rectangle_advanced` | ported | — | S1 |
| `shapes_rectangle_scaling` | pending | possible | pad remap; G1 if pointer |
| `shapes_recursive_tree` | pending | possible | pad remap; G1 if pointer |
| `shapes_ring_drawing` | pending | possible | pad remap; G1 if pointer |
| `shapes_rlgl_color_wheel` | pending | possible | pad remap; G1 if pointer |
| `shapes_rlgl_triangle` | pending | possible | pad remap; G1 if pointer |
| `shapes_rounded_rectangle_drawing` | pending | possible | pad remap; G1 if pointer |
| `shapes_simple_particles` | pending | possible | pad remap; G1 if pointer |
| `shapes_splines_drawing` | pending | possible | pad remap; G1 if pointer |
| `shapes_starfield_effect` | pending | possible | pad remap; G1 if pointer |
| `shapes_top_down_lights` | pending | blocked | G4 render target |
| `shapes_triangle_strip` | pending | possible | pad remap; G1 if pointer |
| `shapes_vector_angle` | pending | possible | pad remap; G1 if pointer |

## textures (33)

| Sample | Status | Portability | Note |
|---|---|---|---|
| `textures_background_scrolling` | ported | — | S7 |
| `textures_blend_modes` | ported | — | S7 |
| `textures_bunnymark` | ported | — | bring-up |
| `textures_cellular_automata` | ported | — | S2 |
| `textures_clipboard_image` | pending | blocked | G6 clipboard |
| `textures_fog_of_war` | pending | blocked | G4 render target |
| `textures_framebuffer_rendering` | pending | blocked | G4 render target |
| `textures_gif_player` | ported | — | S2 |
| `textures_image_channel` | pending | blocked | G3 asset/VRAM |
| `textures_image_drawing` | pending | blocked | G3 asset/VRAM |
| `textures_image_generation` | pending | blocked | G3 asset/VRAM |
| `textures_image_kernel` | ported | — | S7 |
| `textures_image_loading` | ported | — | S2 |
| `textures_image_processing` | ported | — | S7 |
| `textures_image_rotate` | ported | — | S2 |
| `textures_image_text` | ported | — | S7 |
| `textures_logo_raylib` | ported | — | S2 |
| `textures_magnifying_glass` | pending | blocked | G4 render target |
| `textures_mouse_painting` | pending | blocked | G4 render target |
| `textures_npatch_drawing` | ported | — | S2 |
| `textures_particles_blending` | ported | — | S2 |
| `textures_polygon_drawing` | ported | — | S7 |
| `textures_portal_window` | pending | blocked | G4 render target |
| `textures_raw_data` | ported | — | S7 |
| `textures_screen_buffer` | ported | — | S2 |
| `textures_sprite_animation` | ported | — | S2 |
| `textures_sprite_button` | pending | possible | G1 pointer |
| `textures_sprite_explosion` | pending | blocked | G1 pointer + G3 asset |
| `textures_sprite_stacking` | pending | blocked | G3 asset/VRAM |
| `textures_srcrec_dstrec` | ported | — | S2 |
| `textures_textured_curve` | ported | — | S2 |
| `textures_tiled_drawing` | ported | — | S2 |
| `textures_to_image` | pending | blocked | G7 texture readback |

## text (17)

| Sample | Status | Portability | Note |
|---|---|---|---|
| `text_3d_drawing` | pending | blocked | G9 shader policy |
| `text_animations` | pending | possible | pad remap |
| `text_codepoints_loading` | pending | blocked | font atlas size unmeasured |
| `text_font_filters` | pending | possible | G1 pointer |
| `text_font_loading` | pending | blocked | G3 512x512 atlas/VRAM |
| `text_font_sdf` | pending | blocked | G9 shader policy |
| `text_font_spritefont` | pending | blocked | G3 three 512-wide atlases |
| `text_format_text` | pending | possible | no input |
| `text_inline_styling` | pending | possible | no input |
| `text_input_box` | pending | possible | G1 pointer |
| `text_rectangle_bounds` | pending | possible | G1 pointer |
| `text_sprite_fonts` | pending | possible | small assets |
| `text_strings_management` | pending | possible | G1 pointer |
| `text_unicode_emojis` | pending | blocked | G3 three atlases/VRAM |
| `text_unicode_ranges` | pending | blocked | G3 CJK atlas/VRAM |
| `text_words_alignment` | pending | possible | pad remap |
| `text_writing_anim` | pending | possible | pad remap |

## models (32)

| Sample | Status | Portability | Note |
|---|---|---|---|
| `models_animation_blend_custom` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_animation_blending` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_animation_gpu_skinning` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_animation_timing` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_basic_voxel` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_billboard_rendering` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_bone_socket` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_box_collisions` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_cubicmap_rendering` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_decals` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_directional_billboard` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_first_person_maze` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_geometric_shapes` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_heightmap_rendering` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_loading` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_loading_gltf` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_loading_iqm` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_loading_m3d` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_loading_vox` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_mesh_generation` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_mesh_picking` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_mesh_uv_painting` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_orthographic_projection` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_point_rendering` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_procedural_decals` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_rlgl_solar_system` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_rotating_cube` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_skybox_rendering` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_tesseract_view` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_textured_cube` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_waving_cubes` | pending | blocked | G8 3D mesh (Phase 10) |
| `models_yaw_pitch_roll` | pending | blocked | G8 3D mesh (Phase 10) |

## shaders (36)

| Sample | Status | Portability | Note |
|---|---|---|---|
| `shaders_ascii_rendering` | pending | blocked | G9 shader policy |
| `shaders_basic_lighting` | pending | blocked | G9 shader policy |
| `shaders_basic_pbr` | pending | blocked | G9 shader policy |
| `shaders_cel_shading` | pending | blocked | G9 shader policy |
| `shaders_color_correction` | pending | blocked | G9 shader policy |
| `shaders_custom_uniform` | pending | blocked | G9 shader policy |
| `shaders_deferred_rendering` | pending | blocked | G9 shader policy |
| `shaders_depth_rendering` | pending | blocked | G9 shader policy |
| `shaders_depth_writing` | pending | blocked | G9 shader policy |
| `shaders_eratosthenes_sieve` | pending | blocked | G9 shader policy |
| `shaders_fog_rendering` | pending | blocked | G9 shader policy |
| `shaders_game_of_life` | pending | blocked | G9 shader policy |
| `shaders_hot_reloading` | pending | blocked | G9 shader policy |
| `shaders_hybrid_rendering` | pending | blocked | G9 shader policy |
| `shaders_julia_set` | pending | blocked | G9 shader policy |
| `shaders_lightmap_rendering` | pending | blocked | G9 shader policy |
| `shaders_lights_bloom` | pending | blocked | G9 shader policy |
| `shaders_mandelbrot_set` | pending | blocked | G9 shader policy |
| `shaders_mesh_instancing` | pending | blocked | G9 shader policy |
| `shaders_model_shader` | pending | blocked | G9 shader policy |
| `shaders_multi_sample2d` | pending | blocked | G9 shader policy |
| `shaders_normalmap_rendering` | pending | blocked | G9 shader policy |
| `shaders_palette_switch` | pending | blocked | G9 shader policy |
| `shaders_postprocessing` | pending | blocked | G9 shader policy |
| `shaders_raymarching_rendering` | pending | blocked | G9 shader policy |
| `shaders_rlgl_compute` | pending | blocked | G9 shader policy |
| `shaders_rounded_rectangle` | pending | blocked | G9 shader policy |
| `shaders_shadowmap_rendering` | pending | blocked | G9 shader policy |
| `shaders_shapes_textures` | pending | blocked | G9 shader policy |
| `shaders_simple_mask` | pending | blocked | G9 shader policy |
| `shaders_spotlight_rendering` | pending | blocked | G9 shader policy |
| `shaders_texture_outline` | pending | blocked | G9 shader policy |
| `shaders_texture_rendering` | pending | blocked | G9 shader policy |
| `shaders_texture_tiling` | pending | blocked | G9 shader policy |
| `shaders_texture_waves` | pending | blocked | G9 shader policy |
| `shaders_vertex_displacement` | pending | blocked | G9 shader policy |

## audio (11)

| Sample | Status | Portability | Note |
|---|---|---|---|
| `audio_amp_envelope` | ported | — | S13 |
| `audio_mixed_processor` | ported | — | S12 |
| `audio_module_playing` | ported | — | S12 |
| `audio_music_stream` | ported | — | S12 |
| `audio_raw_stream` | ported | — | S13 |
| `audio_sound_loading` | ported | — | S12 |
| `audio_sound_multi` | ported | — | S12 |
| `audio_sound_positioning` | pending | blocked | G8 3D (Phase 10) |
| `audio_spectrum_visualizer` | pending | blocked | G9 shader policy |
| `audio_stream_callback` | ported | — | S13 |
| `audio_stream_effects` | ported | — | S13 |

## others (4)

| Sample | Status | Portability | Note |
|---|---|---|---|
| `embedded_files_loading` | pending | possible | embedded assets |
| `gl_scissor_shapes` | pending | blocked | stencil unsupported |
| `raylib_opengl_interop` | pending | drop | desktop GL interop |
| `rlgl_standalone` | pending | blocked | rl* wrappers not exported |

## How this is maintained

- The ledger is owned by [P13-S19](../../sprints/075-phase13-corpus-ledger.md), a continuous bookkeeping sprint.
- Every sprint that ports, drops or defers a sample updates that sample's row and the summary table in the same
  commit. The sprint's exit criteria include a ledger check.
- Do not renumber or remove rows: a dropped sample stays in the table with `drop` and a reason, so the category
  total always sums to the upstream count.
- The upstream tree can grow. Add new rows under their category, update the totals, and re-check the sum.
