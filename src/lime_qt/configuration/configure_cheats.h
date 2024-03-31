// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <span>
#include <QWidget>
#include "common/common_types.h"

namespace Cheats {
class CheatBase;
class CheatEngine;
} // namespace Cheats

namespace Core {
class System;
}

namespace Ui {
class ConfigureCheats;
} // namespace Ui

class ConfigureCheats : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureCheats(Cheats::CheatEngine& cheat_engine, u64 title_id_,
                             QWidget* parent = nullptr);
    ~ConfigureCheats();
    bool ApplyConfiguration();

private:
    /**
     * Loads the cheats from the CheatEngine, and populates the table.
     */
    void LoadCheats();

    /**
     * Pops up a message box asking if the user wants to save the current cheat.
     * If the user selected Yes, attempts to save the current cheat.
     * @return true if the user selected No, or if the cheat was saved successfully
     *         false if the user selected Cancel, or if the user selected Yes but saving failed
     */
    bool CheckSaveCheat();

    /**
     * Saves the current cheat as the row-th cheat in the cheat list.
     * @return true if the cheat is saved successfully, false otherwise
     */
    bool SaveCheat(int row);

private slots:
    void OnRowSelected(int row, int column);
    void OnCheckChanged(int state);
    void OnTextEdited();
    void OnDeleteCheat();
    void OnAddCheat();

private:
    std::unique_ptr<Ui::ConfigureCheats> ui;
    Cheats::CheatEngine& cheat_engine;
    std::span<const std::shared_ptr<Cheats::CheatBase>> cheats;
    bool edited = false, newly_created = false;
    int last_row = -1, last_col = -1;
    u64 title_id;
};
