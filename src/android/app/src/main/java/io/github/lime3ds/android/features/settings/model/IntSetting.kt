// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.model

enum class IntSetting(
    override val key: String,
    override val section: String,
    override val defaultValue: Int
) : AbstractIntSetting {
    FRAME_LIMIT("frame_limit", Settings.SECTION_RENDERER, 100),
    EMULATED_REGION("region_value", Settings.SECTION_SYSTEM, -1),
    INIT_CLOCK("init_clock", Settings.SECTION_SYSTEM, 0),
    CAMERA_INNER_FLIP("camera_inner_flip", Settings.SECTION_CAMERA, 0),
    CAMERA_OUTER_LEFT_FLIP("camera_outer_left_flip", Settings.SECTION_CAMERA, 0),
    CAMERA_OUTER_RIGHT_FLIP("camera_outer_right_flip", Settings.SECTION_CAMERA, 0),
    GRAPHICS_API("graphics_api", Settings.SECTION_RENDERER, 1),
    RESOLUTION_FACTOR("resolution_factor", Settings.SECTION_RENDERER, 1),
    STEREOSCOPIC_3D_MODE("render_3d", Settings.SECTION_RENDERER, 0),
    STEREOSCOPIC_3D_DEPTH("factor_3d", Settings.SECTION_RENDERER, 0),
    CARDBOARD_SCREEN_SIZE("cardboard_screen_size", Settings.SECTION_LAYOUT, 85),
    CARDBOARD_X_SHIFT("cardboard_x_shift", Settings.SECTION_LAYOUT, 0),
    CARDBOARD_Y_SHIFT("cardboard_y_shift", Settings.SECTION_LAYOUT, 0),
    SCREEN_LAYOUT("layout_option", Settings.SECTION_LAYOUT, 0),
    AUDIO_INPUT_TYPE("output_type", Settings.SECTION_AUDIO, 0),
    NEW_3DS("is_new_3ds", Settings.SECTION_SYSTEM, 1),
    LLE_APPLETS("lle_applets", Settings.SECTION_SYSTEM, 0),
    CPU_CLOCK_SPEED("cpu_clock_percentage", Settings.SECTION_CORE, 100),
    LINEAR_FILTERING("filter_mode", Settings.SECTION_RENDERER, 1),
    SHADERS_ACCURATE_MUL("shaders_accurate_mul", Settings.SECTION_RENDERER, 0),
    DISK_SHADER_CACHE("use_disk_shader_cache", Settings.SECTION_RENDERER, 1),
    DUMP_TEXTURES("dump_textures", Settings.SECTION_UTILITY, 0),
    CUSTOM_TEXTURES("custom_textures", Settings.SECTION_UTILITY, 0),
    ASYNC_CUSTOM_LOADING("async_custom_loading", Settings.SECTION_UTILITY, 1),
    PRELOAD_TEXTURES("preload_textures", Settings.SECTION_UTILITY, 0),
    ENABLE_AUDIO_STRETCHING("enable_audio_stretching", Settings.SECTION_AUDIO, 1),
    ENABLE_REALTIME_AUDIO("enable_realtime_audio", Settings.SECTION_AUDIO, 0),
    CPU_JIT("use_cpu_jit", Settings.SECTION_CORE, 1),
    HW_SHADER("use_hw_shader", Settings.SECTION_RENDERER, 1),
    VSYNC("use_vsync_new", Settings.SECTION_RENDERER, 1),
    DEBUG_RENDERER("renderer_debug", Settings.SECTION_DEBUG, 0),
    TEXTURE_FILTER("texture_filter", Settings.SECTION_RENDERER, 0),
    USE_FRAME_LIMIT("use_frame_limit", Settings.SECTION_RENDERER, 1);

    override var int: Int = defaultValue

    override val valueAsString: String
        get() = int.toString()

    override val isRuntimeEditable: Boolean
        get() {
            for (setting in NOT_RUNTIME_EDITABLE) {
                if (setting == this) {
                    return false
                }
            }
            return true
        }

    companion object {
        private val NOT_RUNTIME_EDITABLE = listOf(
            EMULATED_REGION,
            INIT_CLOCK,
            NEW_3DS,
            LLE_APPLETS,
            GRAPHICS_API,
            VSYNC,
            DEBUG_RENDERER,
            CPU_JIT,
            ASYNC_CUSTOM_LOADING,
            AUDIO_INPUT_TYPE
        )

        fun from(key: String): IntSetting? = IntSetting.values().firstOrNull { it.key == key }

        fun clear() = IntSetting.values().forEach { it.int = it.defaultValue }
    }
}
