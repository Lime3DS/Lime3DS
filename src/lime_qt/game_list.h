// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QMenu>
#include <QString>
#include <QVector>
#include <QWidget>
#include "common/common_types.h"
#include "lime_qt/compatibility_list.h"
#include "lime_qt/play_time_manager.h"
#include "uisettings.h"

namespace Service::FS {
enum class MediaType : u32;
}

class GameListWorker;
class GameListDir;
class GameListSearchField;
class GMainWindow;
class QFileSystemWatcher;
class QHBoxLayout;
class QLabel;
class QLineEdit;
template <typename>
class QList;
class QModelIndex;
class QStandardItem;
class QStandardItemModel;
class QTreeView;
class QToolButton;
class QVBoxLayout;

enum class GameListOpenTarget {
    SAVE_DATA = 0,
    EXT_DATA = 1,
    APPLICATION = 2,
    UPDATE_DATA = 3,
    TEXTURE_DUMP = 4,
    TEXTURE_LOAD = 5,
    MODS = 6,
    DLC_DATA = 7,
    SHADER_CACHE = 8
};

enum class GameListShortcutTarget {
    Desktop,
    Applications,
};

class GameList : public QWidget {
    Q_OBJECT

public:
    enum {
        COLUMN_NAME,
        COLUMN_COMPATIBILITY,
        COLUMN_REGION,
        COLUMN_FILE_TYPE,
        COLUMN_SIZE,
        COLUMN_PLAY_TIME,
        COLUMN_COUNT, // Number of columns
    };

    explicit GameList(PlayTime::PlayTimeManager& play_time_manager_, GMainWindow* parent = nullptr);
    ~GameList() override;

    QString GetLastFilterResultItem() const;
    void ClearFilter();
    void SetFilterFocus();
    void SetFilterVisible(bool visibility);
    void SetDirectoryWatcherEnabled(bool enabled);
    bool IsEmpty() const;

    void LoadCompatibilityList();
    void PopulateAsync(QVector<UISettings::GameDir>& game_dirs);

    void SaveInterfaceLayout();
    void LoadInterfaceLayout();

    QStandardItemModel* GetModel() const;

    QString FindGameByProgramID(u64 program_id, int role);

    void RefreshGameDirectory();

    void ToggleFavorite(u64 program_id);
    void AddFavorite(u64 program_id);
    void RemoveFavorite(u64 program_id);

    static const QStringList supported_file_extensions;

signals:
    void GameChosen(const QString& game_path);
    void ShouldCancelWorker();
    void OpenFolderRequested(u64 program_id, GameListOpenTarget target);
    void CreateShortcut(u64 program_id, const std::string& game_path,
                        GameListShortcutTarget target);
    void RemovePlayTimeRequested(u64 program_id);
    void OpenPerGameGeneralRequested(const QString file);
    void DumpRomFSRequested(QString game_path, u64 program_id);
    void OpenDirectory(const QString& directory);
    void AddDirectory();
    void ShowList(bool show);
    void PopulatingCompleted();

private slots:
    void OnItemExpanded(const QModelIndex& item);
    void OnTextChanged(const QString& new_text);
    void OnFilterCloseClicked();
    void OnUpdateThemedIcons();

private:
    void AddDirEntry(GameListDir* entry_items);
    void AddEntry(const QList<QStandardItem*>& entry_items, GameListDir* parent);
    void ValidateEntry(const QModelIndex& item);
    void DonePopulating(const QStringList& watch_list);

    void PopupContextMenu(const QPoint& menu_location);
    void PopupHeaderContextMenu(const QPoint& menu_location);
    void AddGamePopup(QMenu& context_menu, const QString& path, const QString& name, u64 program_id,
                      u64 extdata_id, Service::FS::MediaType media_type);
    void AddCustomDirPopup(QMenu& context_menu, QModelIndex selected);
    void AddPermDirPopup(QMenu& context_menu, QModelIndex selected);
    void AddFavoritesPopup(QMenu& context_menu);
    void UpdateColumnVisibility();

    QString FindGameByProgramID(QStandardItem* current_item, u64 program_id, int role);

    void changeEvent(QEvent*) override;
    void RetranslateUI();

    GameListSearchField* search_field;
    GMainWindow* main_window = nullptr;
    QVBoxLayout* layout = nullptr;
    QTreeView* tree_view = nullptr;
    QStandardItemModel* item_model = nullptr;
    GameListWorker* current_worker = nullptr;
    QFileSystemWatcher* watcher = nullptr;
    CompatibilityList compatibility_list;

    friend class GameListSearchField;

    const PlayTime::PlayTimeManager& play_time_manager;
};

Q_DECLARE_METATYPE(GameListOpenTarget);

class GameListPlaceholder : public QWidget {
    Q_OBJECT
public:
    explicit GameListPlaceholder(GMainWindow* parent = nullptr);
    ~GameListPlaceholder();

signals:
    void AddDirectory();

private slots:
    void onUpdateThemedIcons();

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QVBoxLayout* layout = nullptr;
    QLabel* image = nullptr;
    QLabel* text = nullptr;
};
