// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>
#include <QVariant>
#include "common/settings.h"
#include "lime_qt/uisettings.h"

class QSettings;

class QtConfig {
public:
    enum class ConfigType : u32 { GlobalConfig, PerGameConfig };

    explicit QtConfig(const std::string& config_name = "qt-config",
                      ConfigType config_type = ConfigType::GlobalConfig);
    ~QtConfig();

    void Reload();
    void Save();

    static const std::array<int, Settings::NativeButton::NumButtons> default_buttons;
    static const std::array<std::array<int, 5>, Settings::NativeAnalog::NumAnalogs> default_analogs;
    static const std::array<UISettings::Shortcut, 35> default_hotkeys;

private:
    void Initialize(const std::string& config_name);

    void ReadValues();
    void ReadAudioValues();
    void ReadCameraValues();
    void ReadControlValues();
    void ReadCoreValues();
    void ReadDataStorageValues();
    void ReadDebuggingValues();
    void ReadLayoutValues();
    void ReadMiscellaneousValues();
    void ReadMultiplayerValues();
    void ReadPathValues();
    void ReadRendererValues();
    void ReadShortcutValues();
    void ReadSystemValues();
    void ReadUIValues();
    void ReadUIGameListValues();
    void ReadUILayoutValues();
    void ReadUpdaterValues();
    void ReadUtilityValues();
    void ReadWebServiceValues();
    void ReadVideoDumpingValues();

    void SaveValues();
    void SaveAudioValues();
    void SaveCameraValues();
    void SaveControlValues();
    void SaveCoreValues();
    void SaveDataStorageValues();
    void SaveDebuggingValues();
    void SaveLayoutValues();
    void SaveMiscellaneousValues();
    void SaveMultiplayerValues();
    void SavePathValues();
    void SaveRendererValues();
    void SaveShortcutValues();
    void SaveSystemValues();
    void SaveUIValues();
    void SaveUIGameListValues();
    void SaveUILayoutValues();
    void SaveUpdaterValues();
    void SaveUtilityValues();
    void SaveWebServiceValues();
    void SaveVideoDumpingValues();

    /**
     * Reads a setting from the qt_config.
     *
     * @param name The setting's identifier
     * @param default_value The value to use when the setting is not already present in the config
     */
    QVariant ReadSetting(const QString& name) const;
    QVariant ReadSetting(const QString& name, const QVariant& default_value) const;

    /**
     * Writes a setting to the qt_config.
     *
     * @param name The setting's idetentifier
     * @param value Value of the setting
     * @param default_value Default of the setting if not present in qt_config
     */
    void WriteSetting(const QString& name, const QVariant& value);
    void WriteSetting(const QString& name, const QVariant& value, const QVariant& default_value);

    /**
     * Reads a value from the qt_config and applies it to the setting, using its label and default
     * value. If the config is a custom config, this will also read the global state of the setting
     * and apply that information to it.
     *
     * @param The setting
     */
    template <typename Type, bool ranged>
    void ReadGlobalSetting(Settings::SwitchableSetting<Type, ranged>& setting);

    /**
     * Sets a value to the qt_config using the setting's label and default value. If the config is a
     * custom config, it will apply the global state, and the custom value if needed.
     *
     * @param The setting
     */
    template <typename Type, bool ranged>
    void WriteGlobalSetting(const Settings::SwitchableSetting<Type, ranged>& setting);

    /**
     * Reads a value from the qt_config using the setting's label and default value and applies the
     * value to the setting.
     *
     * @param The setting
     */
    template <typename Type, bool ranged>
    void ReadBasicSetting(Settings::Setting<Type, ranged>& setting);

    /** Sets a value from the setting in the qt_config using the setting's label and default value.
     *
     * @param The setting
     */
    template <typename Type, bool ranged>
    void WriteBasicSetting(const Settings::Setting<Type, ranged>& setting);

    ConfigType type;
    std::unique_ptr<QSettings> qt_config;
    std::string qt_config_loc;
    bool global;
};
