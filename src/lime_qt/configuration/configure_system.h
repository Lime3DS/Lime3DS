// Copyright 2016 Citra Emulator Project
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

    void InstallSecureData(const std::string& from_path, const std::string& to_path);
    void InstallCTCert(const std::string& from_path);
    void RefreshSecureDataStatus();

    void SetupPerGameUI();

    void DownloadFromNUS();

private:
    std::unique_ptr<Ui::ConfigureSystem> ui;
    Core::System& system;
    ConfigurationShared::CheckState is_new_3ds;
    ConfigurationShared::CheckState lle_applets;
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
};
