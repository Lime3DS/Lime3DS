// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QIcon>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrentRun>
#include "core/telemetry_session.h"
#include "lime_qt/configuration/configure_web.h"
#include "lime_qt/uisettings.h"
#include "network/network_settings.h"
#include "ui_configure_web.h"

ConfigureWeb::ConfigureWeb(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureWeb>()) {
    ui->setupUi(this);

#ifndef USE_DISCORD_PRESENCE
    ui->discord_group->setEnabled(false);
#endif
    SetConfiguration();
}

ConfigureWeb::~ConfigureWeb() = default;

void ConfigureWeb::SetConfiguration() {
    ui->toggle_discordrpc->setChecked(UISettings::values.enable_discord_presence.GetValue());
}

void ConfigureWeb::ApplyConfiguration() {
    UISettings::values.enable_discord_presence = ui->toggle_discordrpc->isChecked();
}

void ConfigureWeb::RetranslateUI() {
    ui->retranslateUi(this);
}
