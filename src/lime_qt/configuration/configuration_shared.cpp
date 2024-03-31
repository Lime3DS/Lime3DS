// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QCheckBox>
#include <QObject>
#include <QString>
#include "common/settings.h"
#include "lime_qt/configuration/configuration_shared.h"
#include "lime_qt/configuration/configure_per_game.h"

void ConfigurationShared::ApplyPerGameSetting(Settings::SwitchableSetting<bool>* setting,
                                              const QCheckBox* checkbox,
                                              const CheckState& tracker) {
    if (Settings::IsConfiguringGlobal() && setting->UsingGlobal()) {
        setting->SetValue(checkbox->checkState());
    } else if (!Settings::IsConfiguringGlobal()) {
        if (tracker == CheckState::Global) {
            setting->SetGlobal(true);
        } else {
            setting->SetGlobal(false);
            setting->SetValue(checkbox->checkState());
        }
    }
}

void ConfigurationShared::SetPerGameSetting(QCheckBox* checkbox,
                                            const Settings::SwitchableSetting<bool>* setting) {
    if (setting->UsingGlobal()) {
        checkbox->setCheckState(Qt::PartiallyChecked);
    } else {
        checkbox->setCheckState(setting->GetValue() ? Qt::Checked : Qt::Unchecked);
    }
}

void ConfigurationShared::SetHighlight(QWidget* widget, bool highlighted) {
    if (highlighted) {
        widget->setStyleSheet(QStringLiteral("QWidget#%1 { background-color:rgba(0,203,255,0.5) }")
                                  .arg(widget->objectName()));
    } else {
        widget->setStyleSheet(QStringLiteral("QWidget#%1 { background-color:rgba(0,0,0,0) }")
                                  .arg(widget->objectName()));
    }
    widget->show();
}

void ConfigurationShared::SetColoredTristate(QCheckBox* checkbox,
                                             const Settings::SwitchableSetting<bool>& setting,
                                             CheckState& tracker) {
    if (setting.UsingGlobal()) {
        tracker = CheckState::Global;
    } else {
        tracker = (setting.GetValue() == setting.GetValue(true)) ? CheckState::On : CheckState::Off;
    }
    SetHighlight(checkbox, tracker != CheckState::Global);
    QObject::connect(checkbox, &QCheckBox::clicked, checkbox, [checkbox, setting, &tracker] {
        tracker = static_cast<CheckState>((static_cast<int>(tracker) + 1) %
                                          static_cast<int>(CheckState::Count));
        if (tracker == CheckState::Global) {
            checkbox->setChecked(setting.GetValue(true));
        }
        SetHighlight(checkbox, tracker != CheckState::Global);
    });
}

void ConfigurationShared::SetColoredTristate(QCheckBox* checkbox, bool global, bool state,
                                             bool global_state, CheckState& tracker) {
    if (global) {
        tracker = CheckState::Global;
    } else {
        tracker = (state == global_state) ? CheckState::On : CheckState::Off;
    }
    SetHighlight(checkbox, tracker != CheckState::Global);
    QObject::connect(checkbox, &QCheckBox::clicked, checkbox, [checkbox, global_state, &tracker] {
        tracker = static_cast<CheckState>((static_cast<int>(tracker) + 1) %
                                          static_cast<int>(CheckState::Count));
        if (tracker == CheckState::Global) {
            checkbox->setChecked(global_state);
        }
        SetHighlight(checkbox, tracker != CheckState::Global);
    });
}

void ConfigurationShared::SetColoredComboBox(QComboBox* combobox, QWidget* target, int global) {
    InsertGlobalItem(combobox, global);
    QObject::connect(combobox, qOverload<int>(&QComboBox::activated), target,
                     [target](int index) { SetHighlight(target, index != 0); });
}

void ConfigurationShared::InsertGlobalItem(QComboBox* combobox, int global_index) {
    const QString use_global_text =
        ConfigurePerGame::tr("Use global configuration (%1)").arg(combobox->itemText(global_index));
    combobox->insertItem(ConfigurationShared::USE_GLOBAL_INDEX, use_global_text);
    combobox->insertSeparator(ConfigurationShared::USE_GLOBAL_SEPARATOR_INDEX);
}
