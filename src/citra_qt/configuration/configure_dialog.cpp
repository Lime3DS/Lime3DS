// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <QListWidgetItem>
#include "citra_qt/configuration/configure_audio.h"
#include "citra_qt/configuration/configure_camera.h"
#include "citra_qt/configuration/configure_debug.h"
#include "citra_qt/configuration/configure_dialog.h"
#include "citra_qt/configuration/configure_enhancements.h"
#include "citra_qt/configuration/configure_general.h"
#include "citra_qt/configuration/configure_graphics.h"
#include "citra_qt/configuration/configure_hotkeys.h"
#include "citra_qt/configuration/configure_input.h"
#include "citra_qt/configuration/configure_storage.h"
#include "citra_qt/configuration/configure_system.h"
#include "citra_qt/configuration/configure_ui.h"
#include "citra_qt/configuration/configure_web.h"
#include "citra_qt/hotkeys.h"
#include "common/settings.h"
#include "core/core.h"
#include "ui_configure.h"

ConfigureDialog::ConfigureDialog(QWidget* parent, HotkeyRegistry& registry_, Core::System& system_,
                                 QString gl_renderer, std::span<const QString> physical_devices,
                                 bool enable_web_config)
    : QDialog(parent), ui{std::make_unique<Ui::ConfigureDialog>()}, registry{registry_},
      system{system_}, is_powered_on{system.IsPoweredOn()},
      general_tab{std::make_unique<ConfigureGeneral>(this)},
      system_tab{std::make_unique<ConfigureSystem>(system, this)},
      input_tab{std::make_unique<ConfigureInput>(this)},
      hotkeys_tab{std::make_unique<ConfigureHotkeys>(this)},
      graphics_tab{
          std::make_unique<ConfigureGraphics>(gl_renderer, physical_devices, is_powered_on, this)},
      enhancements_tab{std::make_unique<ConfigureEnhancements>(this)},
      audio_tab{std::make_unique<ConfigureAudio>(is_powered_on, this)},
      camera_tab{std::make_unique<ConfigureCamera>(this)},
      debug_tab{std::make_unique<ConfigureDebug>(is_powered_on, this)},
      storage_tab{std::make_unique<ConfigureStorage>(is_powered_on, this)},
      web_tab{std::make_unique<ConfigureWeb>(this)}, ui_tab{std::make_unique<ConfigureUi>(this)} {
    Settings::SetConfiguringGlobal(true);

    ui->setupUi(this);

    ui->tabWidget->addTab(general_tab.get(), tr("General"));
    ui->tabWidget->addTab(system_tab.get(), tr("System"));
    ui->tabWidget->addTab(input_tab.get(), tr("Input"));
    ui->tabWidget->addTab(hotkeys_tab.get(), tr("Hotkeys"));
    ui->tabWidget->addTab(graphics_tab.get(), tr("Graphics"));
    ui->tabWidget->addTab(enhancements_tab.get(), tr("Enhancements"));
    ui->tabWidget->addTab(audio_tab.get(), tr("Audio"));
    ui->tabWidget->addTab(camera_tab.get(), tr("Camera"));
    ui->tabWidget->addTab(debug_tab.get(), tr("Debug"));
    ui->tabWidget->addTab(storage_tab.get(), tr("Storage"));
    ui->tabWidget->addTab(web_tab.get(), tr("Web"));
    ui->tabWidget->addTab(ui_tab.get(), tr("UI"));

    hotkeys_tab->Populate(registry);
    web_tab->SetWebServiceConfigEnabled(enable_web_config);

    PopulateSelectionList();

    connect(ui_tab.get(), &ConfigureUi::LanguageChanged, this, &ConfigureDialog::OnLanguageChanged);
    connect(ui->selectorList, &QListWidget::itemSelectionChanged, this,
            &ConfigureDialog::UpdateVisibleTabs);

    adjustSize();
    ui->selectorList->setCurrentRow(0);

    // Set up used key list synchronisation
    connect(input_tab.get(), &ConfigureInput::InputKeysChanged, hotkeys_tab.get(),
            &ConfigureHotkeys::OnInputKeysChanged);
    connect(hotkeys_tab.get(), &ConfigureHotkeys::HotkeysChanged, input_tab.get(),
            &ConfigureInput::OnHotkeysChanged);

    // Synchronise lists upon initialisation
    input_tab->EmitInputKeysChanged();
    hotkeys_tab->EmitHotkeysChanged();
}

ConfigureDialog::~ConfigureDialog() = default;

void ConfigureDialog::SetConfiguration() {
    general_tab->SetConfiguration();
    system_tab->SetConfiguration();
    input_tab->LoadConfiguration();
    graphics_tab->SetConfiguration();
    enhancements_tab->SetConfiguration();
    audio_tab->SetConfiguration();
    camera_tab->SetConfiguration();
    debug_tab->SetConfiguration();
    web_tab->SetConfiguration();
    ui_tab->SetConfiguration();
    storage_tab->SetConfiguration();
}

void ConfigureDialog::ApplyConfiguration() {
    general_tab->ApplyConfiguration();
    system_tab->ApplyConfiguration();
    input_tab->ApplyConfiguration();
    input_tab->ApplyProfile();
    hotkeys_tab->ApplyConfiguration(registry);
    graphics_tab->ApplyConfiguration();
    enhancements_tab->ApplyConfiguration();
    audio_tab->ApplyConfiguration();
    camera_tab->ApplyConfiguration();
    debug_tab->ApplyConfiguration();
    web_tab->ApplyConfiguration();
    ui_tab->ApplyConfiguration();
    storage_tab->ApplyConfiguration();
    system.ApplySettings();
    Settings::LogSettings();
}

Q_DECLARE_METATYPE(QList<QWidget*>);

void ConfigureDialog::PopulateSelectionList() {
    ui->selectorList->clear();

    const std::array<std::pair<QString, QList<QWidget*>>, 5> items{
        {{tr("General"), {general_tab.get(), web_tab.get(), debug_tab.get(), ui_tab.get()}},
         {tr("System"), {system_tab.get(), camera_tab.get(), storage_tab.get()}},
         {tr("Graphics"), {enhancements_tab.get(), graphics_tab.get()}},
         {tr("Audio"), {audio_tab.get()}},
         {tr("Controls"), {input_tab.get(), hotkeys_tab.get()}}}};

    for (const auto& entry : items) {
        auto* const item = new QListWidgetItem(entry.first);
        item->setData(Qt::UserRole, QVariant::fromValue(entry.second));

        ui->selectorList->addItem(item);
    }
}

void ConfigureDialog::OnLanguageChanged(const QString& locale) {
    emit LanguageChanged(locale);
    // first apply the configuration, and then restore the display
    ApplyConfiguration();
    RetranslateUI();
    SetConfiguration();
}

void ConfigureDialog::RetranslateUI() {
    int old_row = ui->selectorList->currentRow();
    int old_index = ui->tabWidget->currentIndex();
    ui->retranslateUi(this);
    PopulateSelectionList();
    // restore selection after repopulating
    ui->selectorList->setCurrentRow(old_row);
    ui->tabWidget->setCurrentIndex(old_index);

    general_tab->RetranslateUI();
    system_tab->RetranslateUI();
    input_tab->RetranslateUI();
    hotkeys_tab->RetranslateUI();
    graphics_tab->RetranslateUI();
    enhancements_tab->RetranslateUI();
    audio_tab->RetranslateUI();
    camera_tab->RetranslateUI();
    debug_tab->RetranslateUI();
    web_tab->RetranslateUI();
    ui_tab->RetranslateUI();
    storage_tab->RetranslateUI();
}

void ConfigureDialog::UpdateVisibleTabs() {
    const auto items = ui->selectorList->selectedItems();
    if (items.isEmpty())
        return;

    const std::map<QWidget*, QString> widgets = {{general_tab.get(), tr("General")},
                                                 {system_tab.get(), tr("System")},
                                                 {input_tab.get(), tr("Input")},
                                                 {hotkeys_tab.get(), tr("Hotkeys")},
                                                 {enhancements_tab.get(), tr("Enhancements")},
                                                 {graphics_tab.get(), tr("Advanced")},
                                                 {audio_tab.get(), tr("Audio")},
                                                 {camera_tab.get(), tr("Camera")},
                                                 {debug_tab.get(), tr("Debug")},
                                                 {storage_tab.get(), tr("Storage")},
                                                 {web_tab.get(), tr("Web")},
                                                 {ui_tab.get(), tr("UI")}};

    ui->tabWidget->clear();

    const QList<QWidget*> tabs = qvariant_cast<QList<QWidget*>>(items[0]->data(Qt::UserRole));

    for (const auto tab : tabs)
        ui->tabWidget->addTab(tab, widgets.at(tab));
}
