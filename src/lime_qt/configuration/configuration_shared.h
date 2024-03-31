// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QCheckBox>
#include <QComboBox>
#include "common/assert.h"
#include "common/settings.h"

namespace ConfigurationShared {

constexpr int USE_GLOBAL_INDEX =
    0; ///< The index of the "Use global configuration" option in checkboxes
constexpr int USE_GLOBAL_SEPARATOR_INDEX = 1;
constexpr int USE_GLOBAL_OFFSET = 2;

/// CheckBoxes require a tracker for their state since we emulate a tristate CheckBox
enum class CheckState {
    Off,    ///< Checkbox overrides to off/false
    On,     ///< Checkbox overrides to on/true
    Global, ///< Checkbox defers to the global state
    Count,  ///< Simply the number of states, not a valid checkbox state
};

/**
 * @brief ApplyPerGameSetting given a setting and a Qt UI element, properly applies a Setting
 * taking into account the global/per-game check state. This is used for configuring checkboxes
 * @param setting
 * @param checkbox
 * @param tracker
 */
void ApplyPerGameSetting(Settings::SwitchableSetting<bool>* setting, const QCheckBox* checkbox,
                         const CheckState& tracker);

/**
 * @brief ApplyPerGameSetting given a setting and a Qt UI element, properly applies a Setting
 * taking into account the global/per-game check state. This is used for both combo boxes
 * as well as any other widget that is accompanied by a combo box in per-game settings.
 * @param setting The setting class that stores the desired option
 * @param combobox The Qt combo box that stores the value/per-game status
 * @param transform A function that accepts the combo box index and transforms it to the
 * desired settings value. When used with sliders/edit the user can ignore the input value
 * and set a custom value this making this function useful for these widgets as well
 */
template <typename Type, bool ranged>
void ApplyPerGameSetting(Settings::SwitchableSetting<Type, ranged>* setting,
                         const QComboBox* combobox, auto transform) {
    if (Settings::IsConfiguringGlobal() && setting->UsingGlobal()) {
        setting->SetValue(static_cast<Type>(transform(combobox->currentIndex())));
    } else if (!Settings::IsConfiguringGlobal()) {
        if (combobox->currentIndex() == ConfigurationShared::USE_GLOBAL_INDEX) {
            setting->SetGlobal(true);
        } else {
            setting->SetGlobal(false);
            setting->SetValue(static_cast<Type>(
                transform(combobox->currentIndex() - ConfigurationShared::USE_GLOBAL_OFFSET)));
        }
    }
}

/// Simpler version of ApplyPerGameSetting without a transform parameter
template <typename Type, bool ranged>
void ApplyPerGameSetting(Settings::SwitchableSetting<Type, ranged>* setting,
                         const QComboBox* combobox) {
    const auto transform = [](s32 index) { return index; };
    return ApplyPerGameSetting(setting, combobox, transform);
}

/// Sets a Qt UI element given a Settings::Setting
void SetPerGameSetting(QCheckBox* checkbox, const Settings::SwitchableSetting<bool>* setting);

template <typename Type, bool ranged>
void SetPerGameSetting(QComboBox* combobox,
                       const Settings::SwitchableSetting<Type, ranged>* setting) {
    combobox->setCurrentIndex(setting->UsingGlobal() ? ConfigurationShared::USE_GLOBAL_INDEX
                                                     : static_cast<int>(setting->GetValue()) +
                                                           ConfigurationShared::USE_GLOBAL_OFFSET);
}

/// Given an index of a combobox setting extracts the setting taking into
/// account per-game status
template <typename Type, bool ranged>
Type GetComboboxSetting(int index, const Settings::SwitchableSetting<Type, ranged>* setting) {
    if (Settings::IsConfiguringGlobal() || setting->UsingGlobal()) {
        return static_cast<Type>(index);
    } else if (!Settings::IsConfiguringGlobal()) {
        if (index == 0) {
            return setting->GetValue();
        } else {
            return static_cast<Type>(index - ConfigurationShared::USE_GLOBAL_OFFSET);
        }
    }
    UNREACHABLE();
}

/// Given a Qt widget sets the background color to indicate whether the setting
/// is per-game overriden (highlighted) or global (non-highlighted)
void SetHighlight(QWidget* widget, bool highlighted);

/// Sets up a QCheckBox like a tristate one, given a Setting
void SetColoredTristate(QCheckBox* checkbox, const Settings::SwitchableSetting<bool>& setting,
                        CheckState& tracker);
void SetColoredTristate(QCheckBox* checkbox, bool global, bool state, bool global_state,
                        CheckState& tracker);

/// Sets up coloring of a QWidget `target` based on the state of a QComboBox, and calls
/// InsertGlobalItem
void SetColoredComboBox(QComboBox* combobox, QWidget* target, int global);

/// Adds the "Use Global Configuration" selection and separator to the beginning of a QComboBox
void InsertGlobalItem(QComboBox* combobox, int global_index);

} // namespace ConfigurationShared
