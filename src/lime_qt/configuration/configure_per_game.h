// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <span>
#include <string>
#include <QDialog>
#include <QList>
#include <QString>
#include "lime_qt/configuration/config.h"

namespace Core {
class System;
}

class ConfigureAudio;
class ConfigureGeneral;
class ConfigureEnhancements;
class ConfigureLayout;
class ConfigureGraphics;
class ConfigureSystem;
class ConfigureDebug;
class ConfigureCheats;

class QGraphicsScene;
class QStandardItem;
class QStandardItemModel;
class QTreeView;
class QVBoxLayout;

namespace Ui {
class ConfigurePerGame;
}

class ConfigurePerGame : public QDialog {
    Q_OBJECT

public:
    explicit ConfigurePerGame(QWidget* parent, u64 title_id_, const QString& file_name,
                              QString gl_renderer, std::span<const QString> physical_devices,
                              Core::System& system_);
    ~ConfigurePerGame() override;

    /// Loads all button configurations to settings file
    void LoadConfiguration();

    /// Save all button configurations to settings file
    void ApplyConfiguration();

    /// Reset the settings for this game
    void ResetDefaults();

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();
    void HandleAcceptedEvent();

    void HandleApplyButtonClicked();

    std::unique_ptr<Ui::ConfigurePerGame> ui;
    std::string filename;
    u64 title_id;

    QGraphicsScene* scene;

    std::unique_ptr<QtConfig> game_config;

    Core::System& system;

    std::unique_ptr<ConfigureAudio> audio_tab;
    std::unique_ptr<ConfigureGeneral> general_tab;
    std::unique_ptr<ConfigureEnhancements> enhancements_tab;
    std::unique_ptr<ConfigureLayout> layout_tab;
    std::unique_ptr<ConfigureGraphics> graphics_tab;
    std::unique_ptr<ConfigureSystem> system_tab;
    std::unique_ptr<ConfigureDebug> debug_tab;
    std::unique_ptr<ConfigureCheats> cheat_tab;
};
