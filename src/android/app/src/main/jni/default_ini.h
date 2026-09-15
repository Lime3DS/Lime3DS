// Copyright 2014-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/hana/string.hpp>
#include "common/setting_keys.h"

namespace DefaultINI {

// All of these setting keys are either not currently used by Android or are too niche to bother
// documenting (people can contribute documentation if they care), or some other explained reason
constexpr std::array android_config_omitted_keys = {
    Settings::Keys::enable_gamemode,
    Settings::Keys::use_custom_storage,
    Settings::Keys::init_time_offset,
    Settings::Keys::physical_device,
    Settings::Keys::use_gles, // Niche
    Settings::Keys::dump_command_buffers,
    Settings::Keys::use_display_refresh_rate_detection,
    Settings::Keys::screen_top_stretch,
    Settings::Keys::screen_top_leftright_padding,
    Settings::Keys::screen_top_topbottom_padding,
    Settings::Keys::screen_bottom_stretch,
    Settings::Keys::screen_bottom_leftright_padding,
    Settings::Keys::screen_bottom_topbottom_padding,
    Settings::Keys::mono_render_option,
    Settings::Keys::log_regex_filter, // Niche
    Settings::Keys::video_encoder,
    Settings::Keys::video_encoder_options,
    Settings::Keys::video_bitrate,
    Settings::Keys::audio_encoder,
    Settings::Keys::audio_encoder_options,
    Settings::Keys::audio_bitrate,
    Settings::Keys::last_artic_base_addr,     // On Android, this value is stored as a "preference"
    Settings::Keys::enable_exception_handler, // Does nothing as the error is ignored
    Settings::Keys::use_gdbstub,              // GDB functionality disabled by deafult on Android
    Settings::Keys::gdbstub_port,
};

// clang-format off

// Is this macro nasty? Yes.
// Does it make the code way more readable? Also yes.
#define DECLARE_KEY(KEY) +Settings::HKeys::KEY+BOOST_HANA_STRING(" =")+

static const char* android_config_default_file_content = (BOOST_HANA_STRING(R"(
[Controls]
# for motion input, the following devices are available:
#  - "motion_emu" (default) for emulating motion input from mouse input. Required parameters:
#      - "update_period": update period in milliseconds (default to 100)
#      - "sensitivity": the coefficient converting mouse movement to tilting angle (default to 0.01)
#      - "tilt_clamp": the max value of the tilt angle in degrees (default to 90)
#  - "cemuhookudp" reads motion input from a udp server that uses cemuhook's udp protocol
)") DECLARE_KEY(motion_device) BOOST_HANA_STRING(R"(

# for touch input, the following devices are available:
#  - "emu_window" (default) for emulating touch input from mouse input to the emulation window. No parameters required
#  - "cemuhookudp" reads touch input from a udp server that uses cemuhook's udp protocol
#      - "min_x", "min_y", "max_x", "max_y": defines the udp device's touch screen coordinate system
)") DECLARE_KEY(touch_device) BOOST_HANA_STRING(R"(

# Most desktop operating systems do not expose a way to poll the motion state of the controllers
# so as a way around it, cemuhook created a udp client/server protocol to broadcast the data directly
# from a controller device to the client program. Citra has a client that can connect and read
# from any cemuhook compatible motion program.

# IPv4 address of the udp input server (Default "127.0.0.1")
)") DECLARE_KEY(udp_input_address) BOOST_HANA_STRING(R"(

# Port of the udp input server. (Default 26760)
)") DECLARE_KEY(udp_input_port) BOOST_HANA_STRING(R"(

# The pad to request data on. Should be between 0 (Pad 1) and 3 (Pad 4). (Default 0)
)") DECLARE_KEY(udp_pad_index) BOOST_HANA_STRING(R"(

# Use Artic Controller when connected to Artic Base Server. (Default 0)
)") DECLARE_KEY(use_artic_base_controller) BOOST_HANA_STRING(R"(

# List of buttons which will be triggered by the combo button. (Default [] or empty)
)") DECLARE_KEY(combo_button_buttons) BOOST_HANA_STRING(R"(

[Core]
# Whether to use the Just-In-Time (JIT) compiler for CPU emulation
# 0: Interpreter (slow), 1 (default): JIT (fast)
)") DECLARE_KEY(use_cpu_jit) BOOST_HANA_STRING(R"(

# Change the Clock Frequency of the emulated 3DS CPU.
# Underclocking can increase the performance of the game at the risk of freezing.
# Overclocking may fix lag that happens on console, but also comes with the risk of freezing.
# Range is any positive integer (but we suspect 25 - 400 is a good idea) Default is 100
)") DECLARE_KEY(cpu_clock_percentage) BOOST_HANA_STRING(R"(

[Renderer]
# Whether to render using OpenGL
# 1: OpenGL ES, 2: Vulkan (default)
)") DECLARE_KEY(graphics_api) BOOST_HANA_STRING(R"(

# Whether to compile shaders on multiple worker threads (Vulkan only)
# 0: Off, 1: On (default)
)") DECLARE_KEY(async_shader_compilation) BOOST_HANA_STRING(R"(

# Whether to emit PICA fragment shader using SPIRV or GLSL (Vulkan only)
# 0: GLSL, 1: SPIR-V (default)
)") DECLARE_KEY(spirv_shader_gen) BOOST_HANA_STRING(R"(

# Whether to disable the SPIRV optimizer. Disabling it reduces stutter, but may slightly worsen performance
# 0: Enabled, 1: Disabled (default)
)") DECLARE_KEY(disable_spirv_optimizer) BOOST_HANA_STRING(R"(

# Whether to use hardware shaders to emulate 3DS shaders
# 0: Software, 1 (default): Hardware
)") DECLARE_KEY(use_hw_shader) BOOST_HANA_STRING(R"(

# Whether to use accurate multiplication in hardware shaders
# 0: Off (Default. Faster, but causes issues in some games) 1: On (Slower, but correct)
)") DECLARE_KEY(shaders_accurate_mul) BOOST_HANA_STRING(R"(

# Whether to use the Just-In-Time (JIT) compiler for shader emulation
# 0: Interpreter (slow), 1 (default): JIT (fast)
)") DECLARE_KEY(use_shader_jit) BOOST_HANA_STRING(R"(

# Overrides the sampling filter used by games. This can be useful in certain
# cases with poorly behaved games when upscaling.
# 0 (default): Game Controlled, 1: Nearest Neighbor, 2: Linear
)") DECLARE_KEY(texture_sampling) BOOST_HANA_STRING(R"(

# Forces VSync on the display thread. Can cause input delay, so only turn this on
# if you have screen tearing, which is unusual on Android
# 0 (default): Off, 1: On
)") DECLARE_KEY(use_vsync) BOOST_HANA_STRING(R"(

# Skips display of duplicated frames in 30 fps games
# 0: Off, 1 (default): On
)") DECLARE_KEY(use_skip_duplicate_frames) BOOST_HANA_STRING(R"(

# Reduce stuttering by storing and loading generated shaders to disk
# 0: Off, 1 (default. On)
)") DECLARE_KEY(use_disk_shader_cache) BOOST_HANA_STRING(R"(

# Resolution scale factor
# 0: Auto (scales resolution to window size), 1: Native 3DS screen resolution, Otherwise a scale
# factor for the 3DS resolution
)") DECLARE_KEY(resolution_factor) BOOST_HANA_STRING(R"(

# Use Integer Scaling when the layout allows
# 0: Off (default), 1: On
)") DECLARE_KEY(use_integer_scaling) BOOST_HANA_STRING(R"(

# Turns on the frame limiter, which will limit frames output to the target game speed
# 0: Off, 1: On (default)
)") DECLARE_KEY(use_frame_limit) BOOST_HANA_STRING(R"(

# Limits the speed of the game to run no faster than this value as a percentage of target speed
# 1 - 9999: Speed limit as a percentage of target game speed. 100 (default)
)") DECLARE_KEY(frame_limit) BOOST_HANA_STRING(R"(

# Alternative frame limit which can be triggered by the user
# 1 - 9999: Speed limit as a percentage of target game speed. 100 (default)
)") DECLARE_KEY(turbo_limit) BOOST_HANA_STRING(R"(

# The clear color for the renderer. What shows up on the sides of the bottom screen.
# Must be in range of 0.0-1.0. Defaults to 0.0 for all.
)") DECLARE_KEY(bg_red) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(bg_blue) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(bg_green) BOOST_HANA_STRING(R"(

# Opacity of second layer when using custom layout option (bottom screen unless swapped). Useful if positioning on top of the first layer.
)") DECLARE_KEY(custom_second_layer_opacity) BOOST_HANA_STRING(R"(

# Whether and how Stereoscopic 3D should be rendered
# 0: Off, 1: Half Width Side by Side, 2 (default): Full Width Side by Side, 3: Anaglyph, 4: Interlaced, 5: Reverse Interlaced, 6: Cardboard VR
# 0 is no longer supported in the interface, as using render_3d_which_display = 0 has the same effect, but supported here for backwards compatibility
)") DECLARE_KEY(render_3d) BOOST_HANA_STRING(R"(

# Change 3D Intensity
# 0 - 255: Intensity. 0 (default)
)") DECLARE_KEY(factor_3d) BOOST_HANA_STRING(R"(

# Swap Eyes in 3d
# true: Swap eyes, false (default): Do not swap eyes
)") DECLARE_KEY(swap_eyes_3d) BOOST_HANA_STRING(R"(

# Which Display to render 3d mode to
# 0 (default) - None. Equivalent to render_3d=0
# 1: Both, 2: Primary Only, 3: Secondary Only
)") DECLARE_KEY(render_3d_which_display) BOOST_HANA_STRING(R"(

# The name of the post processing shader to apply.
# Loaded from shaders if render_3d is off or side by side.
)") DECLARE_KEY(pp_shader_name) BOOST_HANA_STRING(R"(

# The name of the shader to apply when render_3d is anaglyph.
# Loaded from shaders/anaglyph
)") DECLARE_KEY(anaglyph_shader_name) BOOST_HANA_STRING(R"(

# Whether to enable linear filtering or not
# This is required for some shaders to work correctly
# 0: Nearest, 1 (default): Linear
)") DECLARE_KEY(filter_mode) BOOST_HANA_STRING(R"(

# Delays the game render thread by the specified amount of microseconds
# Set to 0 for no delay, only useful in dynamic-fps games to simulate GPU delay.
)") DECLARE_KEY(delay_game_render_thread_us) BOOST_HANA_STRING(R"(

# Delays GPU completion events based on measurements taken from real hardware
# 0: No delay, 1 (default): Enable delay
)") DECLARE_KEY(simulate_3ds_gpu_timings) BOOST_HANA_STRING(R"(

# Disables rendering the right eye image
# Greatly improves performance in some games, but can cause flickering in others.
# 0 : Enable right eye rendering, 1: Disable right eye rendering
)") DECLARE_KEY(disable_right_eye_render) BOOST_HANA_STRING(R"(

# Perform presentation on separate threads
# Improves performance when using Vulkan in most applications.
# Adds ~1 frame of input lag.
# 0: Enable async presentation, 1 (default): Disable async presentation
)") DECLARE_KEY(async_presentation) BOOST_HANA_STRING(R"(

# Which texture filter should be used
# 0 (default): NoFilter
# 1: Anime4K
# 2: Bicubic
# 3: ScaleForce
# 4: xBRZ
# 5: MMPX
)") DECLARE_KEY(texture_filter) BOOST_HANA_STRING(R"(

[Layout]
# Layout for the screen inside the render window, landscape mode
# 0: Original (screens vertically aligned)
# 1: Single Screen Only,
# 2: Large Screen (Default on android)
# 3: Side by Side
# 4: Hybrid
# 5: Custom Layout
)") DECLARE_KEY(layout_option) BOOST_HANA_STRING(R"(

# Whether or not the screens should be rotated upright
# 0 (default): Not rotated, 1: Rotated upright
)") DECLARE_KEY(upright_screen) BOOST_HANA_STRING(R"(

# Which aspect ratio should be used by the emulated 3DS screens
# 0: (default): Default
# 1: 16:9
# 2: 4:3
# 3: 21:9
# 4: 16:10
# 5: Stretch
)") DECLARE_KEY(aspect_ratio) BOOST_HANA_STRING(R"(

# Whether or not the performance overlay should be displayed
# 0 (default): Hidden, 1: Shown
)") DECLARE_KEY(performance_overlay_enable) BOOST_HANA_STRING(R"(

# Whether or not the emulation framerate should be displayed in the performance overlay
# 0: Hidden, 1 (default): Shown
)") DECLARE_KEY(performance_overlay_show_fps) BOOST_HANA_STRING(R"(

# Whether or not the emulation frame time should be displayed in the performance overlay
# 0 (default): Hidden, 1: Shown
)") DECLARE_KEY(performance_overlay_show_frame_time) BOOST_HANA_STRING(R"(

# Whether or not the emulation speed should be displayed as a percentage in the performance overlay
)") DECLARE_KEY(performance_overlay_show_speed) BOOST_HANA_STRING(R"(

# Whether or not Azahar's RAM usage should be displayed in the performance overlay
)") DECLARE_KEY(performance_overlay_show_app_ram_usage) BOOST_HANA_STRING(R"(

# Whether or not the amount of available RAM should be displayed in the performance overlay
)") DECLARE_KEY(performance_overlay_show_available_ram) BOOST_HANA_STRING(R"(

# Whether or not the battery's current temperature should be displayed in the performance overlay
)") DECLARE_KEY(performance_overlay_show_battery_temp) BOOST_HANA_STRING(R"(

# Whether or not the performance overlay should have a transparent background to aid visibility
)") DECLARE_KEY(performance_overlay_background) BOOST_HANA_STRING(R"(

[Storage]
# Whether to compress the installed CIA contents
# 0 (default): Do not compress, 1: Compress
)") DECLARE_KEY(compress_cia_installs) BOOST_HANA_STRING(R"(

# Whether to enable async filesystem operations
# 0: Disabled, 1 (default): Enabled
)") DECLARE_KEY(async_fs_operations) BOOST_HANA_STRING(R"(

# Position of the performance overlay
# 0: Top Left
# 1: Center Top
# 2: Top Right
# 3: Bottom Left
# 4: Center Bottom
# 5: Bottom Right
)") DECLARE_KEY(performance_overlay_position) BOOST_HANA_STRING(R"(

# Screen Gap - adds a gap between screens in all two-screen modes
# Measured in pixels relative to the 240px default height of the screens
# Scales with the larger screen (so 24 is 10% of the larger screen height)
# Default value is 0.0
)") DECLARE_KEY(screen_gap) BOOST_HANA_STRING(R"(

# Large Screen Proportion - Relative size of large:small in large screen mode
# Default value is 2.25
)") DECLARE_KEY(large_screen_proportion) BOOST_HANA_STRING(R"(

# Small Screen Position - where is the small screen relative to the large
# Default value is 0
# 0: Top Right    1: Middle Right    2: Bottom Right
# 3: Top Left     4: Middle left     5: Bottom Left
# 6: Above the large screen          7: Below the large screen
)") DECLARE_KEY(small_screen_position) BOOST_HANA_STRING(R"(

# Screen placement when using Custom layout option
# 0x, 0y is the top left corner of the render window.
# suggested aspect ratio for top screen is 5:3
# suggested aspect ratio for bottom screen is 4:3
)") DECLARE_KEY(custom_top_x) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(custom_top_y) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(custom_top_width) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(custom_top_height) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(custom_bottom_x) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(custom_bottom_y) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(custom_bottom_width) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(custom_bottom_height) BOOST_HANA_STRING(R"(

# Orientation option for the emulator
# 2 (default): Automatic
# 0: Landscape
# 8: Landscape (Flipped)
# 1: Portrait
# 9: Portrait (Flipped)
)") DECLARE_KEY(screen_orientation) BOOST_HANA_STRING(R"(

# Layout for the portrait mode
# 0 (default): Top and bottom screens at top, full width
# 1: Custom Layout
)") DECLARE_KEY(portrait_layout_option) BOOST_HANA_STRING(R"(

# Screen placement when using Portrait Custom layout option
# 0x, 0y is the top left corner of the render window.
)") DECLARE_KEY(custom_portrait_top_x) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(custom_portrait_top_y) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(custom_portrait_top_width) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(custom_portrait_top_height) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(custom_portrait_bottom_x) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(custom_portrait_bottom_y) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(custom_portrait_bottom_width) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(custom_portrait_bottom_height) BOOST_HANA_STRING(R"(

# Swaps the prominent screen with the other screen.
# For example, if Single Screen is chosen, setting this to 1 will display the bottom screen instead of the top screen.
# 0 (default): Top Screen is prominent, 1: Bottom Screen is prominent
)") DECLARE_KEY(swap_screen) BOOST_HANA_STRING(R"(

# Expands the display area to include the cutout (or notch) area
# 0 (default): Off, 1: On
)") DECLARE_KEY(expand_to_cutout_area) BOOST_HANA_STRING(R"(

# Allows Azahar to use externally connected displays
# 0: Off, 1: On (default)
)") DECLARE_KEY(enable_secondary_display) BOOST_HANA_STRING(R"(

# Secondary Display Layout
# What the game should do if a secondary display is connected physically or using
# Miracast / Chromecast screen mirroring
# 0 (default) - Use System Default (mirror)
# 1 - Show Top Screen Only
# 2 - Show Bottom Screen Only
# 3 - Show both screens side by side
)") DECLARE_KEY(secondary_display_layout) BOOST_HANA_STRING(R"(

# Screen placement settings when using Cardboard VR (render3d = 4)
# 30 - 100: Screen size as a percentage of the viewport. 85 (default)
)") DECLARE_KEY(cardboard_screen_size) BOOST_HANA_STRING(R"(
# -100 - 100: Screen X-Coordinate shift as a percentage of empty space. 0 (default)
)") DECLARE_KEY(cardboard_x_shift) BOOST_HANA_STRING(R"(
# -100 - 100: Screen Y-Coordinate shift as a percentage of empty space. 0 (default)
)") DECLARE_KEY(cardboard_y_shift) BOOST_HANA_STRING(R"(

# Which of the available layouts should be cycled by Cycle Layouts
# A list of any values from 0 to 5 inclusive (e.g. 0, 1, 2, 5)
# Default: 0, 1, 2, 3, 4, 5
# 0: Default,
# 1: Single Screen,
# 2: Large Screen,
# 3: Side by Side,
# 4: Hybrid Screens,
# 5: Custom Layout,
)") DECLARE_KEY(layouts_to_cycle) BOOST_HANA_STRING(R"(

[Utility]
# Dumps textures as PNG to dump/textures/[Title ID]/.
# 0 (default): Off, 1: On
)") DECLARE_KEY(dump_textures) BOOST_HANA_STRING(R"(

# Reads PNG files from load/textures/[Title ID]/ and replaces textures.
# 0 (default): Off, 1: On
)") DECLARE_KEY(custom_textures) BOOST_HANA_STRING(R"(

# Loads all custom textures into memory before booting.
# 0 (default): Off, 1: On
)") DECLARE_KEY(preload_textures) BOOST_HANA_STRING(R"(

# Loads custom textures asynchronously with background threads.
# 0: Off, 1 (default): On
)") DECLARE_KEY(async_custom_loading) BOOST_HANA_STRING(R"(

[Audio]

# Whether or not audio emulation should be enabled
# 0: Disabled, 1 (default): Enabled
)") DECLARE_KEY(audio_emulation) BOOST_HANA_STRING(R"(

# Whether or not to enable the audio-stretching post-processing effect
# This effect adjusts audio speed to match emulation speed and helps prevent audio stutter,
# at the cost of increasing audio latency.
# 0: No, 1 (default): Yes
)") DECLARE_KEY(enable_audio_stretching) BOOST_HANA_STRING(R"(

# Scales audio playback speed to account for drops in emulation framerate
# 0 (default): No, 1: Yes
)") DECLARE_KEY(enable_realtime_audio) BOOST_HANA_STRING(R"(

# Simulates whether headphones are plugged in to the emulated 3DS system
# 0 (default): No, 1: Yes
)") DECLARE_KEY(simulate_headphones_plugged) BOOST_HANA_STRING(R"(

# Output volume.
# 1.0 (default): 100%, 0.0; mute
)") DECLARE_KEY(volume) BOOST_HANA_STRING(R"(

# Which audio output type to use.
# 0 (default): Auto-select, 1: No audio output, 2: Cubeb (if available), 3: OpenAL (if available), 4: SDL2 (if available)
)") DECLARE_KEY(output_type) BOOST_HANA_STRING(R"(

# Which audio output device to use.
# auto (default): Auto-select
)") DECLARE_KEY(output_device) BOOST_HANA_STRING(R"(

# Which audio input type to use.
# 0 (default): Auto-select, 1: No audio input, 2: Static noise, 3: Cubeb (if available), 4: OpenAL (if available)
)") DECLARE_KEY(input_type) BOOST_HANA_STRING(R"(

# Which audio input device to use.
# auto (default): Auto-select
)") DECLARE_KEY(input_device) BOOST_HANA_STRING(R"(

[Data Storage]
# Whether to create a virtual SD card.
# 1 (default): Yes, 0: No
)") DECLARE_KEY(use_virtual_sd) BOOST_HANA_STRING(R"(

[System]
# The system model that Citra will try to emulate
# 0: Old 3DS (default), 1: New 3DS
)") DECLARE_KEY(is_new_3ds) BOOST_HANA_STRING(R"(

# Whether to use LLE system applets, if installed
# 0: No, 1 (default): Yes
)") DECLARE_KEY(lle_applets) BOOST_HANA_STRING(R"(

# Whether to enable LLE modules for online play
# 0 (default): No, 1: Yes
)") DECLARE_KEY(enable_required_online_lle_modules) BOOST_HANA_STRING(R"(

# The system region that Citra will use during emulation
# -1: Auto-select (default), 0: Japan, 1: USA, 2: Europe, 3: Australia, 4: China, 5: Korea, 6: Taiwan
)") DECLARE_KEY(region_value) BOOST_HANA_STRING(R"(

# The system language that Citra will use during emulation
# 0: Japanese, 1: English (default), 2: French, 3: German, 4: Italian, 5: Spanish,
# 6: Simplified Chinese, 7: Korean, 8: Dutch, 9: Portuguese, 10: Russian, 11: Traditional Chinese
)") DECLARE_KEY(language) BOOST_HANA_STRING(R"(

# The clock to use when citra starts
# 0: System clock (default), 1: fixed time
)") DECLARE_KEY(init_clock) BOOST_HANA_STRING(R"(

# Time used when init_clock is set to fixed_time in the format %Y-%m-%d %H:%M:%S
# set to fixed time. Default 2000-01-01 00:00:01
# Note: 3DS can only handle times later then Jan 1 2000
)") DECLARE_KEY(init_time) BOOST_HANA_STRING(R"(

# The system ticks count to use when citra starts
# 0: Random (default), 1: Fixed
)") DECLARE_KEY(init_ticks_type) BOOST_HANA_STRING(R"(

# Tick count to use when init_ticks_type is set to Fixed.
# Defaults to 0.
)") DECLARE_KEY(init_ticks_override) BOOST_HANA_STRING(R"(

# Number of steps per hour reported by the pedometer. Range from 0 to 65,535.
# Defaults to 0.
)") DECLARE_KEY(steps_per_hour) BOOST_HANA_STRING(R"(

# Plugin loader state, if enabled plugins will be loaded from the SD card.
# You can also set if homebrew apps are allowed to enable the plugin loader
)") DECLARE_KEY(plugin_loader) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(allow_plugin_loader) BOOST_HANA_STRING(R"(

# Apply region free patch to installed applications
# Patches the region of installed applications to be region free, so that they always appear on the home menu.
# 0: Disabled, 1 (default): Enabled
)") DECLARE_KEY(apply_region_free_patch) BOOST_HANA_STRING(R"(

[Camera]
# Which camera engine to use for the right outer camera
# blank: a dummy camera that always returns black image
# image: loads a still image from the storage. When the camera is started, you will be prompted
#        to select an image.
# ndk (Default): uses the device camera. You can specify the camera ID to use in the config field.
#                If you don't specify an ID, the default setting will be used. For outer cameras,
#                the back-facing camera will be used. For the inner camera, the front-facing
#                camera will be used. Please note that 'Legacy' cameras are not supported.
)") DECLARE_KEY(camera_outer_right_name) BOOST_HANA_STRING(R"(

# A config string for the right outer camera. Its meaning is defined by the camera engine
)") DECLARE_KEY(camera_outer_right_config) BOOST_HANA_STRING(R"(

# The image flip to apply
# 0: None (default), 1: Horizontal, 2: Vertical, 3: Reverse
)") DECLARE_KEY(camera_outer_right_flip) BOOST_HANA_STRING(R"(

# ... for the left outer camera
)") DECLARE_KEY(camera_outer_left_name) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(camera_outer_left_config) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(camera_outer_left_flip) BOOST_HANA_STRING(R"(

# ... for the inner camera
)") DECLARE_KEY(camera_inner_name) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(camera_inner_config) BOOST_HANA_STRING(R"(
)") DECLARE_KEY(camera_inner_flip) BOOST_HANA_STRING(R"(

[Miscellaneous]
# A filter which removes logs below a certain logging level.
# Examples: *:Debug Kernel.SVC:Trace Service.*:Critical
)") DECLARE_KEY(log_filter) BOOST_HANA_STRING(R"(

# Whether or not Azahar-related images should be hidden from the Android gallery
# 0 (default): No, 1: Yes
)") DECLARE_KEY(android_hide_images) BOOST_HANA_STRING(R"(

# Whether or not an in-app notification should be displayed when an update is available for Azahar
# 0: No, 1 (default): Yes
)") DECLARE_KEY(check_for_update_on_start) BOOST_HANA_STRING(R"(

# Which update channel should be used by the update checker
# 0 (default): Stable, 1: Prerelease
)") DECLARE_KEY(update_check_channel) BOOST_HANA_STRING(R"(

[Debugging]
# Record frame time data, can be found in the log directory. Boolean value
)") DECLARE_KEY(record_frame_times) BOOST_HANA_STRING(R"(

# Whether to enable additional debugging information during emulation
# 0 (default): Off, 1: On
)") DECLARE_KEY(renderer_debug) BOOST_HANA_STRING(R"(

# Whether to enable PICA200 debugging (does nothing on Android)
# 0 (default): Off, 1: On
)") DECLARE_KEY(pica_debugging) BOOST_HANA_STRING(R"(

# Flush log output on every message
# Immediately commits the debug log to file. Use this if Azahar crashes and the log output is being cut.
)") DECLARE_KEY(instant_debug_log) BOOST_HANA_STRING(R"(

# Enable RPC server for scripting purposes. Allows accessing guest memory remotely.
# 0 (default): Off, 1: On
)") DECLARE_KEY(enable_rpc_server) BOOST_HANA_STRING(R"(

# Enables toggling the unique data console type (Old 3DS <-> New 3DS) to be able to download the opposite system firmware type from system settings.
# 0 (default): Off, 1: On
)") DECLARE_KEY(toggle_unique_data_console_type) BOOST_HANA_STRING(R"(

# Delay the start of apps when LLE modules are enabled
# 0: Off, 1 (default): On
)") DECLARE_KEY(delay_start_for_lle_modules) BOOST_HANA_STRING(R"(

# Force deterministic async operations
# Only needed for debugging, makes performance worse if enabled
# 0: Off (default), 1: On
)") DECLARE_KEY(deterministic_async_operations) BOOST_HANA_STRING(R"(

# To LLE a service module add "LLE\<module name>=true"

[WebService]
# URL for Web API
)") DECLARE_KEY(web_api_url) BOOST_HANA_STRING(R"(
# Token for Web Service
)") DECLARE_KEY(network_token) BOOST_HANA_STRING("\n")

).c_str();

// clang-format on

} // namespace DefaultINI
