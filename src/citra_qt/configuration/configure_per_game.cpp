// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <utility>
#include <vector>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <fmt/format.h>
#include "citra_qt/configuration/config.h"
#include "citra_qt/configuration/configure_audio.h"
#include "citra_qt/configuration/configure_cheats.h"
#include "citra_qt/configuration/configure_debug.h"
#include "citra_qt/configuration/configure_enhancements.h"
#include "citra_qt/configuration/configure_general.h"
#include "citra_qt/configuration/configure_graphics.h"
#include "citra_qt/configuration/configure_per_game.h"
#include "citra_qt/configuration/configure_system.h"
#include "citra_qt/util/util.h"
#include "common/file_util.h"
#include "core/core.h"
#include "core/loader/loader.h"
#include "core/loader/smdh.h"
#include "ui_configure_per_game.h"

ConfigurePerGame::ConfigurePerGame(QWidget* parent, u64 title_id_, const QString& file_name,
                                   QString gl_renderer, std::span<const QString> physical_devices,
                                   Core::System& system_)
    : QDialog(parent), ui(std::make_unique<Ui::ConfigurePerGame>()),
      filename{file_name.toStdString()}, title_id{title_id_}, system{system_} {
    const auto config_file_name = title_id == 0 ? std::string(FileUtil::GetFilename(filename))
                                                : fmt::format("{:016X}", title_id);
    game_config = std::make_unique<Config>(config_file_name, Config::ConfigType::PerGameConfig);

    const bool is_powered_on = system.IsPoweredOn();
    audio_tab = std::make_unique<ConfigureAudio>(is_powered_on, this);
    general_tab = std::make_unique<ConfigureGeneral>(this);
    enhancements_tab = std::make_unique<ConfigureEnhancements>(this);
    graphics_tab =
        std::make_unique<ConfigureGraphics>(gl_renderer, physical_devices, is_powered_on, this);
    system_tab = std::make_unique<ConfigureSystem>(system, this);
    debug_tab = std::make_unique<ConfigureDebug>(is_powered_on, this);
    cheat_tab = std::make_unique<ConfigureCheats>(system.CheatEngine(), title_id, this);

    ui->setupUi(this);

    ui->tabWidget->addTab(general_tab.get(), tr("General"));
    ui->tabWidget->addTab(system_tab.get(), tr("System"));
    ui->tabWidget->addTab(enhancements_tab.get(), tr("Enhancements"));
    ui->tabWidget->addTab(graphics_tab.get(), tr("Graphics"));
    ui->tabWidget->addTab(audio_tab.get(), tr("Audio"));
    ui->tabWidget->addTab(debug_tab.get(), tr("Debug"));
    ui->tabWidget->addTab(cheat_tab.get(), tr("Cheats"));

    setFocusPolicy(Qt::ClickFocus);
    setWindowTitle(tr("Properties"));

    scene = new QGraphicsScene;
    ui->icon_view->setScene(scene);

    if (system.IsPoweredOn()) {
        QPushButton* apply_button = ui->buttonBox->addButton(QDialogButtonBox::Apply);
        connect(apply_button, &QAbstractButton::clicked, this,
                &ConfigurePerGame::HandleApplyButtonClicked);
    }

    connect(ui->button_reset_per_game, &QPushButton::clicked, this,
            &ConfigurePerGame::ResetDefaults);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this,
            &ConfigurePerGame::HandleAcceptedEvent);

    LoadConfiguration();
}

ConfigurePerGame::~ConfigurePerGame() = default;

void ConfigurePerGame::ResetDefaults() {
    const auto config_file_name = title_id == 0 ? filename : fmt::format("{:016X}", title_id);
    QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Citra"), tr("Are you sure you want to <b>reset your settings for this game</b>?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer == QMessageBox::No) {
        return;
    }

    FileUtil::Delete(fmt::format("{}/custom/{}.ini",
                                 FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir),
                                 config_file_name));
    close();
}

void ConfigurePerGame::HandleAcceptedEvent() {
    if (ui->tabWidget->currentWidget() == cheat_tab.get()) {
        cheat_tab->ApplyConfiguration();
    }
    accept();
}

void ConfigurePerGame::ApplyConfiguration() {
    general_tab->ApplyConfiguration();
    system_tab->ApplyConfiguration();
    enhancements_tab->ApplyConfiguration();
    graphics_tab->ApplyConfiguration();
    audio_tab->ApplyConfiguration();
    debug_tab->ApplyConfiguration();

    system.ApplySettings();
    Settings::LogSettings();

    game_config->Save();
}

void ConfigurePerGame::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QDialog::changeEvent(event);
}

void ConfigurePerGame::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigurePerGame::HandleApplyButtonClicked() {
    ApplyConfiguration();
    if (ui->tabWidget->currentWidget() == cheat_tab.get()) {
        cheat_tab->ApplyConfiguration();
    }
}

static QPixmap GetQPixmapFromSMDH(std::vector<u8>& smdh_data) {
    Loader::SMDH smdh;
    std::memcpy(&smdh, smdh_data.data(), sizeof(Loader::SMDH));

    bool large = true;
    std::vector<u16> icon_data = smdh.GetIcon(large);
    const uchar* data = reinterpret_cast<const uchar*>(icon_data.data());
    int size = large ? 48 : 24;
    QImage icon(data, size, size, QImage::Format::Format_RGB16);
    return QPixmap::fromImage(icon);
}

void ConfigurePerGame::LoadConfiguration() {
    if (filename.empty()) {
        return;
    }

    ui->display_title_id->setText(
        QStringLiteral("%1").arg(title_id, 16, 16, QLatin1Char{'0'}).toUpper());

    const auto loader = Loader::GetLoader(filename);

    std::string title;
    if (loader->ReadTitle(title) == Loader::ResultStatus::Success)
        ui->display_name->setText(QString::fromStdString(title));

    std::vector<u8> bytes;
    if (loader->ReadIcon(bytes) == Loader::ResultStatus::Success) {
        scene->clear();

        QPixmap map = GetQPixmapFromSMDH(bytes);
        scene->addPixmap(map.scaled(ui->icon_view->width(), ui->icon_view->height(),
                                    Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }

    ui->display_filepath->setText(QString::fromStdString(filename));

    ui->display_format->setText(
        QString::fromStdString(Loader::GetFileTypeString(loader->GetFileType())));

    const auto valueText = ReadableByteSize(FileUtil::GetSize(filename));
    ui->display_size->setText(valueText);
}
