// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <QDialog>

class GameList;

namespace Ui {
class MoviePlayDialog;
}

namespace Core {
class System;
}

class MoviePlayDialog : public QDialog {
    Q_OBJECT

public:
    explicit MoviePlayDialog(QWidget* parent, GameList* game_list, const Core::System& system);
    ~MoviePlayDialog() override;

    QString GetMoviePath() const;
    QString GetGamePath() const;

private:
    void OnToolButtonClicked();
    void UpdateUIDisplay();

    std::unique_ptr<Ui::MoviePlayDialog> ui;
    GameList* game_list;
    const Core::System& system;
};
