// Copyright 2014-2026 Citra Emulator Project / Azahar Emulator Project
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
#include "common/setting_keys.h"
#include "common/string_util.h"
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
enum class LayoutOption : u32 { // Shouldn't these have set numbers to prevent last two from
                                // shifting? -OS
    Default,
    SingleScreen,
    LargeScreen,
    SideScreen,
#ifndef ANDROID
    SeparateWindows,
#endif
    HybridScreen,
    CustomLayout,
};

enum class InputMappingType : u8 { AllControllers, Guid, GuidPort };

/** Defines the layout option for mobile portrait */
enum class PortraitLayoutOption : u32 {
    // formerly mobile portrait
    PortraitTopFullWidth,
    PortraitCustomLayout,
    PortraitOriginal
};

enum class SecondaryDisplayLayout : u32 {
    None,
    TopScreenOnly,
    BottomScreenOnly,
    SideBySide,
    OppositeScreenOnly,
    Original,
    Hybrid,
    LargeScreen
};
/** Defines where the small screen will appear relative to the large screen
 * when in Large Screen mode
 */
enum class SmallScreenPosition : u32 {
    TopRight,
    MiddleRight,
    BottomRight,
    TopLeft,
    MiddleLeft,
    BottomLeft,
    AboveLarge,
    BelowLarge
};

enum class StereoRenderOption : u32 {
    Off = 0,
    SideBySide = 1,
    SideBySideFull = 2,
    Anaglyph = 3,
    Interlaced = 4,
    ReverseInterlaced = 5,
    CardboardVR = 6
};

// Which eye to render when 3d is off. 800px wide mode could be added here in the future, when
// implemented
enum class MonoRenderOption : u32 {
    LeftEye = 0,
    RightEye = 1,
};

// on android, which displays to render stereo mode to
enum class StereoWhichDisplay : u32 {
    None = 0, // equivalent to StereoRenderOption = Off
    Both = 1,
    PrimaryOnly = 2,
    SecondaryOnly = 3
};

enum class AudioEmulation : u32 {
    HLE = 0,
    LLE = 1,
    LLEMultithreaded = 2,
};

enum class TextureFilter : u32 {
    NoFilter = 0,
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

enum class AspectRatio : u32 {
    Default = 0,
    R16_9 = 1,
    R4_3 = 2,
    R21_9 = 3,
    R16_10 = 4,
    Stretch = 5,
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
    explicit Setting(const Type& default_val, const std::string_view& name)
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
                     const std::string_view& name)
        requires(ranged)
        : value{default_val}, default_value{default_val}, maximum{max_val}, minimum{min_val},
          label{std::string(name)} {}

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
    explicit SwitchableSetting(const Type& default_val, const std::string_view& name)
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
                               const std::string_view& name)
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
    std::string controller_touch_device;
    bool use_touchpad;
    bool use_touch_from_button;
    int touch_from_button_map_index;
    std::string udp_input_address;
    u16 udp_input_port;
    u8 udp_pad_index;
    InputMappingType maptype = Settings::InputMappingType::GuidPort;
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
    Setting<bool> use_artic_base_controller{false, Keys::use_artic_base_controller};

    SwitchableSetting<bool> enable_gamemode{true, Keys::enable_gamemode};

    // Core
    Setting<bool> use_cpu_jit{true, Keys::use_cpu_jit};
    Setting<bool> use_fastinterp{true, Keys::use_fastinterp};
    SwitchableSetting<s32, true> cpu_clock_percentage{100, 5, 400, Keys::cpu_clock_percentage};
    SwitchableSetting<bool> is_new_3ds{true, Keys::is_new_3ds};
    SwitchableSetting<bool> lle_applets{true, Keys::lle_applets};
    SwitchableSetting<bool> deterministic_async_operations{false,
                                                           Keys::deterministic_async_operations};
    SwitchableSetting<bool> enable_required_online_lle_modules{
        false, Keys::enable_required_online_lle_modules};

    // Data Storage
    Setting<bool> use_virtual_sd{true, Keys::use_virtual_sd};
    Setting<bool> use_custom_storage{false, Keys::use_custom_storage};
    Setting<bool> compress_cia_installs{false, Keys::compress_cia_installs};
    Setting<bool> async_fs_operations{true, Keys::async_fs_operations};

    // System
    SwitchableSetting<s32> region_value{REGION_VALUE_AUTO_SELECT, Keys::region_value};
    Setting<InitClock> init_clock{InitClock::SystemTime, Keys::init_clock};
    Setting<u64> init_time{946681277ULL, Keys::init_time};
    Setting<s64> init_time_offset{0, Keys::init_time_offset};
    Setting<InitTicks> init_ticks_type{InitTicks::Random, Keys::init_ticks_type};
    Setting<s64> init_ticks_override{0, Keys::init_ticks_override};
    Setting<bool> plugin_loader_enabled{false, Keys::plugin_loader};
    Setting<bool> allow_plugin_loader{true, Keys::allow_plugin_loader};
    Setting<u16> steps_per_hour{0, Keys::steps_per_hour};
    Setting<bool> apply_region_free_patch{true, Keys::apply_region_free_patch};

    // Renderer
    // clang-format off
    SwitchableSetting<GraphicsAPI, true> graphics_api{
#if defined(ANDROID) && defined(ENABLE_VULKAN) // Prefer Vulkan on Android, OpenGL on everything else
        GraphicsAPI::Vulkan,
#elif defined(ENABLE_OPENGL)
        GraphicsAPI::OpenGL,
#elif defined(ENABLE_VULKAN)
        GraphicsAPI::Vulkan,
#elif defined(ENABLE_SOFTWARE_RENDERER)
        GraphicsAPI::Software,
#else
// TODO: Add a null renderer backend for this, perhaps.
#error "At least one renderer must be enabled."
#endif
        GraphicsAPI::Software, GraphicsAPI::Vulkan, Keys::graphics_api};
    // clang-format on
    SwitchableSetting<u32> physical_device{0, Keys::physical_device};
    Setting<bool> use_gles{false, Keys::use_gles};
    Setting<bool> renderer_debug{false, Keys::renderer_debug};
    Setting<bool> pica_debugging{false, Keys::pica_debugging};
    Setting<bool> dump_command_buffers{false, Keys::dump_command_buffers};
    SwitchableSetting<bool> spirv_shader_gen{true, Keys::spirv_shader_gen};
    SwitchableSetting<bool> disable_spirv_optimizer{true, Keys::disable_spirv_optimizer};
    SwitchableSetting<bool> async_shader_compilation{false, Keys::async_shader_compilation};
    SwitchableSetting<bool> async_presentation{true, Keys::async_presentation};
    SwitchableSetting<bool> use_hw_shader{true, Keys::use_hw_shader};
    SwitchableSetting<bool> use_disk_shader_cache{true, Keys::use_disk_shader_cache};
    SwitchableSetting<bool> use_skip_duplicate_frames{false, Keys::use_skip_duplicate_frames};
    SwitchableSetting<bool> shaders_accurate_mul{true, Keys::shaders_accurate_mul};
#ifdef ANDROID // TODO: Fuck this -OS
    SwitchableSetting<bool> use_vsync{false, Keys::use_vsync};
#else
    SwitchableSetting<bool> use_vsync{true, Keys::use_vsync};
#endif
    SwitchableSetting<bool> use_display_refresh_rate_detection{
        true, Keys::use_display_refresh_rate_detection};
    Setting<bool> use_shader_jit{true, Keys::use_shader_jit};
    SwitchableSetting<u32, true> resolution_factor{1, 0, 18, Keys::resolution_factor};
    SwitchableSetting<bool> use_integer_scaling{false, Keys::use_integer_scaling};
    SwitchableSetting<double, true> frame_limit{100, 0, 1000, Keys::frame_limit};
    SwitchableSetting<double, true> turbo_limit{200, 0, 1000, Keys::turbo_limit};
    SwitchableSetting<TextureFilter> texture_filter{TextureFilter::NoFilter, Keys::texture_filter};
    SwitchableSetting<TextureSampling> texture_sampling{TextureSampling::GameControlled,
                                                        Keys::texture_sampling};
    SwitchableSetting<u16, true> delay_game_render_thread_us{0, 0, 65000,
                                                             Keys::delay_game_render_thread_us};
    SwitchableSetting<bool> simulate_3ds_gpu_timings{false, Keys::simulate_3ds_gpu_timings};

    SwitchableSetting<LayoutOption> layout_option{LayoutOption::Default, Keys::layout_option};
    SwitchableSetting<bool> swap_screen{false, Keys::swap_screen};
    SwitchableSetting<bool> upright_screen{false, Keys::upright_screen};
    SwitchableSetting<SecondaryDisplayLayout> secondary_display_layout{
        SecondaryDisplayLayout::OppositeScreenOnly, Keys::secondary_display_layout};
    SwitchableSetting<std::vector<LayoutOption>> layouts_to_cycle{
        {LayoutOption::Default, LayoutOption::SingleScreen, LayoutOption::LargeScreen,
         LayoutOption::SideScreen,
#ifndef ANDROID
         LayoutOption::SeparateWindows,
#endif
         LayoutOption::HybridScreen, LayoutOption::CustomLayout},
        Keys::layouts_to_cycle};
    SwitchableSetting<float, true> large_screen_proportion{4.f, 1.f, 16.f,
                                                           Keys::large_screen_proportion};
    SwitchableSetting<int> screen_gap{0, Keys::screen_gap};
    SwitchableSetting<SmallScreenPosition> small_screen_position{SmallScreenPosition::BottomRight,
                                                                 Keys::small_screen_position};
    Setting<u16> custom_top_x{0, Keys::custom_top_x};
    Setting<u16> custom_top_y{0, Keys::custom_top_y};
    Setting<u16> custom_top_width{800, Keys::custom_top_width};
    Setting<u16> custom_top_height{480, Keys::custom_top_height};
    Setting<u16> custom_bottom_x{80, Keys::custom_bottom_x};
    Setting<u16> custom_bottom_y{500, Keys::custom_bottom_y};
    Setting<u16> custom_bottom_width{640, Keys::custom_bottom_width};
    Setting<u16> custom_bottom_height{480, Keys::custom_bottom_height};
    Setting<u16> custom_second_layer_opacity{100, Keys::custom_second_layer_opacity};
    SwitchableSetting<AspectRatio> aspect_ratio{AspectRatio::Default, Keys::aspect_ratio};
    SwitchableSetting<bool> screen_top_stretch{false, Keys::screen_top_stretch};
    Setting<u16> screen_top_leftright_padding{0, Keys::screen_top_leftright_padding};
    Setting<u16> screen_top_topbottom_padding{0, Keys::screen_top_topbottom_padding};
    SwitchableSetting<bool> screen_bottom_stretch{false, Keys::screen_bottom_stretch};
    Setting<u16> screen_bottom_leftright_padding{0, Keys::screen_bottom_leftright_padding};
    Setting<u16> screen_bottom_topbottom_padding{0, Keys::screen_bottom_topbottom_padding};

    SwitchableSetting<PortraitLayoutOption> portrait_layout_option{
        PortraitLayoutOption::PortraitTopFullWidth, Keys::portrait_layout_option};
    Setting<u16> custom_portrait_top_x{0, Keys::custom_portrait_top_x};
    Setting<u16> custom_portrait_top_y{0, Keys::custom_portrait_top_y};
    Setting<u16> custom_portrait_top_width{800, Keys::custom_portrait_top_width};
    Setting<u16> custom_portrait_top_height{480, Keys::custom_portrait_top_height};
    Setting<u16> custom_portrait_bottom_x{80, Keys::custom_portrait_bottom_x};
    Setting<u16> custom_portrait_bottom_y{500, Keys::custom_portrait_bottom_y};
    Setting<u16> custom_portrait_bottom_width{640, Keys::custom_portrait_bottom_width};
    Setting<u16> custom_portrait_bottom_height{480, Keys::custom_portrait_bottom_height};

    SwitchableSetting<float> bg_red{0.f, Keys::bg_red};
    SwitchableSetting<float> bg_green{0.f, Keys::bg_green};
    SwitchableSetting<float> bg_blue{0.f, Keys::bg_blue};

    SwitchableSetting<StereoRenderOption> render_3d{StereoRenderOption::Off, Keys::render_3d};
    SwitchableSetting<u32> factor_3d{0, Keys::factor_3d};
    SwitchableSetting<bool> swap_eyes_3d{false, Keys::swap_eyes_3d};

    SwitchableSetting<StereoWhichDisplay> render_3d_which_display{StereoWhichDisplay::None,
                                                                  Keys::render_3d_which_display};
    SwitchableSetting<MonoRenderOption> mono_render_option{MonoRenderOption::LeftEye,
                                                           Keys::mono_render_option};

    Setting<u32> cardboard_screen_size{85, Keys::cardboard_screen_size};
    Setting<s32> cardboard_x_shift{0, Keys::cardboard_x_shift};
    Setting<s32> cardboard_y_shift{0, Keys::cardboard_y_shift};

    SwitchableSetting<bool> filter_mode{true, Keys::filter_mode};
    SwitchableSetting<std::string> pp_shader_name{"None (builtin)", Keys::pp_shader_name};
    SwitchableSetting<std::string> anaglyph_shader_name{"Dubois (builtin)",
                                                        Keys::anaglyph_shader_name};

    SwitchableSetting<bool> dump_textures{false, Keys::dump_textures};
    SwitchableSetting<bool> custom_textures{false, Keys::custom_textures};
    SwitchableSetting<bool> preload_textures{false, Keys::preload_textures};
    SwitchableSetting<bool> async_custom_loading{true, Keys::async_custom_loading};
    SwitchableSetting<bool> disable_right_eye_render{false, Keys::disable_right_eye_render};

    // Audio
    bool audio_muted;
    SwitchableSetting<AudioEmulation> audio_emulation{AudioEmulation::HLE, Keys::audio_emulation};
    SwitchableSetting<bool> enable_audio_stretching{true, Keys::enable_audio_stretching};
    SwitchableSetting<bool> enable_realtime_audio{false, Keys::enable_realtime_audio};
    SwitchableSetting<float, true> volume{1.f, 0.f, 1.f, Keys::volume};
    Setting<AudioCore::SinkType> output_type{AudioCore::SinkType::Auto, Keys::output_type};
    Setting<std::string> output_device{"Auto", Keys::output_device};
    Setting<AudioCore::InputType> input_type{AudioCore::InputType::Auto, Keys::input_type};
    Setting<std::string> input_device{"Auto", Keys::input_device};
    SwitchableSetting<bool> simulate_headphones_plugged{false, Keys::simulate_headphones_plugged};

    // Camera
    std::array<std::string, Service::CAM::NumCameras> camera_name;
    std::array<std::string, Service::CAM::NumCameras> camera_config;
    std::array<int, Service::CAM::NumCameras> camera_flip;

    // Debugging
    bool record_frame_times;
    std::unordered_map<std::string, bool> lle_modules;
    Setting<bool> delay_start_for_lle_modules{true, Keys::delay_start_for_lle_modules};
    Setting<bool> use_gdbstub{false, Keys::use_gdbstub};
    Setting<u16> gdbstub_port{24689, Keys::gdbstub_port};
    Setting<bool> instant_debug_log{false, Keys::instant_debug_log};
    Setting<bool> enable_rpc_server{false, Keys::enable_rpc_server};
    Setting<bool> toggle_unique_data_console_type{false, Keys::toggle_unique_data_console_type};
    Setting<bool> enable_exception_handler{false, Keys::enable_exception_handler};

    // WebService
    Setting<std::string> web_api_url{"", Keys::web_api_url};
    Setting<std::string> network_token{"", Keys::network_token};

    // Miscellaneous
    Setting<std::string> log_filter{"*:Info", Keys::log_filter};
    Setting<std::string> log_regex_filter{"", Keys::log_regex_filter};

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

/// Gets the graphics API that should be used; not necessarily one set in settings
Settings::GraphicsAPI GetWorkingGraphicsAPI();

// Input profiles
void LoadProfile(int index);
void SaveProfile(int index);
void CreateProfile(std::string name);
void DeleteProfile(int index);
void RenameCurrentProfile(std::string new_name);

extern bool is_temporary_frame_limit;
extern double temporary_frame_limit;
static inline void ResetTemporaryFrameLimit() {
    is_temporary_frame_limit = false;
    temporary_frame_limit = 0;
}
static inline double GetFrameLimit() {
    return is_temporary_frame_limit ? temporary_frame_limit : values.frame_limit.GetValue();
}

} // namespace Settings
