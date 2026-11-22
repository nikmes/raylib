# Samples

Screenshots of every sample ported so far, captured on a PSP-1000 at 480×272. Each row keeps the sample name
beside its screenshot; the capture shown is the first of a byte-identical pair, and the second was required to
match before it was trusted. The page grows with [Phase 13](../psp/phases/13-samples-porting/index.md) and will
eventually cover all 227 upstream samples across the eight categories.

## Core

| Sample | Screenshot |
|---|---|
| `core_basic_window` | ![core_basic_window](images/core_basic_window.png) |
| `core_custom_logging` | ![core_custom_logging](images/core_custom_logging.png) |
| `core_random_values` | ![core_random_values](images/core_random_values.png) |
| `core_3d_camera_mode` | ![core_3d_camera_mode](images/core_3d_camera_mode.png) |

## Shapes

| Sample | Screenshot |
|---|---|
| `shapes_logo_raylib` | ![shapes_logo_raylib](images/shapes_logo_raylib.png) |
| `shapes_math_angle_rotation` | ![shapes_math_angle_rotation](images/shapes_math_angle_rotation.png) |
| `shapes_outlines_thickness` | ![shapes_outlines_thickness](images/shapes_outlines_thickness.png) |
| `shapes_circle_sector_drawing` | ![shapes_circle_sector_drawing](images/shapes_circle_sector_drawing.png) |
| `shapes_rectangle_advanced` | ![shapes_rectangle_advanced](images/shapes_rectangle_advanced.png) |

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

`textures_image_rotate` shows the 45° case introduced in P13-S7; the 90° and -90° rotations were captured in
P13-S2. The P13-S2 and P13-S7 texture samples use different pspgu arms where needed; the sprint pages record which
arm produced each capture.

Not in this gallery: `basic_shapes` (the 284-colour parity gate) and `batch_check` (the deterministic render
gate). Their captures are archived in `/home/psp/evidence/sprint030/`.
