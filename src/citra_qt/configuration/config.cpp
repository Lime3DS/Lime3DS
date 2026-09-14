// Copyright 2014-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <QKeySequence>
#include <QSettings>
#include <QVariant>
#include <QVector>
#include "citra_qt/configuration/config.h"
#include "citra_qt/setting_qkeys.h"
#include "common/file_util.h"
#include "common/settings.h"
#include "core/hle/service/service.h"
#include "input_common/main.h"
#include "input_common/udp/client.h"
#include "network/network.h"

QtConfig::QtConfig(const std::string& config_name, ConfigType config_type) : type{config_type} {
    global = config_type == ConfigType::GlobalConfig;
    Initialize(config_name);
}

QtConfig::~QtConfig() {
    if (global) {
        Save();
    }
}

const std::array<int, Settings::NativeButton::NumButtons> QtConfig::default_buttons = {
    Qt::Key_A, Qt::Key_S, Qt::Key_Z, Qt::Key_X, Qt::Key_T, Qt::Key_G,
    Qt::Key_F, Qt::Key_H, Qt::Key_Q, Qt::Key_W, Qt::Key_M, Qt::Key_N,
    Qt::Key_O, Qt::Key_P, Qt::Key_1, Qt::Key_2, Qt::Key_B, Qt::Key_V,
};

const std::array<std::array<int, 5>, Settings::NativeAnalog::NumAnalogs> QtConfig::default_analogs{{
    {
        Qt::Key_Up,
        Qt::Key_Down,
        Qt::Key_Left,
        Qt::Key_Right,
        Qt::Key_D,
    },
    {
        Qt::Key_I,
        Qt::Key_K,
        Qt::Key_J,
        Qt::Key_L,
        Qt::Key_D,
    },
}};

// This shouldn't have anything except static initializers (no functions). So
// QKeySequence(...).toString() is NOT ALLOWED HERE.
// This must be in alphabetical order according to action name as it must have the same order as
// UISetting::values.shortcuts, which is alphabetically ordered.
// clang-format off
const std::vector<UISettings::Shortcut> QtConfig::default_hotkeys{ {
     {QStringLiteral("Advance Frame"),            QStringLiteral("Main Window"), {QStringLiteral(""),       QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Audio Mute/Unmute"),        QStringLiteral("Main Window"), {QStringLiteral("Ctrl+M"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Audio Volume Down"),        QStringLiteral("Main Window"), {QStringLiteral(""),       QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Audio Volume Up"),          QStringLiteral("Main Window"), {QStringLiteral(""),       QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Capture Screenshot"),       QStringLiteral("Main Window"), {QStringLiteral("Ctrl+P"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Continue/Pause Emulation"), QStringLiteral("Main Window"), {QStringLiteral("F4"),     QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Debug Pause"),              QStringLiteral("Main Window"), {QStringLiteral(""),       QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Debug Resume"),             QStringLiteral("Main Window"), {QStringLiteral(""),       QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Debug Step"),               QStringLiteral("Main Window"), {QStringLiteral(""),       QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Debug Unschedule All"),     QStringLiteral("Main Window"), {QStringLiteral(""),       QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Debug Schedule All"),       QStringLiteral("Main Window"), {QStringLiteral(""),       QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Decrease 3D Factor"),       QStringLiteral("Main Window"), {QStringLiteral("Ctrl+-"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Decrease Speed Limit"),     QStringLiteral("Main Window"), {QStringLiteral("-"),      QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Exit Azahar"),              QStringLiteral("Main Window"), {QStringLiteral("Ctrl+Q"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Exit Fullscreen"),          QStringLiteral("Main Window"), {QStringLiteral("Esc"),    QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Fullscreen"),               QStringLiteral("Main Window"), {QStringLiteral("F11"),    QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Increase 3D Factor"),       QStringLiteral("Main Window"), {QStringLiteral("Ctrl++"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Increase Speed Limit"),     QStringLiteral("Main Window"), {QStringLiteral("+"),      QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Load Amiibo"),              QStringLiteral("Main Window"), {QStringLiteral("F2"),     QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Load File"),                QStringLiteral("Main Window"), {QStringLiteral("Ctrl+O"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Load from Newest Non-Quicksave Slot"),  QStringLiteral("Main Window"), {QStringLiteral("Ctrl+V"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Multiplayer Browse Public Rooms"),      QStringLiteral("Main Window"), {QStringLiteral("Ctrl+B"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Multiplayer Create Room"),              QStringLiteral("Main Window"), {QStringLiteral("Ctrl+N"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Multiplayer Direct Connect to Room"),   QStringLiteral("Main Window"), {QStringLiteral("Ctrl+Shift"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Multiplayer Leave Room"),               QStringLiteral("Main Window"), {QStringLiteral("Ctrl+L"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Multiplayer Show Current Room"),        QStringLiteral("Main Window"), {QStringLiteral("Ctrl+R"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Quick Save"),               QStringLiteral("Main Window"), {QStringLiteral(""),       QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Quick Load"),               QStringLiteral("Main Window"), {QStringLiteral(""),       QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Remove Amiibo"),            QStringLiteral("Main Window"), {QStringLiteral("F3"),     QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Restart Emulation"),        QStringLiteral("Main Window"), {QStringLiteral("F6"),     QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Rotate Screens Upright"),   QStringLiteral("Main Window"), {QStringLiteral("F8"),     QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Save to Oldest Non-Quicksave Slot"),  QStringLiteral("Main Window"), {QStringLiteral("Ctrl+C"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Stop Emulation"),           QStringLiteral("Main Window"), {QStringLiteral("F5"),     QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Swap Screens"),             QStringLiteral("Main Window"), {QStringLiteral("F9"),     QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Toggle 3D"),                QStringLiteral("Main Window"), {QStringLiteral("Ctrl+3"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Toggle Custom Textures"),   QStringLiteral("Main Window"), {QStringLiteral("F7"),     QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Toggle Filter Bar"),        QStringLiteral("Main Window"), {QStringLiteral("Ctrl+F"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Toggle Frame Advancing"),   QStringLiteral("Main Window"), {QStringLiteral("Ctrl+A"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Toggle Per-Application Speed"),  QStringLiteral("Main Window"), {QStringLiteral("Ctrl+Z"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Toggle Screen Layout"),     QStringLiteral("Main Window"), {QStringLiteral("F10"),    QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Toggle Status Bar"),        QStringLiteral("Main Window"), {QStringLiteral("Ctrl+S"), QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Toggle Texture Dumping"),   QStringLiteral("Main Window"), {QStringLiteral(""),       QStringLiteral(""), Qt::ApplicationShortcut}},
     {QStringLiteral("Toggle Turbo Mode"),        QStringLiteral("Main Window"), {QStringLiteral(""),      QStringLiteral(""), Qt::ApplicationShortcut}},
    } };
// clang-format on

void QtConfig::Initialize(const std::string& config_name) {
    const std::string fs_config_loc = FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir);
    const std::string config_file = fmt::format("{}.ini", config_name);

    switch (type) {
    case ConfigType::GlobalConfig:
        qt_config_loc = fmt::format("{}/{}", fs_config_loc, config_file);
        break;
    case ConfigType::PerGameConfig:
        qt_config_loc = fmt::format("{}/custom/{}", fs_config_loc, config_file);
        break;
    }

    FileUtil::CreateFullPath(qt_config_loc);
    qt_config =
        std::make_unique<QSettings>(QString::fromStdString(qt_config_loc), QSettings::IniFormat);
    Reload();
}

/* {Read,Write}BasicSetting and WriteGlobalSetting templates must be defined here before their
 * usages later in this file. This allows explicit definition of some types that don't work
 * nicely with the general version.
 */

// Explicit std::string definition: Qt can't implicitly convert a std::string to a QVariant, nor
// can it implicitly convert a QVariant back to a {std::,Q}string
template <>
void QtConfig::ReadBasicSetting(Settings::Setting<std::string>& setting) {
    const QString name = QString::fromStdString(setting.GetLabel());
    const auto default_value = QString::fromStdString(setting.GetDefault());
    if (qt_config->value(name + QStringLiteral("/default"), false).toBool()) {
        setting.SetValue(default_value.toStdString());
    } else {
        setting.SetValue(qt_config->value(name, default_value).toString().toStdString());
    }
}
// definition for vectors of enums
template <typename Type, bool ranged>
void QtConfig::ReadBasicSetting(Settings::Setting<std::vector<Type>, ranged>& setting) {
    const QString name = QString::fromStdString(setting.GetLabel());
    const std::vector<Type> default_value = setting.GetDefault();
    QStringList stringList = qt_config->value(name).toStringList();

    if (qt_config->value(name + QStringLiteral("/default"), false).toBool() ||
        stringList.size() < 1) {
        setting.SetValue(default_value);
    } else {
        if (stringList.size() < 1) {
            setting.SetValue(default_value);
        } else {
            std::vector<Type> newValue;
            for (const QString& str : stringList) {
                if constexpr (std::is_enum_v<Type>) {
                    using TypeU = std::underlying_type_t<Type>;
                    newValue.push_back(static_cast<Type>(str.toInt()));
                } else if constexpr (std::is_integral_v<Type>) {
                    newValue.push_back(str.toInt());
                } else {
                    newValue.push_back(str.toStdString());
                }
            }
            setting.SetValue(newValue);
        }
    }
}

template <typename Type, bool ranged>
void QtConfig::ReadBasicSetting(Settings::Setting<Type, ranged>& setting) {
    const QString name = QString::fromStdString(setting.GetLabel());
    const Type default_value = setting.GetDefault();
    if (qt_config->value(name + QStringLiteral("/default"), false).toBool()) {
        setting.SetValue(default_value);
    } else {
        QVariant value{};
        if constexpr (std::is_enum_v<Type>) {
            using TypeU = std::underlying_type_t<Type>;
            value = qt_config->value(name, static_cast<TypeU>(default_value));
            setting.SetValue(static_cast<Type>(value.value<TypeU>()));
        } else {
            value = qt_config->value(name, QVariant::fromValue(default_value));
            setting.SetValue(value.value<Type>());
        }
    }
}

template <typename Type, bool ranged>
void QtConfig::ReadGlobalSetting(Settings::SwitchableSetting<Type, ranged>& setting) {
    QString name = QString::fromStdString(setting.GetLabel());
    const bool use_global = qt_config->value(name + QStringLiteral("/use_global"), true).toBool();
    setting.SetGlobal(use_global);
    if (global || !use_global) {
        ReadBasicSetting(setting);
    }
}

// Explicit std::string definition: Qt can't implicitly convert a std::string to a QVariant
template <>
void QtConfig::WriteBasicSetting(const Settings::Setting<std::string>& setting) {
    const QString name = QString::fromStdString(setting.GetLabel());
    const std::string& value = setting.GetValue();
    if (global)
        qt_config->setValue(name + QStringLiteral("/default"), value == setting.GetDefault());
    qt_config->setValue(name, QString::fromStdString(value));
}

template <typename Type, bool ranged>
void QtConfig::WriteBasicSetting(const Settings::Setting<std::vector<Type>, ranged>& setting) {
    const QString name = QString::fromStdString(setting.GetLabel());
    const std::vector<Type>& value = setting.GetValue();

    if (global)
        qt_config->setValue(name + QStringLiteral("/default"), value == setting.GetDefault());

    QStringList stringList;
    if constexpr (std::is_enum_v<Type>) {
        // For enums, convert to underlying integer type strings
        using TypeU = std::underlying_type_t<Type>;
        for (const Type& item : value) {
            stringList.append(QString::number(static_cast<TypeU>(item)));
        }
    } else {
        // For non-enum types (assuming numeric)
        for (const Type& item : value) {
            stringList.append(QString::number(item));
        }
    }
    qt_config->setValue(name, stringList);
}

// Promote configuration types into simpler types so that Qt does not implicitly convert
// configuration values into a QMetaType and ensures the ini files are human-readable
template <typename T>
using config_promoted_t = std::conditional_t<
    // Preserve bools
    std::is_same_v<T, bool>, bool,
    // Signed/Unsigned integers get promoted to s64/u64
    std::conditional_t<std::is_integral_v<T>,
                       std::conditional_t<std::is_signed_v<T>, std::int64_t, std::uint64_t>,
                       // Otherwise, leave the type unchanged
                       T>>;

template <typename Type, bool ranged>
void QtConfig::WriteBasicSetting(const Settings::Setting<Type, ranged>& setting) {
    const QString name = QString::fromStdString(setting.GetLabel());
    const Type value = setting.GetValue();
    if (global)
        qt_config->setValue(name + QStringLiteral("/default"), value == setting.GetDefault());
    if constexpr (std::is_enum_v<Type>) {
        using TypeU = std::underlying_type_t<Type>;
        qt_config->setValue(
            name, QVariant::fromValue<config_promoted_t<TypeU>>(static_cast<TypeU>(value)));
    } else {
        qt_config->setValue(name, QVariant::fromValue<config_promoted_t<Type>>(value));
    }
}

template <typename Type, bool ranged>
void QtConfig::WriteGlobalSetting(const Settings::SwitchableSetting<Type, ranged>& setting) {
    const QString name = QString::fromStdString(setting.GetLabel());
    if (!global) {
        qt_config->setValue(name + QStringLiteral("/use_global"), setting.UsingGlobal());
    }
    if (global || !setting.UsingGlobal()) {
        WriteBasicSetting(setting);
    }
}

void QtConfig::ReadValues() {
    if (global) {
        ReadControlValues();
        ReadCameraValues();
        ReadDataStorageValues();
        ReadMiscellaneousValues();
        ReadDebuggingValues();
        ReadWebServiceValues();
        ReadVideoDumpingValues();
    }

    ReadUIValues();
    ReadCoreValues();
    ReadRendererValues();
    ReadLayoutValues();
    ReadAudioValues();
    ReadSystemValues();
    ReadUtilityValues();
}

void QtConfig::ReadAudioValues() {
    qt_config->beginGroup(QStringLiteral("Audio"));

    ReadGlobalSetting(Settings::values.audio_emulation);
    ReadGlobalSetting(Settings::values.enable_audio_stretching);
    ReadGlobalSetting(Settings::values.enable_realtime_audio);
    ReadGlobalSetting(Settings::values.simulate_headphones_plugged);
    ReadGlobalSetting(Settings::values.volume);

    if (global) {
        ReadBasicSetting(Settings::values.output_type);
        ReadBasicSetting(Settings::values.output_device);
        ReadBasicSetting(Settings::values.input_type);
        ReadBasicSetting(Settings::values.input_device);
    }

    qt_config->endGroup();
}

void QtConfig::ReadCameraValues() {
    using namespace Service::CAM;
    qt_config->beginGroup(QStringLiteral("Camera"));

    Settings::values.camera_name[OuterRightCamera] =
        ReadSetting(Settings::QKeys::camera_outer_right_name, QStringLiteral("blank"))
            .toString()
            .toStdString();
    Settings::values.camera_config[OuterRightCamera] =
        ReadSetting(Settings::QKeys::camera_outer_right_config, QString{}).toString().toStdString();
    Settings::values.camera_flip[OuterRightCamera] =
        ReadSetting(Settings::QKeys::camera_outer_right_flip, 0).toInt();
    Settings::values.camera_name[InnerCamera] =
        ReadSetting(Settings::QKeys::camera_inner_name, QStringLiteral("blank"))
            .toString()
            .toStdString();
    Settings::values.camera_config[InnerCamera] =
        ReadSetting(Settings::QKeys::camera_inner_config, QString{}).toString().toStdString();
    Settings::values.camera_flip[InnerCamera] =
        ReadSetting(Settings::QKeys::camera_inner_flip, 0).toInt();
    Settings::values.camera_name[OuterLeftCamera] =
        ReadSetting(Settings::QKeys::camera_outer_left_name, QStringLiteral("blank"))
            .toString()
            .toStdString();
    Settings::values.camera_config[OuterLeftCamera] =
        ReadSetting(Settings::QKeys::camera_outer_left_config, QString{}).toString().toStdString();
    Settings::values.camera_flip[OuterLeftCamera] =
        ReadSetting(Settings::QKeys::camera_outer_left_flip, 0).toInt();

    qt_config->endGroup();
}

void QtConfig::ReadControlValues() {
    qt_config->beginGroup(QStringLiteral("Controls"));

    ReadBasicSetting(Settings::values.use_artic_base_controller);

    UISettings::values.controller_hotkey_maptype = static_cast<Settings::InputMappingType>(
        ReadSetting(Settings::QKeys::controller_hotkey_maptype,
                    static_cast<int>(Settings::InputMappingType::AllControllers))
            .toInt());

    int num_touch_from_button_maps =
        qt_config->beginReadArray(Settings::QKeys::touch_from_button_maps);

    if (num_touch_from_button_maps > 0) {
        const auto append_touch_from_button_map = [this] {
            Settings::TouchFromButtonMap map;
            map.name = ReadSetting(Settings::QKeys::name, QStringLiteral("default"))
                           .toString()
                           .toStdString();
            const int num_touch_maps = qt_config->beginReadArray(QStringLiteral("entries"));
            map.buttons.reserve(num_touch_maps);
            for (int i = 0; i < num_touch_maps; i++) {
                qt_config->setArrayIndex(i);
                std::string touch_mapping =
                    ReadSetting(Settings::QKeys::bind).toString().toStdString();
                map.buttons.emplace_back(std::move(touch_mapping));
            }
            qt_config->endArray(); // entries
            Settings::values.touch_from_button_maps.emplace_back(std::move(map));
        };

        for (int i = 0; i < num_touch_from_button_maps; ++i) {
            qt_config->setArrayIndex(i);
            append_touch_from_button_map();
        }
    } else {
        Settings::values.touch_from_button_maps.emplace_back(
            Settings::TouchFromButtonMap{"default", {}});
        num_touch_from_button_maps = 1;
    }
    qt_config->endArray();

    Settings::values.current_input_profile_index = ReadSetting(Settings::QKeys::profile, 0).toInt();

    const auto append_profile = [this, num_touch_from_button_maps] {
        Settings::InputProfile profile;
        profile.name =
            ReadSetting(Settings::QKeys::name, QStringLiteral("Default")).toString().toStdString();
        profile.maptype = static_cast<Settings::InputMappingType>(
            ReadSetting(Settings::QKeys::input_maptype, 2).toInt());
        for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
            std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
            profile.buttons[i] = ReadSetting(QString::fromUtf8(Settings::NativeButton::mapping[i]),
                                             QString::fromStdString(default_param))
                                     .toString()
                                     .toStdString();
            if (profile.buttons[i].empty())
                profile.buttons[i] = default_param;
        }
        for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
            std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
                default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
                default_analogs[i][3], default_analogs[i][4], 0.5f);
            profile.analogs[i] =
                ReadSetting(QString::fromStdString(Settings::NativeAnalog::mapping[i]),
                            QString::fromStdString(default_param))
                    .toString()
                    .toStdString();
            if (profile.analogs[i].empty())
                profile.analogs[i] = default_param;
        }
        profile.motion_device =
            ReadSetting(Settings::QKeys::motion_device,
                        QStringLiteral(
                            "engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0"))
                .toString()
                .toStdString();
        profile.touch_device =
            ReadSetting(Settings::QKeys::touch_device, QStringLiteral("engine:emu_window"))
                .toString()
                .toStdString();
        profile.use_touchpad = ReadSetting(Settings::QKeys::use_touchpad, false).toBool();
        profile.controller_touch_device =
            ReadSetting(Settings::QKeys::controller_touch_device, QStringLiteral(""))
                .toString()
                .toStdString();
        profile.use_touch_from_button =
            ReadSetting(Settings::QKeys::use_touch_from_button, false).toBool();
        profile.touch_from_button_map_index =
            ReadSetting(Settings::QKeys::touch_from_button_map, 0).toInt();
        profile.touch_from_button_map_index =
            std::clamp(profile.touch_from_button_map_index, 0, num_touch_from_button_maps - 1);
        profile.udp_input_address =
            ReadSetting(Settings::QKeys::udp_input_address,
                        QString::fromUtf8(InputCommon::CemuhookUDP::DEFAULT_ADDR))
                .toString()
                .toStdString();
        profile.udp_input_port = static_cast<u16>(
            ReadSetting(Settings::QKeys::udp_input_port, InputCommon::CemuhookUDP::DEFAULT_PORT)
                .toInt());
        profile.udp_pad_index =
            static_cast<u8>(ReadSetting(Settings::QKeys::udp_pad_index, 0).toUInt());
        Settings::values.input_profiles.emplace_back(std::move(profile));
    };

    int num_input_profiles = qt_config->beginReadArray(QStringLiteral("profiles"));

    for (int i = 0; i < num_input_profiles; ++i) {
        qt_config->setArrayIndex(i);
        append_profile();
    }

    qt_config->endArray();

    // create a input profile if no input profiles exist, with the default or old settings
    if (num_input_profiles == 0) {
        append_profile();
        num_input_profiles = 1;
    }

    // ensure that the current input profile index is valid.
    Settings::values.current_input_profile_index =
        std::clamp(Settings::values.current_input_profile_index, 0, num_input_profiles - 1);

    Settings::LoadProfile(Settings::values.current_input_profile_index);

    qt_config->endGroup();
}

void QtConfig::ReadUtilityValues() {
    qt_config->beginGroup(QStringLiteral("Utility"));

    ReadGlobalSetting(Settings::values.dump_textures);
    ReadGlobalSetting(Settings::values.custom_textures);
    ReadGlobalSetting(Settings::values.preload_textures);
    ReadGlobalSetting(Settings::values.async_custom_loading);

    qt_config->endGroup();
}

void QtConfig::ReadCoreValues() {
    qt_config->beginGroup(QStringLiteral("Core"));

    ReadGlobalSetting(Settings::values.cpu_clock_percentage);

    if (global) {
        ReadBasicSetting(Settings::values.use_cpu_jit);
        ReadBasicSetting(Settings::values.delay_start_for_lle_modules);
    }

    qt_config->endGroup();
}

void QtConfig::ReadDataStorageValues() {
    qt_config->beginGroup(QStringLiteral("Data Storage"));

    ReadBasicSetting(Settings::values.use_virtual_sd);
    ReadBasicSetting(Settings::values.use_custom_storage);
    ReadBasicSetting(Settings::values.compress_cia_installs);
    ReadBasicSetting(Settings::values.async_fs_operations);

    const std::string nand_dir =
        ReadSetting(Settings::QKeys::nand_directory, QStringLiteral("")).toString().toStdString();
    const std::string sdmc_dir =
        ReadSetting(Settings::QKeys::sdmc_directory, QStringLiteral("")).toString().toStdString();

    if (Settings::values.use_custom_storage) {
        FileUtil::UpdateUserPath(FileUtil::UserPath::NANDDir, nand_dir);
        FileUtil::UpdateUserPath(FileUtil::UserPath::SDMCDir, sdmc_dir);
    }

    qt_config->endGroup();
}

void QtConfig::ReadDebuggingValues() {
    qt_config->beginGroup(QStringLiteral("Debugging"));

    // Intentionally not using the QT default setting as this is intended to be changed in the ini
    Settings::values.record_frame_times =
        qt_config->value(Settings::QKeys::record_frame_times, false).toBool();
    ReadBasicSetting(Settings::values.use_gdbstub);
    ReadBasicSetting(Settings::values.gdbstub_port);
    ReadBasicSetting(Settings::values.renderer_debug);
    ReadBasicSetting(Settings::values.pica_debugging);
    ReadBasicSetting(Settings::values.dump_command_buffers);
    ReadBasicSetting(Settings::values.instant_debug_log);
    ReadBasicSetting(Settings::values.enable_rpc_server);
    ReadBasicSetting(Settings::values.toggle_unique_data_console_type);
    ReadBasicSetting(Settings::values.enable_exception_handler);

    qt_config->beginGroup(QStringLiteral("LLE"));
    for (const auto& service_module : Service::service_module_map) {
        bool use_lle = ReadSetting(QString::fromStdString(service_module.name), false).toBool();
        Settings::values.lle_modules.emplace(service_module.name, use_lle);
    }
    qt_config->endGroup();
    qt_config->endGroup();
}

void QtConfig::ReadLayoutValues() {
    qt_config->beginGroup(QStringLiteral("Layout"));

    ReadGlobalSetting(Settings::values.render_3d);
    ReadGlobalSetting(Settings::values.factor_3d);
    ReadGlobalSetting(Settings::values.swap_eyes_3d);
    ReadGlobalSetting(Settings::values.render_3d_which_display);
    ReadGlobalSetting(Settings::values.filter_mode);
    ReadGlobalSetting(Settings::values.pp_shader_name);
    ReadGlobalSetting(Settings::values.anaglyph_shader_name);
    ReadGlobalSetting(Settings::values.layout_option);
    ReadGlobalSetting(Settings::values.swap_screen);
    ReadGlobalSetting(Settings::values.upright_screen);
    ReadGlobalSetting(Settings::values.large_screen_proportion);
    ReadGlobalSetting(Settings::values.screen_gap);
    ReadGlobalSetting(Settings::values.small_screen_position);
    ReadGlobalSetting(Settings::values.layouts_to_cycle);
    if (global) {
        ReadBasicSetting(Settings::values.mono_render_option);
        ReadBasicSetting(Settings::values.custom_top_x);
        ReadBasicSetting(Settings::values.custom_top_y);
        ReadBasicSetting(Settings::values.custom_top_width);
        ReadBasicSetting(Settings::values.custom_top_height);
        ReadBasicSetting(Settings::values.custom_bottom_x);
        ReadBasicSetting(Settings::values.custom_bottom_y);
        ReadBasicSetting(Settings::values.custom_bottom_width);
        ReadBasicSetting(Settings::values.custom_bottom_height);
        ReadBasicSetting(Settings::values.custom_second_layer_opacity);

        ReadBasicSetting(Settings::values.screen_top_stretch);
        ReadBasicSetting(Settings::values.screen_top_leftright_padding);
        ReadBasicSetting(Settings::values.screen_top_topbottom_padding);
        ReadBasicSetting(Settings::values.screen_bottom_stretch);
        ReadBasicSetting(Settings::values.screen_bottom_leftright_padding);
        ReadBasicSetting(Settings::values.screen_bottom_topbottom_padding);
        ReadBasicSetting(Settings::values.portrait_layout_option);
        ReadBasicSetting(Settings::values.custom_portrait_top_x);
        ReadBasicSetting(Settings::values.custom_portrait_top_y);
        ReadBasicSetting(Settings::values.custom_portrait_top_width);
        ReadBasicSetting(Settings::values.custom_portrait_top_height);
        ReadBasicSetting(Settings::values.custom_portrait_bottom_x);
        ReadBasicSetting(Settings::values.custom_portrait_bottom_y);
        ReadBasicSetting(Settings::values.custom_portrait_bottom_width);
        ReadBasicSetting(Settings::values.custom_portrait_bottom_height);
    }

    qt_config->endGroup();
}

void QtConfig::ReadMiscellaneousValues() {
    qt_config->beginGroup(QStringLiteral("Miscellaneous"));

    ReadBasicSetting(Settings::values.log_filter);
    ReadBasicSetting(Settings::values.log_regex_filter);
#ifdef __unix__
    ReadBasicSetting(Settings::values.enable_gamemode);
#endif
#ifdef ENABLE_QT_UPDATE_CHECKER
    ReadBasicSetting(UISettings::values.check_for_update_on_start);
    ReadBasicSetting(UISettings::values.update_check_channel);
#endif

    qt_config->endGroup();
}

void QtConfig::ReadMultiplayerValues() {
    qt_config->beginGroup(QStringLiteral("Multiplayer"));

    UISettings::values.ip = ReadSetting(Settings::QKeys::ip, QString{}).toString();
    UISettings::values.port =
        ReadSetting(Settings::QKeys::port, Network::DefaultRoomPort).toString();
    UISettings::values.room_name = ReadSetting(Settings::QKeys::room_name, QString{}).toString();
    UISettings::values.room_port =
        ReadSetting(Settings::QKeys::room_port, QStringLiteral("24872")).toString();
    bool ok;
    UISettings::values.host_type = ReadSetting(Settings::QKeys::host_type, 0).toUInt(&ok);
    if (!ok) {
        UISettings::values.host_type = 0;
    }
    UISettings::values.max_player = ReadSetting(Settings::QKeys::max_player, 8).toUInt();
    UISettings::values.game_id = ReadSetting(Settings::QKeys::game_id, 0).toULongLong();
    UISettings::values.room_description =
        ReadSetting(Settings::QKeys::room_description, QString{}).toString();
    UISettings::values.multiplayer_filter_text =
        ReadSetting(Settings::QKeys::multiplayer_filter_text, QString{}).toString();
    UISettings::values.multiplayer_filter_games_owned =
        ReadSetting(Settings::QKeys::multiplayer_filter_games_owned, false).toBool();
    UISettings::values.multiplayer_filter_hide_empty =
        ReadSetting(Settings::QKeys::multiplayer_filter_hide_empty, false).toBool();
    UISettings::values.multiplayer_filter_hide_full =
        ReadSetting(Settings::QKeys::multiplayer_filter_hide_full, false).toBool();

    // Read ban list back
    int size = qt_config->beginReadArray(Settings::QKeys::username_ban_list);
    UISettings::values.ban_list.first.resize(size);
    for (int i = 0; i < size; ++i) {
        qt_config->setArrayIndex(i);
        UISettings::values.ban_list.first[i] =
            ReadSetting(Settings::QKeys::username).toString().toStdString();
    }
    qt_config->endArray();
    size = qt_config->beginReadArray(Settings::QKeys::ip_ban_list);
    UISettings::values.ban_list.second.resize(size);
    for (int i = 0; i < size; ++i) {
        qt_config->setArrayIndex(i);
        UISettings::values.ban_list.second[i] =
            ReadSetting(Settings::QKeys::ip).toString().toStdString();
    }
    qt_config->endArray();

    qt_config->endGroup();
}

void QtConfig::ReadPathValues() {
    qt_config->beginGroup(QStringLiteral("Paths"));

    ReadGlobalSetting(UISettings::values.screenshot_path);

    if (global) {
        UISettings::values.roms_path = ReadSetting(Settings::QKeys::romsPath).toString();
        UISettings::values.symbols_path = ReadSetting(Settings::QKeys::symbolsPath).toString();
        UISettings::values.movie_record_path =
            ReadSetting(Settings::QKeys::movieRecordPath).toString();
        UISettings::values.movie_playback_path =
            ReadSetting(Settings::QKeys::moviePlaybackPath).toString();
        UISettings::values.video_dumping_path =
            ReadSetting(Settings::QKeys::videoDumpingPath).toString();
        UISettings::values.game_dir_deprecated =
            ReadSetting(Settings::QKeys::gameListRootDir, QStringLiteral(".")).toString();
        UISettings::values.game_dir_deprecated_deepscan =
            ReadSetting(Settings::QKeys::gameListDeepScan, false).toBool();
        int size = qt_config->beginReadArray(QStringLiteral("gamedirs"));
        for (int i = 0; i < size; ++i) {
            qt_config->setArrayIndex(i);
            UISettings::GameDir game_dir;
            game_dir.path = ReadSetting(Settings::QKeys::path).toString();
            game_dir.deep_scan = ReadSetting(Settings::QKeys::deep_scan, false).toBool();
            game_dir.expanded = ReadSetting(Settings::QKeys::expanded, true).toBool();
            UISettings::values.game_dirs.append(game_dir);
        }
        qt_config->endArray();
        // create NAND and SD card directories if empty, these are not removable through the UI,
        // also carries over old game list settings if present
        if (UISettings::values.game_dirs.isEmpty()) {
            UISettings::GameDir game_dir;
            game_dir.path = QStringLiteral("INSTALLED");
            game_dir.expanded = true;
            UISettings::values.game_dirs.append(game_dir);
            game_dir.path = QStringLiteral("SYSTEM");
            UISettings::values.game_dirs.append(game_dir);
            if (UISettings::values.game_dir_deprecated != QStringLiteral(".")) {
                game_dir.path = UISettings::values.game_dir_deprecated;
                game_dir.deep_scan = UISettings::values.game_dir_deprecated_deepscan;
                UISettings::values.game_dirs.append(game_dir);
            }
        }
        UISettings::values.last_artic_base_addr =
            ReadSetting(Settings::QKeys::last_artic_base_addr, QString{}).toString();
        UISettings::values.recent_files = ReadSetting(Settings::QKeys::recentFiles).toStringList();
        UISettings::values.language = ReadSetting(Settings::QKeys::language, QString{}).toString();

        ReadBasicSetting(UISettings::values.inserted_cartridge);
        if (!FileUtil::Exists(UISettings::values.inserted_cartridge.GetValue())) {
            UISettings::values.inserted_cartridge.SetValue("");
        }
    }

    qt_config->endGroup();
}

void QtConfig::ReadRendererValues() {
    qt_config->beginGroup(QStringLiteral("Renderer"));

    ReadGlobalSetting(Settings::values.graphics_api);
    ReadGlobalSetting(Settings::values.physical_device);
    ReadGlobalSetting(Settings::values.spirv_shader_gen);
    ReadGlobalSetting(Settings::values.disable_spirv_optimizer);
    ReadGlobalSetting(Settings::values.async_shader_compilation);
    ReadGlobalSetting(Settings::values.async_presentation);
    ReadGlobalSetting(Settings::values.use_hw_shader);
    ReadGlobalSetting(Settings::values.shaders_accurate_mul);
    ReadGlobalSetting(Settings::values.use_disk_shader_cache);
    ReadGlobalSetting(Settings::values.use_vsync);
    ReadGlobalSetting(Settings::values.use_skip_duplicate_frames);
    ReadGlobalSetting(Settings::values.use_display_refresh_rate_detection);
    ReadGlobalSetting(Settings::values.resolution_factor);
    ReadGlobalSetting(Settings::values.use_integer_scaling);
    ReadGlobalSetting(Settings::values.frame_limit);
    ReadGlobalSetting(Settings::values.turbo_limit);

    ReadGlobalSetting(Settings::values.bg_red);
    ReadGlobalSetting(Settings::values.bg_green);
    ReadGlobalSetting(Settings::values.bg_blue);

    ReadGlobalSetting(Settings::values.texture_filter);
    ReadGlobalSetting(Settings::values.texture_sampling);

    ReadGlobalSetting(Settings::values.delay_game_render_thread_us);
    ReadGlobalSetting(Settings::values.disable_right_eye_render);

    ReadGlobalSetting(Settings::values.simulate_3ds_gpu_timings);

    if (global) {
        ReadBasicSetting(Settings::values.use_shader_jit);
    }

    qt_config->endGroup();
}

void QtConfig::ReadShortcutValues() {
    qt_config->beginGroup(QStringLiteral("Shortcuts"));

    for (const auto& [name, group, shortcut] : default_hotkeys) {
        qt_config->beginGroup(group);
        qt_config->beginGroup(name);
        // No longer using ReadSetting for shortcut.second as it innacurately returns a value of 1
        // for WidgetWithChildrenShortcut which is a value of 3. Needed to fix shortcuts the open
        // a file dialog in windowed mode
        UISettings::values.shortcuts.push_back(
            {name,
             group,
             {ReadSetting(Settings::QKeys::KeySeq, shortcut.keyseq).toString(),
              ReadSetting(Settings::QKeys::controller_keyseq, shortcut.controller_keyseq)
                  .toString(),
              shortcut.context}});
        qt_config->endGroup();
        qt_config->endGroup();
    }

    qt_config->endGroup();
}

void QtConfig::ReadSystemValues() {
    qt_config->beginGroup(QStringLiteral("System"));

    ReadGlobalSetting(Settings::values.is_new_3ds);
    ReadGlobalSetting(Settings::values.lle_applets);
    ReadGlobalSetting(Settings::values.enable_required_online_lle_modules);
    ReadGlobalSetting(Settings::values.notify_guest_on_shutdown);
    ReadGlobalSetting(Settings::values.region_value);

    if (global) {
        ReadBasicSetting(Settings::values.init_clock);
        ReadBasicSetting(Settings::values.init_time);
        ReadBasicSetting(Settings::values.init_time_offset);
        ReadBasicSetting(Settings::values.init_ticks_type);
        ReadBasicSetting(Settings::values.init_ticks_override);
        ReadBasicSetting(Settings::values.steps_per_hour);
        ReadBasicSetting(Settings::values.plugin_loader_enabled);
        ReadBasicSetting(Settings::values.allow_plugin_loader);
        ReadBasicSetting(Settings::values.apply_region_free_patch);
    }

    qt_config->endGroup();
}

// Options for variable bit rate live streaming taken from here:
// https://developers.google.com/media/vp9/live-encoding
const QString DEFAULT_VIDEO_ENCODER_OPTIONS =
    QStringLiteral("quality:realtime,speed:6,tile-columns:4,frame-parallel:1,threads:8,row-mt:1");
const QString DEFAULT_AUDIO_ENCODER_OPTIONS = QStringLiteral("");

void QtConfig::ReadVideoDumpingValues() {
    qt_config->beginGroup(QStringLiteral("VideoDumping"));

    Settings::values.output_format =
        ReadSetting(Settings::QKeys::output_format, QStringLiteral("webm"))
            .toString()
            .toStdString();
    Settings::values.format_options =
        ReadSetting(Settings::QKeys::format_options).toString().toStdString();

    Settings::values.video_encoder =
        ReadSetting(Settings::QKeys::video_encoder, QStringLiteral("libvpx-vp9"))
            .toString()
            .toStdString();

    Settings::values.video_encoder_options =
        ReadSetting(Settings::QKeys::video_encoder_options, DEFAULT_VIDEO_ENCODER_OPTIONS)
            .toString()
            .toStdString();

    Settings::values.video_bitrate =
        ReadSetting(Settings::QKeys::video_bitrate, 2500000).toULongLong();

    Settings::values.audio_encoder =
        ReadSetting(Settings::QKeys::audio_encoder, QStringLiteral("libvorbis"))
            .toString()
            .toStdString();
    Settings::values.audio_encoder_options =
        ReadSetting(Settings::QKeys::audio_encoder_options, DEFAULT_AUDIO_ENCODER_OPTIONS)
            .toString()
            .toStdString();
    Settings::values.audio_bitrate =
        ReadSetting(Settings::QKeys::audio_bitrate, 64000).toULongLong();

    qt_config->endGroup();
}

void QtConfig::ReadUIValues() {
    qt_config->beginGroup(QStringLiteral("UI"));

    ReadPathValues();

    if (global) {
        UISettings::values.theme =
            ReadSetting(Settings::QKeys::theme, QString::fromUtf8(UISettings::themes[0].second))
                .toString();
#ifdef ENABLE_DISCORD_RPC
        ReadBasicSetting(UISettings::values.enable_discord_presence);
#endif
        ReadBasicSetting(UISettings::values.screenshot_resolution_factor);

        ReadUILayoutValues();
        ReadUIGameListValues();
        ReadShortcutValues();
        ReadMultiplayerValues();

        ReadBasicSetting(UISettings::values.single_window_mode);
        ReadBasicSetting(UISettings::values.fullscreen);
        ReadBasicSetting(UISettings::values.display_titlebar);
        ReadBasicSetting(UISettings::values.show_filter_bar);
        ReadBasicSetting(UISettings::values.show_status_bar);
        ReadBasicSetting(UISettings::values.show_advanced_frametime_info);
        ReadBasicSetting(UISettings::values.confirm_before_closing);
        ReadBasicSetting(UISettings::values.save_state_warning);
        ReadBasicSetting(UISettings::values.first_start);
        ReadBasicSetting(UISettings::values.callout_flags);
        ReadBasicSetting(UISettings::values.show_console);
        ReadBasicSetting(UISettings::values.pause_when_in_background);
        ReadBasicSetting(UISettings::values.mute_when_in_background);
        ReadBasicSetting(UISettings::values.hide_mouse);
    }

    qt_config->endGroup();
}

void QtConfig::ReadUIGameListValues() {
    qt_config->beginGroup(QStringLiteral("GameList"));

    ReadBasicSetting(UISettings::values.game_list_icon_size);
    ReadBasicSetting(UISettings::values.game_list_row_1);
    ReadBasicSetting(UISettings::values.game_list_row_2);
    ReadBasicSetting(UISettings::values.game_list_hide_no_icon);
    ReadBasicSetting(UISettings::values.game_list_single_line_mode);

    ReadBasicSetting(UISettings::values.show_compat_column);
    ReadBasicSetting(UISettings::values.show_region_column);
    ReadBasicSetting(UISettings::values.show_type_column);
    ReadBasicSetting(UISettings::values.show_size_column);
    ReadBasicSetting(UISettings::values.show_play_time_column);

    const int favorites_size = qt_config->beginReadArray(QStringLiteral("favorites"));
    for (int i = 0; i < favorites_size; i++) {
        qt_config->setArrayIndex(i);
        UISettings::values.favorited_ids.append(
            ReadSetting(Settings::QKeys::program_id).toULongLong());
    }
    qt_config->endArray();

    qt_config->endGroup();
}

void QtConfig::ReadUILayoutValues() {
    qt_config->beginGroup(QStringLiteral("UILayout"));

    UISettings::values.geometry = ReadSetting(Settings::QKeys::geometry).toByteArray();
    UISettings::values.state = ReadSetting(Settings::QKeys::state).toByteArray();
    UISettings::values.renderwindow_geometry =
        ReadSetting(Settings::QKeys::geometryRenderWindow).toByteArray();
    UISettings::values.secondarywindow_geometry =
        ReadSetting(Settings::QKeys::geometrySecondaryWindow).toByteArray();
    UISettings::values.gamelist_header_state =
        ReadSetting(Settings::QKeys::gameListHeaderState).toByteArray();
    UISettings::values.microprofile_geometry =
        ReadSetting(Settings::QKeys::microProfileDialogGeometry).toByteArray();
    ReadBasicSetting(UISettings::values.microprofile_visible);

    qt_config->endGroup();
}

void QtConfig::ReadWebServiceValues() {
    qt_config->beginGroup(QStringLiteral("WebService"));

    ReadBasicSetting(Settings::values.web_api_url);
    ReadBasicSetting(Settings::values.network_token);

    qt_config->endGroup();
}

void QtConfig::SaveValues() {
    if (global) {
        SaveControlValues();
        SaveCameraValues();
        SaveDataStorageValues();
        SaveMiscellaneousValues();
        SaveDebuggingValues();
        SaveWebServiceValues();
        SaveVideoDumpingValues();
    }

    SaveUIValues();
    SaveCoreValues();
    SaveRendererValues();
    SaveLayoutValues();
    SaveAudioValues();
    SaveSystemValues();
    SaveUtilityValues();
    qt_config->sync();
}

void QtConfig::SaveAudioValues() {
    qt_config->beginGroup(QStringLiteral("Audio"));

    WriteGlobalSetting(Settings::values.audio_emulation);
    WriteGlobalSetting(Settings::values.enable_audio_stretching);
    WriteGlobalSetting(Settings::values.enable_realtime_audio);
    WriteGlobalSetting(Settings::values.simulate_headphones_plugged);
    WriteGlobalSetting(Settings::values.volume);

    if (global) {
        WriteBasicSetting(Settings::values.output_type);
        WriteBasicSetting(Settings::values.output_device);
        WriteBasicSetting(Settings::values.input_type);
        WriteBasicSetting(Settings::values.input_device);
    }

    qt_config->endGroup();
}

void QtConfig::SaveCameraValues() {
    using namespace Service::CAM;
    qt_config->beginGroup(QStringLiteral("Camera"));

    WriteSetting(Settings::QKeys::camera_outer_right_name,
                 QString::fromStdString(Settings::values.camera_name[OuterRightCamera]),
                 QStringLiteral("blank"));
    WriteSetting(Settings::QKeys::camera_outer_right_config,
                 QString::fromStdString(Settings::values.camera_config[OuterRightCamera]),
                 QString{});
    WriteSetting(Settings::QKeys::camera_outer_right_flip,
                 Settings::values.camera_flip[OuterRightCamera], 0);
    WriteSetting(Settings::QKeys::camera_inner_name,
                 QString::fromStdString(Settings::values.camera_name[InnerCamera]),
                 QStringLiteral("blank"));
    WriteSetting(Settings::QKeys::camera_inner_config,
                 QString::fromStdString(Settings::values.camera_config[InnerCamera]), QString{});
    WriteSetting(Settings::QKeys::camera_inner_flip, Settings::values.camera_flip[InnerCamera], 0);
    WriteSetting(Settings::QKeys::camera_outer_left_name,
                 QString::fromStdString(Settings::values.camera_name[OuterLeftCamera]),
                 QStringLiteral("blank"));
    WriteSetting(Settings::QKeys::camera_outer_left_config,
                 QString::fromStdString(Settings::values.camera_config[OuterLeftCamera]),
                 QString{});
    WriteSetting(Settings::QKeys::camera_outer_left_flip,
                 Settings::values.camera_flip[OuterLeftCamera], 0);

    qt_config->endGroup();
}

void QtConfig::SaveControlValues() {
    qt_config->beginGroup(QStringLiteral("Controls"));

    WriteBasicSetting(Settings::values.use_artic_base_controller);
    WriteSetting(Settings::QKeys::controller_hotkey_maptype,
                 static_cast<int>(UISettings::values.controller_hotkey_maptype.GetValue()),
                 static_cast<int>(Settings::InputMappingType::GuidPort));
    WriteSetting(Settings::QKeys::profile, Settings::values.current_input_profile_index, 0);
    qt_config->beginWriteArray(QStringLiteral("profiles"));
    for (std::size_t p = 0; p < Settings::values.input_profiles.size(); ++p) {
        qt_config->setArrayIndex(static_cast<int>(p));
        const auto& profile = Settings::values.input_profiles[p];
        WriteSetting(Settings::QKeys::name, QString::fromStdString(profile.name),
                     QStringLiteral("default"));
        WriteSetting(Settings::QKeys::input_maptype, static_cast<int>(profile.maptype),
                     static_cast<int>(Settings::InputMappingType::GuidPort));
        for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
            std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
            WriteSetting(QString::fromStdString(Settings::NativeButton::mapping[i]),
                         QString::fromStdString(profile.buttons[i]),
                         QString::fromStdString(default_param));
        }
        for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
            std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
                default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
                default_analogs[i][3], default_analogs[i][4], 0.5f);
            WriteSetting(QString::fromStdString(Settings::NativeAnalog::mapping[i]),
                         QString::fromStdString(profile.analogs[i]),
                         QString::fromStdString(default_param));
        }
        WriteSetting(
            Settings::QKeys::motion_device, QString::fromStdString(profile.motion_device),
            QStringLiteral("engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0"));
        WriteSetting(Settings::QKeys::touch_device, QString::fromStdString(profile.touch_device),
                     QStringLiteral("engine:emu_window"));
        WriteSetting(Settings::QKeys::use_touchpad, profile.use_touchpad, false);
        WriteSetting(Settings::QKeys::controller_touch_device,
                     QString::fromStdString(profile.controller_touch_device), QStringLiteral(""));
        WriteSetting(Settings::QKeys::use_touch_from_button, profile.use_touch_from_button, false);
        WriteSetting(Settings::QKeys::touch_from_button_map, profile.touch_from_button_map_index,
                     0);
        WriteSetting(Settings::QKeys::udp_input_address,
                     QString::fromStdString(profile.udp_input_address),
                     QString::fromUtf8(InputCommon::CemuhookUDP::DEFAULT_ADDR));
        WriteSetting(Settings::QKeys::udp_input_port, profile.udp_input_port,
                     InputCommon::CemuhookUDP::DEFAULT_PORT);
        WriteSetting(Settings::QKeys::udp_pad_index, profile.udp_pad_index, 0);
    }
    qt_config->endArray();

    qt_config->beginWriteArray(Settings::QKeys::touch_from_button_maps);
    for (std::size_t p = 0; p < Settings::values.touch_from_button_maps.size(); ++p) {
        qt_config->setArrayIndex(static_cast<int>(p));
        const auto& map = Settings::values.touch_from_button_maps[p];
        WriteSetting(Settings::QKeys::name, QString::fromStdString(map.name),
                     QStringLiteral("default"));
        qt_config->beginWriteArray(QStringLiteral("entries"));
        for (std::size_t q = 0; q < map.buttons.size(); ++q) {
            qt_config->setArrayIndex(static_cast<int>(q));
            WriteSetting(Settings::QKeys::bind, QString::fromStdString(map.buttons[q]));
        }
        qt_config->endArray();
    }
    qt_config->endArray();

    qt_config->endGroup();
}

void QtConfig::SaveUtilityValues() {
    qt_config->beginGroup(QStringLiteral("Utility"));

    WriteGlobalSetting(Settings::values.dump_textures);
    WriteGlobalSetting(Settings::values.custom_textures);
    WriteGlobalSetting(Settings::values.preload_textures);
    WriteGlobalSetting(Settings::values.async_custom_loading);

    qt_config->endGroup();
}

void QtConfig::SaveCoreValues() {
    qt_config->beginGroup(QStringLiteral("Core"));

    WriteGlobalSetting(Settings::values.cpu_clock_percentage);

    if (global) {
        WriteBasicSetting(Settings::values.use_cpu_jit);
        WriteBasicSetting(Settings::values.delay_start_for_lle_modules);
    }

    qt_config->endGroup();
}

void QtConfig::SaveDataStorageValues() {
    qt_config->beginGroup(QStringLiteral("Data Storage"));

    WriteBasicSetting(Settings::values.use_virtual_sd);
    WriteBasicSetting(Settings::values.use_custom_storage);
    WriteBasicSetting(Settings::values.compress_cia_installs);
    WriteBasicSetting(Settings::values.async_fs_operations);
    WriteSetting(Settings::QKeys::nand_directory,
                 QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::NANDDir)),
                 QStringLiteral(""));
    WriteSetting(Settings::QKeys::sdmc_directory,
                 QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir)),
                 QStringLiteral(""));

    qt_config->endGroup();
}

void QtConfig::SaveDebuggingValues() {
    qt_config->beginGroup(QStringLiteral("Debugging"));

    // Intentionally not using the QT default setting as this is intended to be changed in the ini
    qt_config->setValue(Settings::QKeys::record_frame_times, Settings::values.record_frame_times);
    WriteBasicSetting(Settings::values.use_gdbstub);
    WriteBasicSetting(Settings::values.gdbstub_port);
    WriteBasicSetting(Settings::values.renderer_debug);
    WriteBasicSetting(Settings::values.pica_debugging);
    WriteBasicSetting(Settings::values.instant_debug_log);
    WriteBasicSetting(Settings::values.enable_rpc_server);
    WriteBasicSetting(Settings::values.toggle_unique_data_console_type);
    WriteBasicSetting(Settings::values.enable_exception_handler);

    qt_config->beginGroup(QStringLiteral("LLE"));
    for (const auto& service_module : Settings::values.lle_modules) {
        WriteSetting(QString::fromStdString(service_module.first), service_module.second, false);
    }
    qt_config->endGroup();

    qt_config->endGroup();
}

void QtConfig::SaveLayoutValues() {
    qt_config->beginGroup(QStringLiteral("Layout"));

    WriteGlobalSetting(Settings::values.render_3d);
    WriteGlobalSetting(Settings::values.factor_3d);
    WriteGlobalSetting(Settings::values.swap_eyes_3d);
    WriteGlobalSetting(Settings::values.render_3d_which_display);
    WriteGlobalSetting(Settings::values.filter_mode);
    WriteGlobalSetting(Settings::values.pp_shader_name);
    WriteGlobalSetting(Settings::values.anaglyph_shader_name);
    WriteGlobalSetting(Settings::values.layout_option);
    WriteGlobalSetting(Settings::values.swap_screen);
    WriteGlobalSetting(Settings::values.upright_screen);
    WriteGlobalSetting(Settings::values.large_screen_proportion);
    WriteGlobalSetting(Settings::values.screen_gap);
    WriteGlobalSetting(Settings::values.small_screen_position);
    WriteGlobalSetting(Settings::values.layouts_to_cycle);
    if (global) {
        WriteBasicSetting(Settings::values.mono_render_option);
        WriteBasicSetting(Settings::values.custom_top_x);
        WriteBasicSetting(Settings::values.custom_top_y);
        WriteBasicSetting(Settings::values.custom_top_width);
        WriteBasicSetting(Settings::values.custom_top_height);
        WriteBasicSetting(Settings::values.custom_bottom_x);
        WriteBasicSetting(Settings::values.custom_bottom_y);
        WriteBasicSetting(Settings::values.custom_bottom_width);
        WriteBasicSetting(Settings::values.custom_bottom_height);
        WriteBasicSetting(Settings::values.custom_second_layer_opacity);

        WriteBasicSetting(Settings::values.screen_top_stretch);
        WriteBasicSetting(Settings::values.screen_top_leftright_padding);
        WriteBasicSetting(Settings::values.screen_top_topbottom_padding);
        WriteBasicSetting(Settings::values.screen_bottom_stretch);
        WriteBasicSetting(Settings::values.screen_bottom_leftright_padding);
        WriteBasicSetting(Settings::values.screen_bottom_topbottom_padding);
        WriteBasicSetting(Settings::values.custom_portrait_top_x);
        WriteBasicSetting(Settings::values.custom_portrait_top_y);
        WriteBasicSetting(Settings::values.custom_portrait_top_width);
        WriteBasicSetting(Settings::values.custom_portrait_top_height);
        WriteBasicSetting(Settings::values.custom_portrait_bottom_x);
        WriteBasicSetting(Settings::values.custom_portrait_bottom_y);
        WriteBasicSetting(Settings::values.custom_portrait_bottom_width);
        WriteBasicSetting(Settings::values.custom_portrait_bottom_height);
    }

    qt_config->endGroup();
}

void QtConfig::SaveMiscellaneousValues() {
    qt_config->beginGroup(QStringLiteral("Miscellaneous"));

    WriteBasicSetting(Settings::values.log_filter);
    WriteBasicSetting(Settings::values.log_regex_filter);
#ifdef __unix__
    WriteBasicSetting(Settings::values.enable_gamemode);
#endif
#ifdef ENABLE_QT_UPDATE_CHECKER
    WriteBasicSetting(UISettings::values.check_for_update_on_start);
    WriteBasicSetting(UISettings::values.update_check_channel);
#endif
    qt_config->endGroup();
}

void QtConfig::SaveMultiplayerValues() {
    qt_config->beginGroup(QStringLiteral("Multiplayer"));

    WriteSetting(Settings::QKeys::ip, UISettings::values.ip, QString{});
    WriteSetting(Settings::QKeys::port, UISettings::values.port, Network::DefaultRoomPort);
    WriteSetting(Settings::QKeys::room_name, UISettings::values.room_name, QString{});
    WriteSetting(Settings::QKeys::room_port, UISettings::values.room_port, QStringLiteral("24872"));
    WriteSetting(Settings::QKeys::host_type, UISettings::values.host_type, 0);
    WriteSetting(Settings::QKeys::max_player, UISettings::values.max_player, 8);
    WriteSetting(Settings::QKeys::game_id, UISettings::values.game_id, 0);
    WriteSetting(Settings::QKeys::room_description, UISettings::values.room_description, QString{});
    WriteSetting(Settings::QKeys::multiplayer_filter_text,
                 UISettings::values.multiplayer_filter_text, QString{});
    WriteSetting(Settings::QKeys::multiplayer_filter_games_owned,
                 UISettings::values.multiplayer_filter_games_owned, false);
    WriteSetting(Settings::QKeys::multiplayer_filter_hide_empty,
                 UISettings::values.multiplayer_filter_hide_empty, false);
    WriteSetting(Settings::QKeys::multiplayer_filter_hide_full,
                 UISettings::values.multiplayer_filter_hide_full, false);

    // Write ban list
    qt_config->beginWriteArray(Settings::QKeys::username_ban_list);
    for (std::size_t i = 0; i < UISettings::values.ban_list.first.size(); ++i) {
        qt_config->setArrayIndex(static_cast<int>(i));
        WriteSetting(Settings::QKeys::username,
                     QString::fromStdString(UISettings::values.ban_list.first[i]));
    }
    qt_config->endArray();
    qt_config->beginWriteArray(Settings::QKeys::ip_ban_list);
    for (std::size_t i = 0; i < UISettings::values.ban_list.second.size(); ++i) {
        qt_config->setArrayIndex(static_cast<int>(i));
        WriteSetting(Settings::QKeys::ip,
                     QString::fromStdString(UISettings::values.ban_list.second[i]));
    }
    qt_config->endArray();

    qt_config->endGroup();
}

void QtConfig::SavePathValues() {
    qt_config->beginGroup(QStringLiteral("Paths"));

    WriteGlobalSetting(UISettings::values.screenshot_path);
    if (global) {
        WriteSetting(Settings::QKeys::romsPath, UISettings::values.roms_path);
        WriteSetting(Settings::QKeys::symbolsPath, UISettings::values.symbols_path);
        WriteSetting(Settings::QKeys::movieRecordPath, UISettings::values.movie_record_path);
        WriteSetting(Settings::QKeys::moviePlaybackPath, UISettings::values.movie_playback_path);
        WriteSetting(Settings::QKeys::videoDumpingPath, UISettings::values.video_dumping_path);
        qt_config->beginWriteArray(Settings::QKeys::gamedirs);
        for (int i = 0; i < UISettings::values.game_dirs.size(); ++i) {
            qt_config->setArrayIndex(i);
            const auto& game_dir = UISettings::values.game_dirs[i];
            WriteSetting(Settings::QKeys::path, game_dir.path);
            WriteSetting(Settings::QKeys::deep_scan, game_dir.deep_scan, false);
            WriteSetting(Settings::QKeys::expanded, game_dir.expanded, true);
        }
        qt_config->endArray();
        WriteSetting(Settings::QKeys::last_artic_base_addr, UISettings::values.last_artic_base_addr,
                     QString{});
        WriteSetting(Settings::QKeys::recentFiles, UISettings::values.recent_files);
        WriteSetting(Settings::QKeys::language, UISettings::values.language, QString{});
        WriteBasicSetting(UISettings::values.inserted_cartridge);
    }

    qt_config->endGroup();
}

void QtConfig::SaveRendererValues() {
    qt_config->beginGroup(QStringLiteral("Renderer"));

    WriteGlobalSetting(Settings::values.graphics_api);
    WriteGlobalSetting(Settings::values.physical_device);
    WriteGlobalSetting(Settings::values.spirv_shader_gen);
    WriteGlobalSetting(Settings::values.disable_spirv_optimizer);
    WriteGlobalSetting(Settings::values.async_shader_compilation);
    WriteGlobalSetting(Settings::values.async_presentation);
    WriteGlobalSetting(Settings::values.use_hw_shader);
    WriteGlobalSetting(Settings::values.shaders_accurate_mul);
    WriteGlobalSetting(Settings::values.use_disk_shader_cache);
    WriteGlobalSetting(Settings::values.use_vsync);
    WriteGlobalSetting(Settings::values.use_skip_duplicate_frames);
    WriteGlobalSetting(Settings::values.use_display_refresh_rate_detection);
    WriteGlobalSetting(Settings::values.resolution_factor);
    WriteGlobalSetting(Settings::values.use_integer_scaling);
    WriteGlobalSetting(Settings::values.frame_limit);
    WriteGlobalSetting(Settings::values.turbo_limit);

    WriteGlobalSetting(Settings::values.bg_red);
    WriteGlobalSetting(Settings::values.bg_green);
    WriteGlobalSetting(Settings::values.bg_blue);

    WriteGlobalSetting(Settings::values.texture_filter);
    WriteGlobalSetting(Settings::values.texture_sampling);

    WriteGlobalSetting(Settings::values.delay_game_render_thread_us);
    WriteGlobalSetting(Settings::values.disable_right_eye_render);

    WriteGlobalSetting(Settings::values.simulate_3ds_gpu_timings);

    if (global) {
        WriteSetting(Settings::QKeys::use_shader_jit, Settings::values.use_shader_jit.GetValue(),
                     true);
    }

    qt_config->endGroup();
}

void QtConfig::SaveShortcutValues() {
    qt_config->beginGroup(QStringLiteral("Shortcuts"));

    // Lengths of UISettings::values.shortcuts & default_hotkeys are same.
    // However, their ordering must also be the same.
    for (std::size_t i = 0; i < default_hotkeys.size(); i++) {
        const auto& [name, group, shortcut] = UISettings::values.shortcuts[i];
        const auto& default_hotkey = default_hotkeys[i].shortcut;

        qt_config->beginGroup(group);
        qt_config->beginGroup(name);
        WriteSetting(Settings::QKeys::KeySeq, shortcut.keyseq, default_hotkey.keyseq);
        WriteSetting(Settings::QKeys::Context, shortcut.context, default_hotkey.context);
        WriteSetting(Settings::QKeys::controller_keyseq, shortcut.controller_keyseq,
                     default_hotkey.controller_keyseq);
        qt_config->endGroup();
        qt_config->endGroup();
    }

    qt_config->endGroup();
}

void QtConfig::SaveSystemValues() {
    qt_config->beginGroup(QStringLiteral("System"));

    WriteGlobalSetting(Settings::values.is_new_3ds);
    WriteGlobalSetting(Settings::values.lle_applets);
    WriteGlobalSetting(Settings::values.enable_required_online_lle_modules);
    WriteGlobalSetting(Settings::values.notify_guest_on_shutdown);
    WriteGlobalSetting(Settings::values.region_value);

    if (global) {
        WriteBasicSetting(Settings::values.init_clock);
        WriteBasicSetting(Settings::values.init_time);
        WriteBasicSetting(Settings::values.init_time_offset);
        WriteBasicSetting(Settings::values.init_ticks_type);
        WriteBasicSetting(Settings::values.init_ticks_override);
        WriteBasicSetting(Settings::values.steps_per_hour);
        WriteBasicSetting(Settings::values.plugin_loader_enabled);
        WriteBasicSetting(Settings::values.allow_plugin_loader);
        WriteBasicSetting(Settings::values.apply_region_free_patch);
    }

    qt_config->endGroup();
}

void QtConfig::SaveVideoDumpingValues() {
    qt_config->beginGroup(QStringLiteral("VideoDumping"));

    WriteSetting(Settings::QKeys::output_format,
                 QString::fromStdString(Settings::values.output_format), QStringLiteral("webm"));
    WriteSetting(Settings::QKeys::format_options,
                 QString::fromStdString(Settings::values.format_options));
    WriteSetting(Settings::QKeys::video_encoder,
                 QString::fromStdString(Settings::values.video_encoder),
                 QStringLiteral("libvpx-vp9"));
    WriteSetting(Settings::QKeys::video_encoder_options,
                 QString::fromStdString(Settings::values.video_encoder_options),
                 DEFAULT_VIDEO_ENCODER_OPTIONS);
    WriteSetting(Settings::QKeys::video_bitrate,
                 static_cast<unsigned long long>(Settings::values.video_bitrate), 2500000);
    WriteSetting(Settings::QKeys::audio_encoder,
                 QString::fromStdString(Settings::values.audio_encoder),
                 QStringLiteral("libvorbis"));
    WriteSetting(Settings::QKeys::audio_encoder_options,
                 QString::fromStdString(Settings::values.audio_encoder_options),
                 DEFAULT_AUDIO_ENCODER_OPTIONS);
    WriteSetting(Settings::QKeys::audio_bitrate,
                 static_cast<unsigned long long>(Settings::values.audio_bitrate), 64000);

    qt_config->endGroup();
}

void QtConfig::SaveUIValues() {
    qt_config->beginGroup(QStringLiteral("UI"));

    SavePathValues();

    if (global) {
        WriteSetting(Settings::QKeys::theme, UISettings::values.theme,
                     QString::fromUtf8(UISettings::themes[0].second));
#ifdef ENABLE_DISCORD_RPC
        WriteBasicSetting(UISettings::values.enable_discord_presence);
#endif
        WriteBasicSetting(UISettings::values.screenshot_resolution_factor);

        SaveUILayoutValues();
        SaveUIGameListValues();
        SaveShortcutValues();
        SaveMultiplayerValues();

        WriteBasicSetting(UISettings::values.single_window_mode);
        WriteBasicSetting(UISettings::values.fullscreen);
        WriteBasicSetting(UISettings::values.display_titlebar);
        WriteBasicSetting(UISettings::values.show_filter_bar);
        WriteBasicSetting(UISettings::values.show_status_bar);
        WriteBasicSetting(UISettings::values.show_advanced_frametime_info);
        WriteBasicSetting(UISettings::values.confirm_before_closing);
        WriteBasicSetting(UISettings::values.save_state_warning);
        WriteBasicSetting(UISettings::values.first_start);
        WriteBasicSetting(UISettings::values.callout_flags);
        WriteBasicSetting(UISettings::values.show_console);
        WriteBasicSetting(UISettings::values.pause_when_in_background);
        WriteBasicSetting(UISettings::values.mute_when_in_background);
        WriteBasicSetting(UISettings::values.hide_mouse);
    }

    qt_config->endGroup();
}

void QtConfig::SaveUIGameListValues() {
    qt_config->beginGroup(QStringLiteral("GameList"));

    WriteBasicSetting(UISettings::values.game_list_icon_size);
    WriteBasicSetting(UISettings::values.game_list_row_1);
    WriteBasicSetting(UISettings::values.game_list_row_2);
    WriteBasicSetting(UISettings::values.game_list_hide_no_icon);
    WriteBasicSetting(UISettings::values.game_list_single_line_mode);

    WriteBasicSetting(UISettings::values.show_compat_column);
    WriteBasicSetting(UISettings::values.show_region_column);
    WriteBasicSetting(UISettings::values.show_type_column);
    WriteBasicSetting(UISettings::values.show_size_column);
    WriteBasicSetting(UISettings::values.show_play_time_column);

    qt_config->beginWriteArray(Settings::QKeys::favorites);
    for (int i = 0; i < UISettings::values.favorited_ids.size(); i++) {
        qt_config->setArrayIndex(i);
        WriteSetting(Settings::QKeys::program_id,
                     QVariant::fromValue(UISettings::values.favorited_ids[i]));
    }
    qt_config->endArray();

    qt_config->endGroup();
}

void QtConfig::SaveUILayoutValues() {
    qt_config->beginGroup(QStringLiteral("UILayout"));

    WriteSetting(Settings::QKeys::geometry, UISettings::values.geometry);
    WriteSetting(Settings::QKeys::state, UISettings::values.state);
    WriteSetting(Settings::QKeys::geometryRenderWindow, UISettings::values.renderwindow_geometry);
    WriteSetting(Settings::QKeys::geometrySecondaryWindow,
                 UISettings::values.secondarywindow_geometry);
    WriteSetting(Settings::QKeys::gameListHeaderState, UISettings::values.gamelist_header_state);
    WriteSetting(Settings::QKeys::microProfileDialogGeometry,
                 UISettings::values.microprofile_geometry);
    WriteBasicSetting(UISettings::values.microprofile_visible);

    qt_config->endGroup();
}

void QtConfig::SaveWebServiceValues() {
    qt_config->beginGroup(QStringLiteral("WebService"));

    WriteBasicSetting(Settings::values.web_api_url);
    WriteBasicSetting(Settings::values.network_token);

    qt_config->endGroup();
}

QVariant QtConfig::ReadSetting(const QString& name) const {
    return qt_config->value(name);
}

QVariant QtConfig::ReadSetting(const QString& name, const QVariant& default_value) const {
    QVariant result;
    if (qt_config->value(name + QStringLiteral("/default"), false).toBool()) {
        result = default_value;
    } else {
        result = qt_config->value(name, default_value);
    }
    return result;
}

void QtConfig::WriteSetting(const QString& name, const QVariant& value) {
    qt_config->setValue(name, value);
}

void QtConfig::WriteSetting(const QString& name, const QVariant& value,
                            const QVariant& default_value) {
    if (global)
        qt_config->setValue(name + QStringLiteral("/default"), value == default_value);
    qt_config->setValue(name, value);
}

void QtConfig::Reload() {
    ReadValues();
    // To apply default value changes
    SaveValues();
}

void QtConfig::Save() {
    SaveValues();
}
