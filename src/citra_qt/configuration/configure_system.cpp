// Copyright 2016-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QMessageBox>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrentMap>
#include "citra_qt/configuration/configuration_shared.h"
#include "citra_qt/configuration/configure_system.h"
#include "common/file_util.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hw/aes/key.h"
#include "core/hw/unique_data.h"
#include "core/system_titles.h"
#include "ui_configure_system.h"

static const std::array<int, 12> days_in_month = {{
    31,
    29,
    31,
    30,
    31,
    30,
    31,
    31,
    30,
    31,
    30,
    31,
}};

static const std::array<const char*, 187> country_names = {
    "",
    QT_TRANSLATE_NOOP("ConfigureSystem", "Japan"),
    "",
    "",
    "",
    "",
    "",
    "",
    QT_TRANSLATE_NOOP("ConfigureSystem", "Anguilla"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Antigua and Barbuda"), // 0-9
    QT_TRANSLATE_NOOP("ConfigureSystem", "Argentina"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Aruba"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Bahamas"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Barbados"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Belize"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Bolivia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Brazil"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "British Virgin Islands"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Canada"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Cayman Islands"), // 10-19
    QT_TRANSLATE_NOOP("ConfigureSystem", "Chile"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Colombia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Costa Rica"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Dominica"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Dominican Republic"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Ecuador"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "El Salvador"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "French Guiana"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Grenada"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Guadeloupe"), // 20-29
    QT_TRANSLATE_NOOP("ConfigureSystem", "Guatemala"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Guyana"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Haiti"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Honduras"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Jamaica"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Martinique"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Mexico"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Montserrat"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Netherlands Antilles"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Nicaragua"), // 30-39
    QT_TRANSLATE_NOOP("ConfigureSystem", "Panama"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Paraguay"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Peru"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Saint Kitts and Nevis"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Saint Lucia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Saint Vincent and the Grenadines"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Suriname"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Trinidad and Tobago"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Turks and Caicos Islands"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "United States"), // 40-49
    QT_TRANSLATE_NOOP("ConfigureSystem", "Uruguay"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "US Virgin Islands"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Venezuela"),
    "",
    "",
    "",
    "",
    "",
    "",
    "", // 50-59
    "",
    "",
    "",
    "",
    QT_TRANSLATE_NOOP("ConfigureSystem", "Albania"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Australia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Austria"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Belgium"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Bosnia and Herzegovina"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Botswana"), // 60-69
    QT_TRANSLATE_NOOP("ConfigureSystem", "Bulgaria"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Croatia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Cyprus"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Czech Republic"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Denmark"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Estonia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Finland"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "France"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Germany"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Greece"), // 70-79
    QT_TRANSLATE_NOOP("ConfigureSystem", "Hungary"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Iceland"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Ireland"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Italy"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Latvia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Lesotho"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Liechtenstein"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Lithuania"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Luxembourg"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Macedonia"), // 80-89
    QT_TRANSLATE_NOOP("ConfigureSystem", "Malta"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Montenegro"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Mozambique"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Namibia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Netherlands"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "New Zealand"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Norway"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Poland"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Portugal"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Romania"), // 90-99
    QT_TRANSLATE_NOOP("ConfigureSystem", "Russia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Serbia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Slovakia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Slovenia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "South Africa"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Spain"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Swaziland"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Sweden"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Switzerland"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Turkey"), // 100-109
    QT_TRANSLATE_NOOP("ConfigureSystem", "United Kingdom"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Zambia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Zimbabwe"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Azerbaijan"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Mauritania"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Mali"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Niger"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Chad"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Sudan"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Eritrea"), // 110-119
    QT_TRANSLATE_NOOP("ConfigureSystem", "Djibouti"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Somalia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Andorra"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Gibraltar"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Guernsey"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Isle of Man"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Jersey"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Monaco"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Taiwan"),
    "", // 120-129
    "",
    "",
    "",
    "",
    "",
    "",
    QT_TRANSLATE_NOOP("ConfigureSystem", "South Korea"),
    "",
    "",
    "", // 130-139
    "",
    "",
    "",
    "",
    QT_TRANSLATE_NOOP("ConfigureSystem", "Hong Kong"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Macau"),
    "",
    "",
    "",
    "", // 140-149
    "",
    "",
    QT_TRANSLATE_NOOP("ConfigureSystem", "Indonesia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Singapore"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Thailand"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Philippines"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Malaysia"),
    "",
    "",
    "", // 150-159
    QT_TRANSLATE_NOOP("ConfigureSystem", "China"),
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    QT_TRANSLATE_NOOP("ConfigureSystem", "United Arab Emirates"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "India"), // 160-169
    QT_TRANSLATE_NOOP("ConfigureSystem", "Egypt"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Oman"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Qatar"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Kuwait"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Saudi Arabia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Syria"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Bahrain"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Jordan"),
    "",
    "", // 170-179
    "",
    "",
    "",
    "",
    QT_TRANSLATE_NOOP("ConfigureSystem", "San Marino"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Vatican City"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Bermuda"), // 180-186
};

ConfigureSystem::ConfigureSystem(Core::System& system_, QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureSystem>()), system{system_} {
    ui->setupUi(this);
    connect(ui->combo_birthmonth, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &ConfigureSystem::UpdateBirthdayComboBox);
    connect(ui->combo_init_clock, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &ConfigureSystem::UpdateInitTime);
    connect(ui->combo_init_ticks_type, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &ConfigureSystem::UpdateInitTicks);
    connect(ui->button_regenerate_console_id, &QPushButton::clicked, this,
            &ConfigureSystem::RefreshConsoleID);
    connect(ui->button_regenerate_mac, &QPushButton::clicked, this, &ConfigureSystem::RefreshMAC);
    connect(ui->button_unlink_console, &QPushButton::clicked, this,
            &ConfigureSystem::UnlinkConsole);
    connect(ui->combo_country, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                CheckCountryValid(static_cast<u8>(ui->combo_country->itemData(index).toInt()));
            });
    connect(ui->region_combobox, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this]([[maybe_unused]] int index) {
                CheckCountryValid(static_cast<u8>(ui->combo_country->currentData().toInt()));
            });

    connect(ui->button_secure_info, &QPushButton::clicked, this, [this] {
        ui->button_secure_info->setEnabled(false);
        const QString file_path_qtstr = QFileDialog::getOpenFileName(
            this, tr("Select SecureInfo_A/B"), QString(),
            QStringLiteral("SecureInfo_A/B (SecureInfo_A SecureInfo_B);;") + tr("All Files") +
                QStringLiteral(" (*.*)"));
        ui->button_secure_info->setEnabled(true);
        InstallSecureData(file_path_qtstr.toStdString(), HW::UniqueData::GetSecureInfoAPath());
    });
    connect(ui->button_friend_code_seed, &QPushButton::clicked, this, [this] {
        ui->button_friend_code_seed->setEnabled(false);
        const QString file_path_qtstr = QFileDialog::getOpenFileName(
            this, tr("Select LocalFriendCodeSeed_A/B"), QString(),
            QStringLiteral(
                "LocalFriendCodeSeed_A/B (LocalFriendCodeSeed_A LocalFriendCodeSeed_B);;") +
                tr("All Files") + QStringLiteral(" (*.*)"));
        ui->button_friend_code_seed->setEnabled(true);
        InstallSecureData(file_path_qtstr.toStdString(),
                          HW::UniqueData::GetLocalFriendCodeSeedBPath());
    });
    connect(ui->button_otp, &QPushButton::clicked, this, [this] {
        ui->button_otp->setEnabled(false);
        const QString file_path_qtstr =
            QFileDialog::getOpenFileName(this, tr("Select encrypted OTP file"), QString(),
                                         tr("Binary file") + QStringLiteral(" (*.bin);;") +
                                             tr("All Files") + QStringLiteral(" (*.*)"));
        ui->button_otp->setEnabled(true);
        InstallSecureData(file_path_qtstr.toStdString(), HW::UniqueData::GetOTPPath());
    });
    connect(ui->button_movable, &QPushButton::clicked, this, [this] {
        ui->button_movable->setEnabled(false);
        const QString file_path_qtstr =
            QFileDialog::getOpenFileName(this, tr("Select movable.sed"), QString(),
                                         tr("Sed file") + QStringLiteral(" (*.sed);;") +
                                             tr("All Files") + QStringLiteral(" (*.*)"));
        ui->button_movable->setEnabled(true);
        InstallSecureData(file_path_qtstr.toStdString(), HW::UniqueData::GetMovablePath());
    });

    for (u8 i = 0; i < country_names.size(); i++) {
        if (std::strcmp(country_names.at(i), "") != 0) {
            ui->combo_country->addItem(tr(country_names.at(i)), i);
        }
    }
    ui->label_country_invalid->setVisible(false);
    ui->label_country_invalid->setStyleSheet(QStringLiteral("QLabel { color: #ff3333; }"));

    SetupPerGameUI();
    ConfigureTime();
}

ConfigureSystem::~ConfigureSystem() = default;

void ConfigureSystem::SetConfiguration() {
    enabled = !system.IsPoweredOn();

    if (!Settings::IsConfiguringGlobal()) {
        ConfigurationShared::SetHighlight(ui->region_label,
                                          !Settings::values.region_value.UsingGlobal());
        const bool is_region_global = Settings::values.region_value.UsingGlobal();
        ui->region_combobox->setCurrentIndex(
            is_region_global ? ConfigurationShared::USE_GLOBAL_INDEX
                             : static_cast<int>(Settings::values.region_value.GetValue()) +
                                   ConfigurationShared::USE_GLOBAL_OFFSET + 1);
    } else {
        // The first item is "auto-select" with actual value -1, so plus one here will do the trick
        ui->region_combobox->setCurrentIndex(Settings::values.region_value.GetValue() + 1);
    }

    ui->apply_region_free_patch->setChecked(Settings::values.apply_region_free_patch.GetValue());

    ui->combo_init_clock->setCurrentIndex(static_cast<u8>(Settings::values.init_clock.GetValue()));
    QDateTime date_time;
    date_time.setSecsSinceEpoch(Settings::values.init_time.GetValue());
    ui->edit_init_time->setDateTime(date_time);

    s64 init_time_offset = Settings::values.init_time_offset.GetValue();
    int days_offset = static_cast<int>(init_time_offset / 86400);
    ui->edit_init_time_offset_days->setValue(days_offset);

    u64 time_offset = std::abs(init_time_offset) - std::abs(days_offset * 86400);
    QTime time = QTime::fromMSecsSinceStartOfDay(static_cast<int>(time_offset * 1000));
    ui->edit_init_time_offset_time->setTime(time);

    ui->combo_init_ticks_type->setCurrentIndex(
        static_cast<u8>(Settings::values.init_ticks_type.GetValue()));
    ui->edit_init_ticks_value->setText(
        QString::number(Settings::values.init_ticks_override.GetValue()));

    ui->spinBox_steps_per_hour->setValue(Settings::values.steps_per_hour.GetValue());

    cfg = Service::CFG::GetModule(system);
    ReadSystemSettings();

    ui->group_system_settings->setEnabled(enabled);
    ui->group_real_console_unique_data->setEnabled(enabled);
    if (enabled) {
        ui->label_disable_info->hide();
    }

    ui->toggle_new_3ds->setChecked(Settings::values.is_new_3ds.GetValue());
    ui->toggle_lle_applets->setChecked(Settings::values.lle_applets.GetValue());
    ui->enable_required_online_lle_modules->setChecked(
        Settings::values.enable_required_online_lle_modules.GetValue());
    ui->plugin_loader->setChecked(Settings::values.plugin_loader_enabled.GetValue());
    ui->allow_plugin_loader->setChecked(Settings::values.allow_plugin_loader.GetValue());
}

void ConfigureSystem::ReadSystemSettings() {
    // set username
    username = cfg->GetUsername();
    ui->edit_username->setText(QString::fromStdU16String(username));

    // set birthday
    std::tie(birthmonth, birthday) = cfg->GetBirthday();
    ui->combo_birthmonth->setCurrentIndex(birthmonth - 1);
    UpdateBirthdayComboBox(
        birthmonth -
        1); // explicitly update it because the signal from setCurrentIndex is not reliable
    ui->combo_birthday->setCurrentIndex(birthday - 1);

    // set system language
    language_index = cfg->GetSystemLanguage();
    ui->combo_language->setCurrentIndex(language_index);

    // set sound output mode
    sound_index = cfg->GetSoundOutputMode();
    ui->combo_sound->setCurrentIndex(sound_index);

    // set the country code
    country_code = cfg->GetCountryCode();
    ui->combo_country->setCurrentIndex(ui->combo_country->findData(country_code));
    CheckCountryValid(country_code);

    // set whether system setup is needed
    system_setup = cfg->IsSystemSetupNeeded();
    ui->toggle_system_setup->setChecked(system_setup);

    // set the console id
    u64 console_id = cfg->GetConsoleUniqueId();
    ui->label_console_id->setText(
        tr("Console ID: 0x%1").arg(QString::number(console_id, 16).toUpper()));
    mac_address = cfg->GetMacAddress();
    ui->label_mac->setText(tr("MAC: %1").arg(QString::fromStdString(mac_address)));

    // set play coin
    play_coin = Service::PTM::Module::GetPlayCoins();
    ui->spinBox_play_coins->setValue(play_coin);

    // Refresh secure data status
    RefreshSecureDataStatus();
}

void ConfigureSystem::ApplyConfiguration() {
    if (enabled) {
        ConfigurationShared::ApplyPerGameSetting(&Settings::values.region_value,
                                                 ui->region_combobox,
                                                 [](s32 index) { return index - 1; });

        bool modified = false;

        // apply username
        std::u16string new_username = ui->edit_username->text().toStdU16String();
        if (new_username != username) {
            cfg->SetUsername(new_username);
            modified = true;
        }

        // apply birthday
        s32 new_birthmonth = ui->combo_birthmonth->currentIndex() + 1;
        s32 new_birthday = ui->combo_birthday->currentIndex() + 1;
        if (birthmonth != new_birthmonth || birthday != new_birthday) {
            cfg->SetBirthday(static_cast<u8>(new_birthmonth), static_cast<u8>(new_birthday));
            modified = true;
        }

        // apply language
        s32 new_language = ui->combo_language->currentIndex();
        if (language_index != new_language) {
            cfg->SetSystemLanguage(static_cast<Service::CFG::SystemLanguage>(new_language));
            modified = true;
        }

        // apply sound
        s32 new_sound = ui->combo_sound->currentIndex();
        if (sound_index != new_sound) {
            cfg->SetSoundOutputMode(static_cast<Service::CFG::SoundOutputMode>(new_sound));
            modified = true;
        }

        // apply country
        u8 new_country = static_cast<u8>(ui->combo_country->currentData().toInt());
        if (country_code != new_country) {
            cfg->SetCountryCode(new_country);
            modified = true;
        }

        // apply whether system setup is needed
        bool new_system_setup = static_cast<u8>(ui->toggle_system_setup->isChecked());
        if (system_setup != new_system_setup) {
            cfg->SetSystemSetupNeeded(new_system_setup);
            modified = true;
        }

        // apply play coin
        u16 new_play_coin = static_cast<u16>(ui->spinBox_play_coins->value());
        if (play_coin != new_play_coin) {
            Service::PTM::Module::SetPlayCoins(new_play_coin);
        }

        // update the config savegame if any item is modified.
        if (modified) {
            cfg->UpdateConfigNANDSavegame();
        }

        ConfigurationShared::ApplyPerGameSetting(&Settings::values.is_new_3ds, ui->toggle_new_3ds,
                                                 is_new_3ds);
        ConfigurationShared::ApplyPerGameSetting(&Settings::values.lle_applets,
                                                 ui->toggle_lle_applets, lle_applets);
        ConfigurationShared::ApplyPerGameSetting(
            &Settings::values.enable_required_online_lle_modules,
            ui->enable_required_online_lle_modules, required_online_lle_modules);

        Settings::values.init_clock =
            static_cast<Settings::InitClock>(ui->combo_init_clock->currentIndex());
        Settings::values.init_time = ui->edit_init_time->dateTime().toSecsSinceEpoch();

        Settings::values.init_ticks_type =
            static_cast<Settings::InitTicks>(ui->combo_init_ticks_type->currentIndex());
        Settings::values.init_ticks_override =
            static_cast<s64>(ui->edit_init_ticks_value->text().toLongLong());

        Settings::values.steps_per_hour = static_cast<u16>(ui->spinBox_steps_per_hour->value());

        s64 time_offset_time = ui->edit_init_time_offset_time->time().msecsSinceStartOfDay() / 1000;
        s64 time_offset_days = ui->edit_init_time_offset_days->value() * 86400;

        if (time_offset_days < 0) {
            time_offset_time = -time_offset_time;
        }

        Settings::values.init_time_offset = time_offset_days + time_offset_time;
        Settings::values.is_new_3ds = ui->toggle_new_3ds->isChecked();
        Settings::values.lle_applets = ui->toggle_lle_applets->isChecked();
        Settings::values.enable_required_online_lle_modules =
            ui->enable_required_online_lle_modules->isChecked();
        Settings::values.apply_region_free_patch.SetValue(ui->apply_region_free_patch->isChecked());

        Settings::values.plugin_loader_enabled.SetValue(ui->plugin_loader->isChecked());
        Settings::values.allow_plugin_loader.SetValue(ui->allow_plugin_loader->isChecked());

        cfg->GetMacAddress() = mac_address;
        cfg->SaveMacAddress();
    }
}

void ConfigureSystem::UpdateBirthdayComboBox(int birthmonth_index) {
    if (birthmonth_index < 0 || birthmonth_index >= 12)
        return;

    // store current day selection
    s32 birthday_index = ui->combo_birthday->currentIndex();

    // get number of days in the new selected month
    s32 days = days_in_month[birthmonth_index];

    // if the selected day is out of range,
    // reset it to 1st
    if (birthday_index < 0 || birthday_index >= days)
        birthday_index = 0;

    // update the day combo box
    ui->combo_birthday->clear();
    for (s32 i = 1; i <= days; ++i) {
        ui->combo_birthday->addItem(QString::number(i));
    }

    // restore the day selection
    ui->combo_birthday->setCurrentIndex(birthday_index);
}

void ConfigureSystem::ConfigureTime() {
    QDateTime dt;
    dt.fromString(QStringLiteral("2000-01-01 00:00:01"), QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    ui->edit_init_time->setMinimumDateTime(dt);
    ui->edit_init_time->setCalendarPopup(true);

    SetConfiguration();

    UpdateInitTime(ui->combo_init_clock->currentIndex());
    UpdateInitTicks(ui->combo_init_ticks_type->currentIndex());
}

void ConfigureSystem::UpdateInitTime(int init_clock) {
    const bool is_global = Settings::IsConfiguringGlobal();
    const bool is_fixed_time =
        static_cast<Settings::InitClock>(init_clock) == Settings::InitClock::FixedTime;

    ui->label_init_time->setVisible(is_fixed_time && is_global);
    ui->edit_init_time->setVisible(is_fixed_time && is_global);

    ui->label_init_time_offset->setVisible(!is_fixed_time && is_global);
    ui->edit_init_time_offset_days->setVisible(!is_fixed_time && is_global);
    ui->edit_init_time_offset_time->setVisible(!is_fixed_time && is_global);
}

void ConfigureSystem::UpdateInitTicks(int init_ticks_type) {
    const bool is_global = Settings::IsConfiguringGlobal();
    const bool is_fixed =
        static_cast<Settings::InitTicks>(init_ticks_type) == Settings::InitTicks::Fixed;

    ui->label_init_ticks_value->setVisible(is_fixed && is_global);
    ui->edit_init_ticks_value->setVisible(is_fixed && is_global);
}

void ConfigureSystem::RefreshConsoleID() {
    ui->button_regenerate_console_id->setEnabled(false);
    QMessageBox::StandardButton reply;
    QString warning_text =
        tr("This will replace your current virtual 3DS console ID with a new one. "
           "Your current virtual 3DS console ID will not be recoverable. "
           "This might have unexpected effects in applications. This might fail "
           "if you use an outdated config save. Continue?");
    reply =
        QMessageBox::warning(this, tr("Warning"), warning_text, QMessageBox::No | QMessageBox::Yes);
    if (reply == QMessageBox::No) {
        ui->button_regenerate_console_id->setEnabled(true);
        return;
    }

    const auto [random_number, console_id] = cfg->GenerateConsoleUniqueId();
    cfg->SetConsoleUniqueId(random_number, console_id);
    cfg->UpdateConfigNANDSavegame();
    ui->label_console_id->setText(
        tr("Console ID: 0x%1").arg(QString::number(console_id, 16).toUpper()));
    ui->button_regenerate_console_id->setEnabled(true);
}

void ConfigureSystem::RefreshMAC() {
    ui->button_regenerate_mac->setEnabled(false);
    QMessageBox::StandardButton reply;
    QString warning_text = tr("This will replace your current MAC address with a new one. "
                              "It is not recommended to do this if you got the MAC address from "
                              "your real console using the setup tool. Continue?");
    reply =
        QMessageBox::warning(this, tr("Warning"), warning_text, QMessageBox::No | QMessageBox::Yes);
    if (reply == QMessageBox::No) {
        ui->button_regenerate_mac->setEnabled(true);
        return;
    }

    mac_address = Service::CFG::GenerateRandomMAC();
    ui->label_mac->setText(tr("MAC: %1").arg(QString::fromStdString(mac_address)));
    ui->button_regenerate_mac->setEnabled(true);
}

void ConfigureSystem::UnlinkConsole() {
    ui->button_unlink_console->setEnabled(false);
    QMessageBox::StandardButton reply;
    QString warning_text =
        tr("This action will unlink your real console from Azahar, with the following "
           "consequences:<br><ul><li>Your OTP, SecureInfo and LocalFriendCodeSeed will be removed "
           "from Azahar.</li><li>Your friend list will reset and you will be logged out of your "
           "NNID/PNID account.</li><li>System files and eshop titles obtained through Azahar will "
           "become inaccessible until the same console is linked again (save data will not be "
           "lost).</li></ul><br>Continue?");
    reply =
        QMessageBox::warning(this, tr("Warning"), warning_text, QMessageBox::No | QMessageBox::Yes);
    if (reply == QMessageBox::No) {
        ui->button_unlink_console->setEnabled(true);
        return;
    }

    HW::UniqueData::UnlinkConsole();
    RefreshSecureDataStatus();
    ui->button_unlink_console->setEnabled(true);
}

void ConfigureSystem::CheckCountryValid(u8 country) {
    // TODO(PabloMK7): Make this per-game compatible
    if (!Settings::IsConfiguringGlobal())
        return;

    s32 region = ui->region_combobox->currentIndex() - 1;
    QString label_text;

    if (region != Settings::REGION_VALUE_AUTO_SELECT &&
        !cfg->IsValidRegionCountry(static_cast<u32>(region), country)) {
        label_text = tr("Invalid country for configured region");
    }
    if (HW::UniqueData::GetSecureInfoA().IsValid()) {
        region = static_cast<u32>(cfg->GetRegionValue(true));
        if (!cfg->IsValidRegionCountry(static_cast<u32>(region), country)) {
            if (!label_text.isEmpty()) {
                label_text += QString::fromStdString("\n");
            }
            label_text += tr("Invalid country for console unique data");
        }
    }

    ui->label_country_invalid->setText(label_text);
    ui->label_country_invalid->setVisible(!label_text.isEmpty());
}

void ConfigureSystem::InstallSecureData(const std::string& from_path, const std::string& to_path) {
    std::string from =
        FileUtil::SanitizePath(from_path, FileUtil::DirectorySeparator::PlatformDefault);
    std::string to = FileUtil::SanitizePath(to_path, FileUtil::DirectorySeparator::PlatformDefault);
    if (from.empty() || from == to) {
        return;
    }
    FileUtil::CreateFullPath(to);
    FileUtil::Copy(from, to);
    HW::UniqueData::InvalidateSecureData();
    RefreshSecureDataStatus();
}

void ConfigureSystem::RefreshSecureDataStatus() {
    auto status_to_str = [](HW::UniqueData::SecureDataLoadStatus status) {
        switch (status) {
        case HW::UniqueData::SecureDataLoadStatus::Loaded:
            return tr("Status: Loaded");
        case HW::UniqueData::SecureDataLoadStatus::InvalidSignature:
            return tr("Status: Loaded (Invalid Signature)");
        case HW::UniqueData::SecureDataLoadStatus::RegionChanged:
            return tr("Status: Loaded (Region Changed)");
        case HW::UniqueData::SecureDataLoadStatus::CannotValidateSignature:
            return tr("Status: Loaded (Cannot Validate Signature)");
        case HW::UniqueData::SecureDataLoadStatus::NotFound:
            return tr("Status: Not Found");
        case HW::UniqueData::SecureDataLoadStatus::Invalid:
            return tr("Status: Invalid");
        case HW::UniqueData::SecureDataLoadStatus::IOError:
            return tr("Status: IO Error");
        case HW::UniqueData::SecureDataLoadStatus::NoCryptoKeys:
            return tr("Status: Missing Crypto Keys");
        default:
            return QString();
        }
    };

    ui->label_secure_info_status->setText(status_to_str(HW::UniqueData::LoadSecureInfoA()));
    ui->label_friend_code_seed_status->setText(
        status_to_str(HW::UniqueData::LoadLocalFriendCodeSeedB()));
    ui->label_otp_status->setText(status_to_str(HW::UniqueData::LoadOTP()));
    ui->label_movable_status->setText(status_to_str(HW::UniqueData::LoadMovable()));

    if (HW::UniqueData::IsFullConsoleLinked()) {
        ui->linked_console->setVisible(true);
        ui->button_otp->setEnabled(false);
        ui->button_secure_info->setEnabled(false);
        ui->button_friend_code_seed->setEnabled(false);
    } else {
        ui->linked_console->setVisible(false);
    }
}

void ConfigureSystem::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureSystem::SetupPerGameUI() {
    // Block the global settings if a game is currently running that overrides them
    if (Settings::IsConfiguringGlobal()) {
        ui->toggle_new_3ds->setEnabled(Settings::values.is_new_3ds.UsingGlobal());
        ui->toggle_lle_applets->setEnabled(Settings::values.lle_applets.UsingGlobal());
        ui->enable_required_online_lle_modules->setEnabled(
            Settings::values.enable_required_online_lle_modules.UsingGlobal());
        ui->region_combobox->setEnabled(Settings::values.region_value.UsingGlobal());
        return;
    }

    // Hide most settings for now, we can implement them later
    ui->apply_region_free_patch->setVisible(false);
    ui->label_username->setVisible(false);
    ui->label_birthday->setVisible(false);
    ui->label_init_clock->setVisible(false);
    ui->label_init_time->setVisible(false);
    ui->label_init_ticks_type->setVisible(false);
    ui->label_init_ticks_value->setVisible(false);
    ui->label_console_id->setVisible(false);
    ui->label_mac->setVisible(false);
    ui->label_sound->setVisible(false);
    ui->label_language->setVisible(false);
    ui->label_country->setVisible(false);
    ui->label_play_coins->setVisible(false);
    ui->label_steps_per_hour->setVisible(false);
    ui->edit_username->setVisible(false);
    ui->spinBox_play_coins->setVisible(false);
    ui->spinBox_steps_per_hour->setVisible(false);
    ui->combo_birthday->setVisible(false);
    ui->combo_birthmonth->setVisible(false);
    ui->combo_init_clock->setVisible(false);
    ui->combo_init_ticks_type->setVisible(false);
    ui->combo_sound->setVisible(false);
    ui->combo_language->setVisible(false);
    ui->combo_country->setVisible(false);
    ui->label_init_time_offset->setVisible(false);
    ui->edit_init_time_offset_days->setVisible(false);
    ui->edit_init_time_offset_time->setVisible(false);
    ui->edit_init_ticks_value->setVisible(false);
    ui->toggle_system_setup->setVisible(false);
    ui->button_regenerate_console_id->setVisible(false);
    ui->button_regenerate_mac->setVisible(false);
    // Apps can change the state of the plugin loader, so plugins load
    // to a chainloaded app with specific parameters. Don't allow
    // the plugin loader state to be configured per-game as it may
    // mess things up.
    ui->label_plugin_loader->setVisible(false);
    ui->plugin_loader->setVisible(false);
    ui->allow_plugin_loader->setVisible(false);
    ui->group_real_console_unique_data->setVisible(false);

    ConfigurationShared::SetColoredTristate(ui->toggle_new_3ds, Settings::values.is_new_3ds,
                                            is_new_3ds);
    ConfigurationShared::SetColoredTristate(ui->toggle_lle_applets, Settings::values.lle_applets,
                                            lle_applets);
    ConfigurationShared::SetColoredTristate(ui->enable_required_online_lle_modules,
                                            Settings::values.enable_required_online_lle_modules,
                                            required_online_lle_modules);
    ConfigurationShared::SetColoredComboBox(
        ui->region_combobox, ui->region_label,
        static_cast<u32>(Settings::values.region_value.GetValue(true) + 1));
}
