// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <QFutureWatcher>
#include <QMessageBox>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrentMap>
#include "citra_qt/configuration/configuration_shared.h"
#include "citra_qt/configuration/configure_system.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hw/aes/key.h"
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
    connect(ui->button_start_download, &QPushButton::clicked, this,
            &ConfigureSystem::DownloadFromNUS);
    for (u8 i = 0; i < country_names.size(); i++) {
        if (std::strcmp(country_names.at(i), "") != 0) {
            ui->combo_country->addItem(tr(country_names.at(i)), i);
        }
    }

    SetupPerGameUI();

    ui->combo_download_set->setCurrentIndex(0);    // set to Minimal
    ui->combo_download_region->setCurrentIndex(0); // set to the base region

    HW::AES::InitKeys(true);
    bool keys_available = HW::AES::IsKeyXAvailable(HW::AES::KeySlotID::NCCHSecure1) &&
                          HW::AES::IsKeyXAvailable(HW::AES::KeySlotID::NCCHSecure2);
    for (u8 i = 0; i < HW::AES::MaxCommonKeySlot && keys_available; i++) {
        HW::AES::SelectCommonKeyIndex(i);
        if (!HW::AES::IsNormalKeyAvailable(HW::AES::KeySlotID::TicketCommonKey)) {
            keys_available = false;
            break;
        }
    }
    if (keys_available) {
        ui->button_start_download->setEnabled(true);
        ui->combo_download_set->setEnabled(true);
        ui->combo_download_region->setEnabled(true);
        ui->label_nus_download->setText(tr("Download System Files from Nintendo servers"));
    } else {
        ui->button_start_download->setEnabled(false);
        ui->combo_download_set->setEnabled(false);
        ui->combo_download_region->setEnabled(false);
        ui->label_nus_download->setTextInteractionFlags(Qt::TextBrowserInteraction);
        ui->label_nus_download->setOpenExternalLinks(true);
        ui->label_nus_download->setText(
            tr("Citra is missing keys to download system files. <br><a "
               "href='https://citra-emu.org/wiki/aes-keys/'><span style=\"text-decoration: "
               "underline; color:#039be5;\">How to get keys?</span></a>"));
    }

    ConfigureTime();
}

ConfigureSystem::~ConfigureSystem() = default;

void ConfigureSystem::SetConfiguration() {
    enabled = !system.IsPoweredOn();

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

    cfg = Service::CFG::GetModule(system);
    ReadSystemSettings();

    ui->group_system_settings->setEnabled(enabled);
    if (enabled) {
        ui->label_disable_info->hide();
    }

    ui->toggle_new_3ds->setChecked(Settings::values.is_new_3ds.GetValue());
    ui->toggle_lle_applets->setChecked(Settings::values.lle_applets.GetValue());
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

    // set whether system setup is needed
    system_setup = cfg->IsSystemSetupNeeded();
    ui->toggle_system_setup->setChecked(system_setup);

    // set the console id
    u64 console_id = cfg->GetConsoleUniqueId();
    ui->label_console_id->setText(
        tr("Console ID: 0x%1").arg(QString::number(console_id, 16).toUpper()));

    // set play coin
    play_coin = Service::PTM::Module::GetPlayCoins();
    ui->spinBox_play_coins->setValue(play_coin);

    // set firmware download region
    ui->combo_download_region->setCurrentIndex(static_cast<int>(cfg->GetRegionValue()));
}

void ConfigureSystem::ApplyConfiguration() {
    if (enabled) {
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
            cfg->SetBirthday(new_birthmonth, new_birthday);
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

        Settings::values.init_clock =
            static_cast<Settings::InitClock>(ui->combo_init_clock->currentIndex());
        Settings::values.init_time = ui->edit_init_time->dateTime().toSecsSinceEpoch();

        Settings::values.init_ticks_type =
            static_cast<Settings::InitTicks>(ui->combo_init_ticks_type->currentIndex());
        Settings::values.init_ticks_override =
            static_cast<s64>(ui->edit_init_ticks_value->text().toLongLong());

        s64 time_offset_time = ui->edit_init_time_offset_time->time().msecsSinceStartOfDay() / 1000;
        s64 time_offset_days = ui->edit_init_time_offset_days->value() * 86400;

        if (time_offset_days < 0) {
            time_offset_time = -time_offset_time;
        }

        Settings::values.init_time_offset = time_offset_days + time_offset_time;
        Settings::values.is_new_3ds = ui->toggle_new_3ds->isChecked();
        Settings::values.lle_applets = ui->toggle_lle_applets->isChecked();

        Settings::values.plugin_loader_enabled.SetValue(ui->plugin_loader->isChecked());
        Settings::values.allow_plugin_loader.SetValue(ui->allow_plugin_loader->isChecked());
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
    QMessageBox::StandardButton reply;
    QString warning_text = tr("This will replace your current virtual 3DS with a new one. "
                              "Your current virtual 3DS will not be recoverable. "
                              "This might have unexpected effects in games. This might fail, "
                              "if you use an outdated config savegame. Continue?");
    reply = QMessageBox::critical(this, tr("Warning"), warning_text,
                                  QMessageBox::No | QMessageBox::Yes);
    if (reply == QMessageBox::No) {
        return;
    }

    const auto [random_number, console_id] = cfg->GenerateConsoleUniqueId();
    cfg->SetConsoleUniqueId(random_number, console_id);
    cfg->UpdateConfigNANDSavegame();
    ui->label_console_id->setText(
        tr("Console ID: 0x%1").arg(QString::number(console_id, 16).toUpper()));
}

void ConfigureSystem::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureSystem::SetupPerGameUI() {
    // Block the global settings if a game is currently running that overrides them
    if (Settings::IsConfiguringGlobal()) {
        ui->toggle_new_3ds->setEnabled(Settings::values.is_new_3ds.UsingGlobal());
        ui->toggle_lle_applets->setEnabled(Settings::values.lle_applets.UsingGlobal());
        return;
    }

    // Hide most settings for now, we can implement them later
    ui->label_username->setVisible(false);
    ui->label_birthday->setVisible(false);
    ui->label_init_clock->setVisible(false);
    ui->label_init_time->setVisible(false);
    ui->label_init_ticks_type->setVisible(false);
    ui->label_init_ticks_value->setVisible(false);
    ui->label_console_id->setVisible(false);
    ui->label_sound->setVisible(false);
    ui->label_language->setVisible(false);
    ui->label_country->setVisible(false);
    ui->label_play_coins->setVisible(false);
    ui->edit_username->setVisible(false);
    ui->spinBox_play_coins->setVisible(false);
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
    // Apps can change the state of the plugin loader, so plugins load
    // to a chainloaded app with specific parameters. Don't allow
    // the plugin loader state to be configured per-game as it may
    // mess things up.
    ui->label_plugin_loader->setVisible(false);
    ui->plugin_loader->setVisible(false);
    ui->allow_plugin_loader->setVisible(false);
    // Disable the system firmware downloader.
    ui->label_nus_download->setVisible(false);
    ui->body_nus_download->setVisible(false);

    ConfigurationShared::SetColoredTristate(ui->toggle_new_3ds, Settings::values.is_new_3ds,
                                            is_new_3ds);
    ConfigurationShared::SetColoredTristate(ui->toggle_lle_applets, Settings::values.lle_applets,
                                            lle_applets);
}

void ConfigureSystem::DownloadFromNUS() {
    ui->button_start_download->setEnabled(false);

    const auto mode =
        static_cast<Core::SystemTitleSet>(1 << ui->combo_download_set->currentIndex());
    const auto region = static_cast<u32>(ui->combo_download_region->currentIndex());
    const std::vector<u64> titles = Core::GetSystemTitleIds(mode, region);

    QProgressDialog progress(tr("Downloading files..."), tr("Cancel"), 0,
                             static_cast<int>(titles.size()), this);
    progress.setWindowModality(Qt::WindowModal);

    QFutureWatcher<void> future_watcher;
    QObject::connect(&future_watcher, &QFutureWatcher<void>::finished, &progress,
                     &QProgressDialog::reset);
    QObject::connect(&progress, &QProgressDialog::canceled, &future_watcher,
                     &QFutureWatcher<void>::cancel);
    QObject::connect(&future_watcher, &QFutureWatcher<void>::progressValueChanged, &progress,
                     &QProgressDialog::setValue);

    auto failed = false;
    const auto download_title = [&future_watcher, &failed](const u64& title_id) {
        if (Service::AM::InstallFromNus(title_id) != Service::AM::InstallStatus::Success) {
            failed = true;
            future_watcher.cancel();
        }
    };

    future_watcher.setFuture(QtConcurrent::map(titles, download_title));
    progress.exec();
    future_watcher.waitForFinished();

    if (failed) {
        QMessageBox::critical(this, tr("Citra"), tr("Downloading system files failed."));
    } else if (!future_watcher.isCanceled()) {
        QMessageBox::information(this, tr("Citra"), tr("Successfully downloaded system files."));
    }

    ui->button_start_download->setEnabled(true);
}
