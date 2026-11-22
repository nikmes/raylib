#ifndef PSP_SAMPLES_H
#define PSP_SAMPLES_H

typedef struct Sample { const char *category; const char *name; void (*run)(void); } Sample;

void sample_core_2d_camera(void);
void sample_core_2d_camera_mouse_zoom(void);
void sample_core_2d_camera_platformer(void);
void sample_core_3d_camera_first_person(void);
void sample_core_3d_camera_fps(void);
void sample_core_3d_camera_free(void);
void sample_core_3d_camera_mode(void);
void sample_core_3d_picking(void);
void sample_core_basic_screen_manager(void);
void sample_core_basic_window(void);
void sample_core_compute_hash(void);
void sample_core_custom_frame_control(void);
void sample_core_custom_logging(void);
void sample_core_delta_time(void);
void sample_core_directory_files(void);
void sample_core_input_actions(void);
void sample_core_input_gamepad(void);
void sample_core_random_sequence(void);
void sample_core_random_values(void);
void sample_core_scissor_test(void);
void sample_core_screen_recording(void);
void sample_core_storage_values(void);
void sample_core_text_file_loading(void);
void sample_core_world_screen(void);
void sample_shapes_ball_physics(void);
void sample_shapes_basic_shapes(void);
void sample_shapes_bouncing_ball(void);
void sample_shapes_circle_sector_drawing(void);
void sample_shapes_clock_of_clocks(void);
void sample_shapes_colors_palette(void);
void sample_shapes_dashed_line(void);
void sample_shapes_digital_clock(void);
void sample_shapes_drag_puzzle(void);
void sample_shapes_easings_ball(void);
void sample_shapes_easings_box(void);
void sample_shapes_easings_rectangles(void);
void sample_shapes_easings_testbed(void);
void sample_shapes_ellipse_collision(void);
void sample_shapes_following_eyes(void);
void sample_shapes_hilbert_curve(void);
void sample_shapes_lines_bezier(void);
void sample_shapes_logo_raylib(void);
void sample_shapes_math_angle_rotation(void);
void sample_shapes_math_sine_cosine(void);
void sample_shapes_outlines_thickness(void);
void sample_shapes_rectangle_advanced(void);
void sample_textures_background_scrolling(void);
void sample_textures_blend_modes(void);
void sample_textures_bunnymark(void);
void sample_textures_cellular_automata(void);
void sample_textures_gif_player(void);
void sample_textures_image_kernel(void);
void sample_textures_image_loading(void);
void sample_textures_image_processing(void);
void sample_textures_image_rotate(void);
void sample_textures_image_text(void);
void sample_textures_logo_raylib(void);
void sample_textures_npatch_drawing(void);
void sample_textures_particles_blending(void);
void sample_textures_polygon_drawing(void);
void sample_textures_raw_data(void);
void sample_textures_screen_buffer(void);
void sample_textures_sprite_animation(void);
void sample_textures_srcrec_dstrec(void);
void sample_textures_textured_curve(void);
void sample_textures_tiled_drawing(void);
void sample_audio_amp_envelope(void);
void sample_audio_mixed_processor(void);
void sample_audio_module_playing(void);
void sample_audio_music_stream(void);
void sample_audio_raw_stream(void);
void sample_audio_sound_loading(void);
void sample_audio_sound_multi(void);
void sample_audio_stream_callback(void);
void sample_audio_stream_effects(void);

static Sample samples[] = {
    { "core", "2d_camera", sample_core_2d_camera },
    { "core", "2d_camera_mouse_zoom", sample_core_2d_camera_mouse_zoom },
    { "core", "2d_camera_platformer", sample_core_2d_camera_platformer },
    { "core", "3d_camera_first_person", sample_core_3d_camera_first_person },
    { "core", "3d_camera_fps", sample_core_3d_camera_fps },
    { "core", "3d_camera_free", sample_core_3d_camera_free },
    { "core", "3d_camera_mode", sample_core_3d_camera_mode },
    { "core", "3d_picking", sample_core_3d_picking },
    { "core", "basic_screen_manager", sample_core_basic_screen_manager },
    { "core", "basic_window", sample_core_basic_window },
    { "core", "compute_hash", sample_core_compute_hash },
    { "core", "custom_frame_control", sample_core_custom_frame_control },
    { "core", "custom_logging", sample_core_custom_logging },
    { "core", "delta_time", sample_core_delta_time },
    { "core", "directory_files", sample_core_directory_files },
    { "core", "input_actions", sample_core_input_actions },
    { "core", "input_gamepad", sample_core_input_gamepad },
    { "core", "random_sequence", sample_core_random_sequence },
    { "core", "random_values", sample_core_random_values },
    { "core", "scissor_test", sample_core_scissor_test },
    { "core", "screen_recording", sample_core_screen_recording },
    { "core", "storage_values", sample_core_storage_values },
    { "core", "text_file_loading", sample_core_text_file_loading },
    { "core", "world_screen", sample_core_world_screen },
    { "shapes", "ball_physics", sample_shapes_ball_physics },
    { "shapes", "basic_shapes", sample_shapes_basic_shapes },
    { "shapes", "bouncing_ball", sample_shapes_bouncing_ball },
    { "shapes", "circle_sector_drawing", sample_shapes_circle_sector_drawing },
    { "shapes", "clock_of_clocks", sample_shapes_clock_of_clocks },
    { "shapes", "colors_palette", sample_shapes_colors_palette },
    { "shapes", "dashed_line", sample_shapes_dashed_line },
    { "shapes", "digital_clock", sample_shapes_digital_clock },
    { "shapes", "drag_puzzle", sample_shapes_drag_puzzle },
    { "shapes", "easings_ball", sample_shapes_easings_ball },
    { "shapes", "easings_box", sample_shapes_easings_box },
    { "shapes", "easings_rectangles", sample_shapes_easings_rectangles },
    { "shapes", "easings_testbed", sample_shapes_easings_testbed },
    { "shapes", "ellipse_collision", sample_shapes_ellipse_collision },
    { "shapes", "following_eyes", sample_shapes_following_eyes },
    { "shapes", "hilbert_curve", sample_shapes_hilbert_curve },
    { "shapes", "lines_bezier", sample_shapes_lines_bezier },
    { "shapes", "logo_raylib", sample_shapes_logo_raylib },
    { "shapes", "math_angle_rotation", sample_shapes_math_angle_rotation },
    { "shapes", "math_sine_cosine", sample_shapes_math_sine_cosine },
    { "shapes", "outlines_thickness", sample_shapes_outlines_thickness },
    { "shapes", "rectangle_advanced", sample_shapes_rectangle_advanced },
    { "textures", "background_scrolling", sample_textures_background_scrolling },
    { "textures", "blend_modes", sample_textures_blend_modes },
    { "textures", "bunnymark", sample_textures_bunnymark },
    { "textures", "cellular_automata", sample_textures_cellular_automata },
    { "textures", "gif_player", sample_textures_gif_player },
    { "textures", "image_kernel", sample_textures_image_kernel },
    { "textures", "image_loading", sample_textures_image_loading },
    { "textures", "image_processing", sample_textures_image_processing },
    { "textures", "image_rotate", sample_textures_image_rotate },
    { "textures", "image_text", sample_textures_image_text },
    { "textures", "logo_raylib", sample_textures_logo_raylib },
    { "textures", "npatch_drawing", sample_textures_npatch_drawing },
    { "textures", "particles_blending", sample_textures_particles_blending },
    { "textures", "polygon_drawing", sample_textures_polygon_drawing },
    { "textures", "raw_data", sample_textures_raw_data },
    { "textures", "screen_buffer", sample_textures_screen_buffer },
    { "textures", "sprite_animation", sample_textures_sprite_animation },
    { "textures", "srcrec_dstrec", sample_textures_srcrec_dstrec },
    { "textures", "textured_curve", sample_textures_textured_curve },
    { "textures", "tiled_drawing", sample_textures_tiled_drawing },
    { "audio", "amp_envelope", sample_audio_amp_envelope },
    { "audio", "mixed_processor", sample_audio_mixed_processor },
    { "audio", "module_playing", sample_audio_module_playing },
    { "audio", "music_stream", sample_audio_music_stream },
    { "audio", "raw_stream", sample_audio_raw_stream },
    { "audio", "sound_loading", sample_audio_sound_loading },
    { "audio", "sound_multi", sample_audio_sound_multi },
    { "audio", "stream_callback", sample_audio_stream_callback },
    { "audio", "stream_effects", sample_audio_stream_effects },
};

static const char *categories[] = { "core", "shapes", "textures", "audio" };
#define CATEGORY_COUNT ((int)(sizeof(categories)/sizeof(categories[0])))
#define SAMPLE_COUNT ((int)(sizeof(samples)/sizeof(samples[0])))

#endif
