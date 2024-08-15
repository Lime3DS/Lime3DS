// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>
#include "audio_core/input_details.h"
#include "audio_core/sink_details.h"
#include "common/common_types.h"
#include "core/hle/service/cam/cam_params.h"

namespace Settings {

enum class GraphicsAPI {
    Software = 0,
    OpenGL = 1,
    Vulkan = 2,
};

enum class InitClock : u32 {
    SystemTime = 0,
    FixedTime = 1,
};

enum class InitTicks : u32 {
    Random = 0,
    Fixed = 1,
};

/** Defines the layout option for desktop and mobile landscape */
enum class LayoutOption : u32 {
    Default,
    SingleScreen,
    LargeScreen,
    SideScreen,
#ifndef ANDROID
    SeparateWindows,
#endif
    HybridScreen,
    CustomLayout,

    // Similiar to LargeScreen, but better for mobile devices in landscape mode. The screens are
    // clamped to the top of the frame, and the bottom screen is a bit bigger.
    MobileLandscape,
};

/** Defines the layout option for mobile portrait */
enum class PortraitLayoutOption : u32 {
    // formerly mobile portrait
    PortraitTopFullWidth,
    PortraitCustomLayout,
};

enum class StereoRenderOption : u32 {
    Off = 0,
    SideBySide = 1,
    Anaglyph = 2,
    Interlaced = 3,
    ReverseInterlaced = 4,
    CardboardVR = 5
};

// Which eye to render when 3d is off. 800px wide mode could be added here in the future, when
// implemented
enum class MonoRenderOption : u32 {
    LeftEye = 0,
    RightEye = 1,
};

enum class AudioEmulation : u32 {
    HLE = 0,
    LLE = 1,
    LLEMultithreaded = 2,
};

enum class TextureFilter : u32 {
    None = 0,
    Anime4K = 1,
    Bicubic = 2,
    ScaleForce = 3,
    xBRZ = 4,
    MMPX = 5,
};

enum class TextureSampling : u32 {
    GameControlled = 0,
    NearestNeighbor = 1,
    Linear = 2,
};

namespace NativeButton {

enum Values {
    A,
    B,
    X,
    Y,
    Up,
    Down,
    Left,
    Right,
    L,
    R,
    Start,
    Select,
    Debug,
    Gpio14,

    ZL,
    ZR,

    Home,
    Power,

    NumButtons,
};

constexpr int BUTTON_HID_BEGIN = A;
constexpr int BUTTON_IR_BEGIN = ZL;
constexpr int BUTTON_NS_BEGIN = Power;

constexpr int BUTTON_HID_END = BUTTON_IR_BEGIN;
constexpr int BUTTON_IR_END = BUTTON_NS_BEGIN;
constexpr int BUTTON_NS_END = NumButtons;

constexpr int NUM_BUTTONS_HID = BUTTON_HID_END - BUTTON_HID_BEGIN;
constexpr int NUM_BUTTONS_IR = BUTTON_IR_END - BUTTON_IR_BEGIN;
constexpr int NUM_BUTTONS_NS = BUTTON_NS_END - BUTTON_NS_BEGIN;

static const std::array<const char*, NumButtons> mapping = {{
    "button_a",
    "button_b",
    "button_x",
    "button_y",
    "button_up",
    "button_down",
    "button_left",
    "button_right",
    "button_l",
    "button_r",
    "button_start",
    "button_select",
    "button_debug",
    "button_gpio14",
    "button_zl",
    "button_zr",
    "button_home",
    "button_power",
}};

} // namespace NativeButton

namespace NativeAnalog {
enum Values {
    CirclePad,
    CStick,
    NumAnalogs,
};

constexpr std::array<const char*, NumAnalogs> mapping = {{
    "circle_pad",
    "c_stick",
}};
} // namespace NativeAnalog

/** The Setting class is a simple resource manager. It defines a label and default value alongside
 * the actual value of the setting for simpler and less-error prone use with frontend
 * configurations. Specifying a default value and label is required. A minimum and maximum range can
 * be specified for sanitization.
 */
template <typename Type, bool ranged = false>
class Setting {
protected:
    Setting() = default;

    /**
     * Only sets the setting to the given initializer, leaving the other members to their default
     * initializers.
     *
     * @param global_val Initial value of the setting
     */
    explicit Setting(const Type& val) : value{val} {}

public:
    /**
     * Sets a default value, label, and setting value.
     *
     * @param default_val Intial value of the setting, and default value of the setting
     * @param name Label for the setting
     */
    explicit Setting(const Type& default_val, const std::string& name)
        requires(!ranged)
        : value{default_val}, default_value{default_val}, label{name} {}
    virtual ~Setting() = default;

    /**
     * Sets a default value, minimum value, maximum value, and label.
     *
     * @param default_val Intial value of the setting, and default value of the setting
     * @param min_val Sets the minimum allowed value of the setting
     * @param max_val Sets the maximum allowed value of the setting
     * @param name Label for the setting
     */
    explicit Setting(const Type& default_val, const Type& min_val, const Type& max_val,
                     const std::string& name)
        requires(ranged)
        : value{default_val}, default_value{default_val}, maximum{max_val}, minimum{min_val},
          label{name} {}

    /**
     *  Returns a reference to the setting's value.
     *
     * @returns A reference to the setting
     */
    [[nodiscard]] virtual const Type& GetValue() const {
        return value;
    }

    /**
     * Sets the setting to the given value.
     *
     * @param val The desired value
     */
    virtual void SetValue(const Type& val) {
        Type temp{ranged ? std::clamp(val, minimum, maximum) : val};
        std::swap(value, temp);
    }

    /**
     * Returns the value that this setting was created with.
     *
     * @returns A reference to the default value
     */
    [[nodiscard]] const Type& GetDefault() const {
        return default_value;
    }

    /**
     * Returns the label this setting was created with.
     *
     * @returns A reference to the label
     */
    [[nodiscard]] const std::string& GetLabel() const {
        return label;
    }

    /**
     * Assigns a value to the setting.
     *
     * @param val The desired setting value
     *
     * @returns A reference to the setting
     */
    virtual const Type& operator=(const Type& val) {
        Type temp{ranged ? std::clamp(val, minimum, maximum) : val};
        std::swap(value, temp);
        return value;
    }

    /**
     * Returns a reference to the setting.
     *
     * @returns A reference to the setting
     */
    explicit virtual operator const Type&() const {
        return value;
    }

protected:
    Type value{};               ///< The setting
    const Type default_value{}; ///< The default value
    const Type maximum{};       ///< Maximum allowed value of the setting
    const Type minimum{};       ///< Minimum allowed value of the setting
    const std::string label{};  ///< The setting's label
};

/**
 * The SwitchableSetting class is a slightly more complex version of the Setting class. This adds a
 * custom setting to switch to when a guest application specifically requires it. The effect is that
 * other components of the emulator can access the setting's intended value without any need for the
 * component to ask whether the custom or global setting is needed at the moment.
 *
 * By default, the global setting is used.
 */
template <typename Type, bool ranged = false>
class SwitchableSetting : virtual public Setting<Type, ranged> {
public:
    /**
     * Sets a default value, label, and setting value.
     *
     * @param default_val Intial value of the setting, and default value of the setting
     * @param name Label for the setting
     */
    explicit SwitchableSetting(const Type& default_val, const std::string& name)
        requires(!ranged)
        : Setting<Type>{default_val, name} {}
    virtual ~SwitchableSetting() = default;

    /**
     * Sets a default value, minimum value, maximum value, and label.
     *
     * @param default_val Intial value of the setting, and default value of the setting
     * @param min_val Sets the minimum allowed value of the setting
     * @param max_val Sets the maximum allowed value of the setting
     * @param name Label for the setting
     */
    explicit SwitchableSetting(const Type& default_val, const Type& min_val, const Type& max_val,
                               const std::string& name)
        requires(ranged)
        : Setting<Type, true>{default_val, min_val, max_val, name} {}

    /**
     * Tells this setting to represent either the global or custom setting when other member
     * functions are used.
     *
     * @param to_global Whether to use the global or custom setting.
     */
    void SetGlobal(bool to_global) {
        use_global = to_global;
    }

    /**
     * Returns whether this setting is using the global setting or not.
     *
     * @returns The global state
     */
    [[nodiscard]] bool UsingGlobal() const {
        return use_global;
    }

    /**
     * Returns either the global or custom setting depending on the values of this setting's global
     * state or if the global value was specifically requested.
     *
     * @param need_global Request global value regardless of setting's state; defaults to false
     *
     * @returns The required value of the setting
     */
    [[nodiscard]] virtual const Type& GetValue() const override {
        if (use_global) {
            return this->value;
        }
        return custom;
    }
    [[nodiscard]] virtual const Type& GetValue(bool need_global) const {
        if (use_global || need_global) {
            return this->value;
        }
        return custom;
    }

    /**
     * Sets the current setting value depending on the global state.
     *
     * @param val The new value
     */
    void SetValue(const Type& val) override {
        Type temp{ranged ? std::clamp(val, this->minimum, this->maximum) : val};
        if (use_global) {
            std::swap(this->value, temp);
        } else {
            std::swap(custom, temp);
        }
    }

    /**
     * Assigns the current setting value depending on the global state.
     *
     * @param val The new value
     *
     * @returns A reference to the current setting value
     */
    const Type& operator=(const Type& val) override {
        Type temp{ranged ? std::clamp(val, this->minimum, this->maximum) : val};
        if (use_global) {
            std::swap(this->value, temp);
            return this->value;
        }
        std::swap(custom, temp);
        return custom;
    }

    /**
     * Returns the current setting value depending on the global state.
     *
     * @returns A reference to the current setting value
     */
    virtual explicit operator const Type&() const override {
        if (use_global) {
            return this->value;
        }
        return custom;
    }

protected:
    bool use_global{true}; ///< The setting's global state
    Type custom{};         ///< The custom value of the setting
};

struct InputProfile {
    std::string name;
    std::array<std::string, NativeButton::NumButtons> buttons;
    std::array<std::string, NativeAnalog::NumAnalogs> analogs;
    std::string motion_device;
    std::string touch_device;
    bool use_touch_from_button;
    int touch_from_button_map_index;
    std::string udp_input_address;
    u16 udp_input_port;
    u8 udp_pad_index;
};

struct TouchFromButtonMap {
    std::string name;
    std::vector<std::string> buttons;
};

/// A special region value indicating that citra will automatically select a region
/// value to fit the region lockout info of the game
static constexpr s32 REGION_VALUE_AUTO_SELECT = -1;

struct Values {
    // Controls
    InputProfile current_input_profile;       ///< The current input profile
    int current_input_profile_index;          ///< The current input profile index
    std::vector<InputProfile> input_profiles; ///< The list of input profiles
    std::vector<TouchFromButtonMap> touch_from_button_maps;
    Setting<bool> use_artic_base_controller{false, "use_artic_base_controller"};

    SwitchableSetting<bool> enable_gamemode{true, "enable_gamemode"};

    // Core
    Setting<bool> use_cpu_jit{true, "use_cpu_jit"};
    SwitchableSetting<s32, true> cpu_clock_percentage{100, 5, 400, "cpu_clock_percentage"};
    SwitchableSetting<bool> is_new_3ds{true, "is_new_3ds"};
    SwitchableSetting<bool> lle_applets{false, "lle_applets"};

    // Data Storage
    Setting<bool> use_virtual_sd{true, "use_virtual_sd"};
    Setting<bool> use_custom_storage{false, "use_custom_storage"};

    // System
    SwitchableSetting<s32> region_value{REGION_VALUE_AUTO_SELECT, "region_value"};
    Setting<InitClock> init_clock{InitClock::SystemTime, "init_clock"};
    Setting<u64> init_time{946681277ULL, "init_time"};
    Setting<s64> init_time_offset{0, "init_time_offset"};
    Setting<InitTicks> init_ticks_type{InitTicks::Random, "init_ticks_type"};
    Setting<s64> init_ticks_override{0, "init_ticks_override"};
    Setting<bool> plugin_loader_enabled{false, "plugin_loader"};
    Setting<bool> allow_plugin_loader{true, "allow_plugin_loader"};

    // Renderer
    SwitchableSetting<GraphicsAPI, true> graphics_api{
#if defined(ENABLE_OPENGL)
        GraphicsAPI::OpenGL,
#elif defined(ENABLE_VULKAN)
        GraphicsAPI::Vulkan,
#elif defined(ENABLE_SOFTWARE_RENDERER)
        GraphicsAPI::Software,
#else
// TODO: Add a null renderer backend for this, perhaps.
#error "At least one renderer must be enabled."
#endif
        GraphicsAPI::Software, GraphicsAPI::Vulkan, "graphics_api"};
    SwitchableSetting<u32> physical_device{0, "physical_device"};
    Setting<bool> use_gles{false, "use_gles"};
    Setting<bool> renderer_debug{false, "renderer_debug"};
    Setting<bool> dump_command_buffers{false, "dump_command_buffers"};
    SwitchableSetting<bool> spirv_shader_gen{true, "spirv_shader_gen"};
    SwitchableSetting<bool> async_shader_compilation{false, "async_shader_compilation"};
    SwitchableSetting<bool> async_presentation{true, "async_presentation"};
    SwitchableSetting<bool> use_hw_shader{true, "use_hw_shader"};
    SwitchableSetting<bool> use_disk_shader_cache{true, "use_disk_shader_cache"};
    SwitchableSetting<bool> shaders_accurate_mul{true, "shaders_accurate_mul"};
    SwitchableSetting<bool> use_vsync_new{true, "use_vsync_new"};
    Setting<bool> use_shader_jit{true, "use_shader_jit"};
    SwitchableSetting<u32, true> resolution_factor{1, 0, 10, "resolution_factor"};
    SwitchableSetting<u16, true> frame_limit{100, 0, 1000, "frame_limit"};
    SwitchableSetting<TextureFilter> texture_filter{TextureFilter::None, "texture_filter"};
    SwitchableSetting<TextureSampling> texture_sampling{TextureSampling::GameControlled,
                                                        "texture_sampling"};
    SwitchableSetting<LayoutOption> layout_option{LayoutOption::Default, "layout_option"};
    SwitchableSetting<bool> swap_screen{false, "swap_screen"};
    SwitchableSetting<bool> upright_screen{false, "upright_screen"};
    SwitchableSetting<float, true> large_screen_proportion{4.f, 1.f, 16.f,
                                                           "large_screen_proportion"};
    Setting<u16> custom_top_x{0, "custom_top_x"};
    Setting<u16> custom_top_y{0, "custom_top_y"};
    Setting<u16> custom_top_width{800, "custom_top_width"};
    Setting<u16> custom_top_height{480, "custom_top_height"};
    Setting<u16> custom_bottom_x{80, "custom_bottom_x"};
    Setting<u16> custom_bottom_y{500, "custom_bottom_y"};
    Setting<u16> custom_bottom_width{640, "custom_bottom_width"};
    Setting<u16> custom_bottom_height{480, "custom_bottom_height"};
    Setting<u16> custom_second_layer_opacity{100, "custom_second_layer_opacity"};

    SwitchableSetting<bool> screen_top_stretch{false, "screen_top_stretch"};
    Setting<u16> screen_top_leftright_padding{0, "screen_top_leftright_padding"};
    Setting<u16> screen_top_topbottom_padding{0, "screen_top_topbottom_padding"};
    SwitchableSetting<bool> screen_bottom_stretch{false, "screen_bottom_stretch"};
    Setting<u16> screen_bottom_leftright_padding{0, "screen_bottom_leftright_padding"};
    Setting<u16> screen_bottom_topbottom_padding{0, "screen_bottom_topbottom_padding"};

    SwitchableSetting<PortraitLayoutOption> portrait_layout_option{
        PortraitLayoutOption::PortraitTopFullWidth, "portrait_layout_option"};
    Setting<u16> custom_portrait_top_x{0, "custom_portrait_top_x"};
    Setting<u16> custom_portrait_top_y{0, "custom_portrait_top_y"};
    Setting<u16> custom_portrait_top_width{800, "custom_portrait_top_width"};
    Setting<u16> custom_portrait_top_height{480, "custom_portrait_top_height"};
    Setting<u16> custom_portrait_bottom_x{80, "custom_portrait_bottom_x"};
    Setting<u16> custom_portrait_bottom_y{500, "custom_portrait_bottom_y"};
    Setting<u16> custom_portrait_bottom_width{640, "custom_portrait_bottom_width"};
    Setting<u16> custom_portrait_bottom_height{480, "custom_portrait_bottom_height"};

    SwitchableSetting<float> bg_red{0.f, "bg_red"};
    SwitchableSetting<float> bg_green{0.f, "bg_green"};
    SwitchableSetting<float> bg_blue{0.f, "bg_blue"};

    SwitchableSetting<StereoRenderOption> render_3d{StereoRenderOption::Off, "render_3d"};
    SwitchableSetting<u32> factor_3d{0, "factor_3d"};
    SwitchableSetting<MonoRenderOption> mono_render_option{MonoRenderOption::LeftEye,
                                                           "mono_render_option"};

    Setting<u32> cardboard_screen_size{85, "cardboard_screen_size"};
    Setting<s32> cardboard_x_shift{0, "cardboard_x_shift"};
    Setting<s32> cardboard_y_shift{0, "cardboard_y_shift"};

    SwitchableSetting<bool> filter_mode{true, "filter_mode"};
    SwitchableSetting<std::string> pp_shader_name{"none (builtin)", "pp_shader_name"};
    SwitchableSetting<std::string> anaglyph_shader_name{"dubois (builtin)", "anaglyph_shader_name"};

    SwitchableSetting<bool> dump_textures{false, "dump_textures"};
    SwitchableSetting<bool> custom_textures{false, "custom_textures"};
    SwitchableSetting<bool> preload_textures{false, "preload_textures"};
    SwitchableSetting<bool> async_custom_loading{true, "async_custom_loading"};

    // Audio
    bool audio_muted;
    SwitchableSetting<AudioEmulation> audio_emulation{AudioEmulation::HLE, "audio_emulation"};
    SwitchableSetting<bool> enable_audio_stretching{true, "enable_audio_stretching"};
    SwitchableSetting<bool> enable_realtime_audio{false, "enable_realtime_audio"};
    SwitchableSetting<float, true> volume{1.f, 0.f, 1.f, "volume"};
    Setting<AudioCore::SinkType> output_type{AudioCore::SinkType::Auto, "output_type"};
    Setting<std::string> output_device{"auto", "output_device"};
    Setting<AudioCore::InputType> input_type{AudioCore::InputType::Auto, "input_type"};
    Setting<std::string> input_device{"auto", "input_device"};

    // Camera
    std::array<std::string, Service::CAM::NumCameras> camera_name;
    std::array<std::string, Service::CAM::NumCameras> camera_config;
    std::array<int, Service::CAM::NumCameras> camera_flip;

    // Debugging
    bool record_frame_times;
    std::unordered_map<std::string, bool> lle_modules;
    Setting<bool> delay_start_for_lle_modules{true, "delay_start_for_lle_modules"};
    Setting<bool> use_gdbstub{false, "use_gdbstub"};
    Setting<u16> gdbstub_port{24689, "gdbstub_port"};

    // Miscellaneous
    Setting<std::string> log_filter{"*:Info", "log_filter"};

    // Video Dumping
    std::string output_format;
    std::string format_options;

    std::string video_encoder;
    std::string video_encoder_options;
    u64 video_bitrate;

    std::string audio_encoder;
    std::string audio_encoder_options;
    u64 audio_bitrate;
};

extern Values values;

bool IsConfiguringGlobal();
void SetConfiguringGlobal(bool is_global);

float Volume();

void LogSettings();

// Restore the global state of all applicable settings in the Values struct
void RestoreGlobalState(bool is_powered_on);

// Input profiles
void LoadProfile(int index);
void SaveProfile(int index);
void CreateProfile(std::string name);
void DeleteProfile(int index);
void RenameCurrentProfile(std::string new_name);

} // namespace Settings
