// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <span>
#include <QDialog>
#include <QString>

class HotkeyRegistry;

namespace Ui {
class ConfigureDialog;
}

namespace Core {
class System;
}

class ConfigureGeneral;
class ConfigureSystem;
class ConfigureInput;
class ConfigureHotkeys;
class ConfigureGraphics;
class ConfigureEnhancements;
class ConfigureAudio;
class ConfigureCamera;
class ConfigureDebug;
class ConfigureStorage;
class ConfigureWeb;
class ConfigureUi;

class ConfigureDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConfigureDialog(QWidget* parent, HotkeyRegistry& registry, Core::System& system,
                             QString gl_renderer, std::span<const QString> physical_devices,
                             bool enable_web_config = true);
    ~ConfigureDialog() override;

    void ApplyConfiguration();

private slots:
    void OnLanguageChanged(const QString& locale);

signals:
    void LanguageChanged(const QString& locale);

private:
    void SetConfiguration();
    void RetranslateUI();
    void UpdateVisibleTabs();
    void PopulateSelectionList();

    std::unique_ptr<Ui::ConfigureDialog> ui;
    HotkeyRegistry& registry;
    Core::System& system;
    bool is_powered_on;

    std::unique_ptr<ConfigureGeneral> general_tab;
    std::unique_ptr<ConfigureSystem> system_tab;
    std::unique_ptr<ConfigureInput> input_tab;
    std::unique_ptr<ConfigureHotkeys> hotkeys_tab;
    std::unique_ptr<ConfigureGraphics> graphics_tab;
    std::unique_ptr<ConfigureEnhancements> enhancements_tab;
    std::unique_ptr<ConfigureAudio> audio_tab;
    std::unique_ptr<ConfigureCamera> camera_tab;
    std::unique_ptr<ConfigureDebug> debug_tab;
    std::unique_ptr<ConfigureStorage> storage_tab;
    std::unique_ptr<ConfigureWeb> web_tab;
    std::unique_ptr<ConfigureUi> ui_tab;
};
