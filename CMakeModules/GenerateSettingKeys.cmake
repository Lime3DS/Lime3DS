## This file should be the *only place* where setting keys exist as strings.
# All references to setting strings should be derived from the
# `setting_keys.h` and `jni_setting_keys.cpp` files generated here.

# !!! Changes made here should be mirrored to SettingKeys.kt if used on Android

# Shared setting keys (multi-platform)
foreach(KEY IN ITEMS
    "use_artic_base_controller"
    "enable_gamemode"
    "use_cpu_jit"
    "cpu_clock_percentage"
    "is_new_3ds"
    "lle_applets"
    "deterministic_async_operations"
    "enable_required_online_lle_modules"
    "use_virtual_sd"
    "use_custom_storage"
    "compress_cia_installs"
    "async_fs_operations"
    "region_value"
    "init_clock"
    "init_time"
    "init_time_offset"
    "init_ticks_type"
    "init_ticks_override"
    "plugin_loader"
    "allow_plugin_loader"
    "steps_per_hour"
    "apply_region_free_patch"
    "graphics_api"
    "physical_device"
    "use_gles"
    "renderer_debug"
    "pica_debugging"
    "dump_command_buffers"
    "spirv_shader_gen"
    "disable_spirv_optimizer"
    "async_shader_compilation"
    "async_presentation"
    "use_hw_shader"
    "use_disk_shader_cache"
    "shaders_accurate_mul"
    "use_vsync"
    "use_skip_duplicate_frames"
    "use_display_refresh_rate_detection"
    "use_shader_jit"
    "resolution_factor"
    "frame_limit"
    "turbo_limit"
    "texture_filter"
    "texture_sampling"
    "delay_game_render_thread_us"
    "simulate_3ds_gpu_timings"
    "layout_option"
    "swap_screen"
    "upright_screen"
    "secondary_display_layout"
    "large_screen_proportion"
    "screen_gap"
    "small_screen_position"
    "custom_top_x"
    "custom_top_y"
    "custom_top_width"
    "custom_top_height"
    "custom_bottom_x"
    "custom_bottom_y"
    "custom_bottom_width"
    "custom_bottom_height"
    "custom_second_layer_opacity"
    "aspect_ratio"
    "screen_top_stretch"
    "screen_top_leftright_padding"
    "screen_top_topbottom_padding"
    "screen_bottom_stretch"
    "screen_bottom_leftright_padding"
    "screen_bottom_topbottom_padding"
    "portrait_layout_option"
    "custom_portrait_top_x"
    "custom_portrait_top_y"
    "custom_portrait_top_width"
    "custom_portrait_top_height"
    "custom_portrait_bottom_x"
    "custom_portrait_bottom_y"
    "custom_portrait_bottom_width"
    "custom_portrait_bottom_height"
    "bg_red"
    "bg_green"
    "bg_blue"
    "render_3d"
    "factor_3d"
    "swap_eyes_3d"
    "render_3d_which_display"
    "mono_render_option"
    "cardboard_screen_size"
    "cardboard_x_shift"
    "cardboard_y_shift"
    "filter_mode"
    "pp_shader_name"
    "anaglyph_shader_name"
    "dump_textures"
    "custom_textures"
    "preload_textures"
    "async_custom_loading"
    "disable_right_eye_render"
    "audio_emulation"
    "enable_audio_stretching"
    "enable_realtime_audio"
    "volume"
    "output_type"
    "output_device"
    "input_type"
    "input_device"
    "simulate_headphones_plugged"
    "delay_start_for_lle_modules"
    "use_gdbstub"
    "gdbstub_port"
    "instant_debug_log"
    "enable_rpc_server"
    "log_filter"
    "log_regex_filter"
    "toggle_unique_data_console_type"
    "enable_exception_handler"
    "use_integer_scaling"
    "layouts_to_cycle"
    "camera_inner_flip"
    "camera_outer_left_flip"
    "camera_outer_right_flip"
    "camera_inner_name"
    "camera_inner_config"
    "camera_outer_left_name"
    "camera_outer_left_config"
    "camera_outer_right_name"
    "camera_outer_right_config"
    "video_encoder"
    "video_encoder_options"
    "video_bitrate"
    "audio_encoder"
    "audio_encoder_options"
    "audio_bitrate"
    "last_artic_base_addr"
    "motion_device"
    "touch_device"
    "udp_input_address"
    "udp_input_port"
    "udp_pad_index"
    "record_frame_times"
    "language" # FIXME: DUPLICATE KEY (libretro equivalent: language_value)
    "web_api_url"
    "network_token"
    "check_for_update_on_start"
    "update_check_channel"
)
    set(SETTING_KEY_LIST "${SETTING_KEY_LIST}\n\"${KEY}\",")
    set(SETTING_KEY_DEFINITIONS "${SETTING_KEY_DEFINITIONS}\nDEFINE_KEY(${KEY})")
    if (ANDROID)
        string(REPLACE "_" "_1" KEY_JNI_ESCAPED ${KEY})
        set(JNI_SETTING_KEY_DEFINITIONS "${JNI_SETTING_KEY_DEFINITIONS}
            JNI_DEFINE_KEY(${KEY}, ${KEY_JNI_ESCAPED})")
    endif()
endforeach()

# Qt exclusive setting keys
# Note: A lot of these are very generic because our Qt settings are currently put under groups:
#       E.g. UILayout\geometry
# TODO: We should probably get rid of these groups and use complete keys at some point. -OS
# FIXME: Some of these settings don't use the standard snake_case. When we can migrate, address that. -OS
if (ENABLE_QT)
    foreach(KEY IN ITEMS
        "nickname"
        "ip"
        "port"
        "room_nickname"
        "room_name"
        "room_port"
        "host_type"
        "max_player"
        "room_description"
        "multiplayer_filter_text"
        "multiplayer_filter_games_owned"
        "multiplayer_filter_hide_empty"
        "multiplayer_filter_hide_full"
        "username_ban_list"
        "username"
        "ip_ban_list"
        "romsPath"
        "symbolsPath"
        "movieRecordPath"
        "moviePlaybackPath"
        "videoDumpingPath"
        "gameListRootDir"
        "gameListDeepScan"
        "path"
        "deep_scan"
        "expanded"
        "recentFiles"
        "output_format"
        "format_options"
        "theme"
        "program_id"
        "geometry"
        "state"
        "geometryRenderWindow"
        "geometrySecondaryWindow"
        "gameListHeaderState"
        "microProfileDialogGeometry"
        "name"
        "bind"
        "profile"
        "use_touchpad"
        "controller_touch_device"
        "use_touch_from_button"
        "input_maptype"
        "controller_hotkey_maptype"
        "touch_from_button_map"
        "touch_from_button_maps" # Why are these two so similar? Basically typo bait
        "nand_directory"
        "sdmc_directory"
        "game_id"
        "KeySeq"
        "controller_keyseq"
        "gamedirs"
        "libvorbis"
        "Context"
        "favorites"
        "microProfileDialogVisible"
        "singleWindowMode"
        "fullscreen"
        "displayTitleBars"
        "showFilterBar"
        "showStatusBar"
        "show_advanced_frametime_info"
        "confirmClose"
        "saveStateWarning"
        "firstStart"
        "pauseWhenInBackground"
        "muteWhenInBackground"
        "hideInactiveMouse"
        "inserted_cartridge"
        "enable_discord_presence"
        "iconSize"
        "row1" # The keys for this and...
        "row2" # this suck ass.
        "hideNoIcon"
        "singleLineMode"
        "show_compat_column"
        "show_region_column"
        "show_type_column"
        "show_size_column"
        "show_play_time_column"
        "screenshot_resolution_factor"
        "screenshotPath"
        "calloutFlags"
        "showConsole"
    )
        set(SETTING_KEY_LIST "${SETTING_KEY_LIST}\n\"${KEY}\",")
        set(SETTING_KEY_DEFINITIONS "${SETTING_KEY_DEFINITIONS}\nDEFINE_KEY(${KEY})")
    endforeach()
endif()

# Android exclusive setting keys (standalone app only, not Android libretro)
if (ANDROID)
    foreach(KEY IN ITEMS
        "expand_to_cutout_area"
        "performance_overlay_enable"
        "performance_overlay_show_fps"
        "performance_overlay_show_frame_time"
        "performance_overlay_show_speed"
        "performance_overlay_show_app_ram_usage"
        "performance_overlay_show_available_ram"
        "performance_overlay_show_battery_temp"
        "performance_overlay_background"
        "use_frame_limit" # FIXME: DUPLICATE KEY (shared equivalent: frame_limit)
        "android_hide_images"
        "screen_orientation"
        "performance_overlay_position"
        "enable_secondary_display"
        "combo_button_buttons"
    )
        string(REPLACE "_" "_1" KEY_JNI_ESCAPED ${KEY})
        set(SETTING_KEY_LIST "${SETTING_KEY_LIST}\n\"${KEY}\",")
        set(SETTING_KEY_DEFINITIONS "${SETTING_KEY_DEFINITIONS}\nDEFINE_KEY(${KEY})")
        set(JNI_SETTING_KEY_DEFINITIONS "${JNI_SETTING_KEY_DEFINITIONS}
            JNI_DEFINE_KEY(${KEY}, ${KEY_JNI_ESCAPED})")
    endforeach()
endif()

# Libretro exclusive setting keys
if (ENABLE_LIBRETRO)
    foreach(KEY IN ITEMS
        "language_value"
        "swap_screen_mode"
        "use_libretro_save_path"
        "analog_function"
        "analog_deadzone"
        "enable_mouse_touchscreen"
        "enable_touch_touchscreen"
        "enable_touch_pointer_timeout"
        "enable_motion"
        "motion_sensitivity"
    )
        string(REPLACE "_" "_1" KEY_JNI_ESCAPED ${KEY})
        set(SETTING_KEY_LIST "${SETTING_KEY_LIST}\n\"${KEY}\",")
        set(SETTING_KEY_DEFINITIONS "${SETTING_KEY_DEFINITIONS}\nDEFINE_KEY(${KEY})")
    endforeach()
endif()

# Trim trailing comma and newline from SETTING_KEY_LIST
string(LENGTH "${SETTING_KEY_LIST}" SETTING_KEY_LIST_LENGTH)
math(EXPR SETTING_KEY_LIST_NEW_LENGTH "${SETTING_KEY_LIST_LENGTH} - 1")
string(SUBSTRING "${SETTING_KEY_LIST}" 0 ${SETTING_KEY_LIST_NEW_LENGTH} SETTING_KEY_LIST)

# Configure files
configure_file("common/setting_keys.h.in" "common/setting_keys.h" @ONLY)
if (ENABLE_QT)
    configure_file("citra_qt/setting_qkeys.h.in" "citra_qt/setting_qkeys.h" @ONLY)
endif()
if (ANDROID AND NOT ENABLE_LIBRETRO)
    configure_file("android/app/src/main/jni/jni_setting_keys.cpp.in" "android/app/src/main/jni/jni_setting_keys.cpp" @ONLY)
endif()
