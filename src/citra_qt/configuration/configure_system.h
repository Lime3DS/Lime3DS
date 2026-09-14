// Copyright 2016-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>
#include "common/common_types.h"

namespace Ui {
class ConfigureSystem;
}

namespace ConfigurationShared {
enum class CheckState;
}

namespace Core {
class System;
}

namespace Service {
namespace AM {
class Module;
} // namespace AM
} // namespace Service

namespace Service {
namespace CFG {
class Module;
} // namespace CFG
} // namespace Service

class ConfigureSystem : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureSystem(Core::System& system, QWidget* parent = nullptr);
    ~ConfigureSystem() override;

    void ApplyConfiguration();
    void SetConfiguration();
    void RetranslateUI();

private:
    void ReadSystemSettings();
    void ConfigureTime();

    void UpdateBirthdayComboBox(int birthmonth_index);
    void UpdateInitTime(int init_clock);
    void UpdateInitTicks(int init_ticks_type);
    void RefreshConsoleID();
    void RefreshMAC();
    void UnlinkConsole();
    void CheckCountryValid(u8 country);

    void InstallSecureData(const std::string& from_path, const std::string& to_path);
    void RefreshSecureDataStatus();

    void SetupPerGameUI();

private:
    std::unique_ptr<Ui::ConfigureSystem> ui;
    Core::System& system;
    ConfigurationShared::CheckState is_new_3ds;
    ConfigurationShared::CheckState lle_applets;
    ConfigurationShared::CheckState required_online_lle_modules;
    ConfigurationShared::CheckState notify_guest_on_shutdown;
    bool enabled = false;

    std::shared_ptr<Service::CFG::Module> cfg;
    std::u16string username;
    int birthmonth = 0;
    int birthday = 0;
    int language_index = 0;
    int sound_index = 0;
    u8 country_code;
    u16 play_coin;
    bool system_setup;
    std::string mac_address;
};
