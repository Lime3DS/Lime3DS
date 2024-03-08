// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QActionGroup>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QModelIndex>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QThreadPool>
#include <QToolButton>
#include <QTreeView>
#include <fmt/format.h>
#include "citra_qt/compatibility_list.h"
#include "citra_qt/game_list.h"
#include "citra_qt/game_list_p.h"
#include "citra_qt/game_list_worker.h"
#include "citra_qt/main.h"
#include "citra_qt/uisettings.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/archive_source_sd_savedata.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "qcursor.h"

GameListSearchField::KeyReleaseEater::KeyReleaseEater(GameList* gamelist, QObject* parent)
    : QObject(parent), gamelist{gamelist} {}

// EventFilter in order to process systemkeys while editing the searchfield
bool GameListSearchField::KeyReleaseEater::eventFilter(QObject* obj, QEvent* event) {
    // If it isn't a KeyRelease event then continue with standard event processing
    if (event->type() != QEvent::KeyRelease)
        return QObject::eventFilter(obj, event);

    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
    QString edit_filter_text = gamelist->search_field->edit_filter->text().toLower();

    // If the searchfield's text hasn't changed special function keys get checked
    // If no function key changes the searchfield's text the filter doesn't need to get reloaded
    if (edit_filter_text == edit_filter_text_old) {
        switch (keyEvent->key()) {
        // Escape: Resets the searchfield
        case Qt::Key_Escape: {
            if (edit_filter_text_old.isEmpty()) {
                return QObject::eventFilter(obj, event);
            } else {
                gamelist->search_field->edit_filter->clear();
                edit_filter_text.clear();
            }
            break;
        }
        // Return and Enter
        // If the enter key gets pressed first checks how many and which entry is visible
        // If there is only one result launch this game
        case Qt::Key_Return:
        case Qt::Key_Enter: {
            if (gamelist->search_field->visible == 1) {
                const QString file_path = gamelist->GetLastFilterResultItem();

                // To avoid loading error dialog loops while confirming them using enter
                // Also users usually want to run a different game after closing one
                gamelist->search_field->edit_filter->clear();
                edit_filter_text.clear();
                emit gamelist->GameChosen(file_path);
            } else {
                return QObject::eventFilter(obj, event);
            }
            break;
        }
        default:
            return QObject::eventFilter(obj, event);
        }
    }
    edit_filter_text_old = edit_filter_text;
    return QObject::eventFilter(obj, event);
}

void GameListSearchField::setFilterResult(int visible, int total) {
    this->visible = visible;
    this->total = total;

    QString result_of_text = tr("of");
    QString result_text;
    if (total == 1) {
        result_text = tr("result");
    } else {
        result_text = tr("results");
    }
    label_filter_result->setText(
        QStringLiteral("%1 %2 %3 %4").arg(visible).arg(result_of_text).arg(total).arg(result_text));
}

QString GameList::GetLastFilterResultItem() const {
    QString file_path;
    const int folderCount = item_model->rowCount();

    for (int i = 0; i < folderCount; ++i) {
        const QStandardItem* folder = item_model->item(i, 0);
        const QModelIndex folder_index = folder->index();
        const int children_count = folder->rowCount();

        for (int j = 0; j < children_count; ++j) {
            if (tree_view->isRowHidden(j, folder_index)) {
                continue;
            }

            const QStandardItem* child = folder->child(j, 0);
            file_path = child->data(GameListItemPath::FullPathRole).toString();
        }
    }

    return file_path;
}

void GameListSearchField::clear() {
    edit_filter->clear();
}

void GameListSearchField::setFocus() {
    if (edit_filter->isVisible()) {
        edit_filter->setFocus();
    }
}

GameListSearchField::GameListSearchField(GameList* parent) : QWidget{parent} {
    auto* const key_release_eater = new KeyReleaseEater(parent, this);
    layout_filter = new QHBoxLayout;
    layout_filter->setContentsMargins(8, 8, 8, 8);
    label_filter = new QLabel;
    edit_filter = new QLineEdit;
    edit_filter->clear();
    edit_filter->installEventFilter(key_release_eater);
    edit_filter->setClearButtonEnabled(true);
    connect(edit_filter, &QLineEdit::textChanged, parent, &GameList::OnTextChanged);
    label_filter_result = new QLabel;
    button_filter_close = new QToolButton(this);
    button_filter_close->setText(QStringLiteral("X"));
    button_filter_close->setCursor(Qt::ArrowCursor);
    button_filter_close->setStyleSheet(
        QStringLiteral("QToolButton{ border: none; padding: 0px; color: "
                       "#000000; font-weight: bold; background: #F0F0F0; }"
                       "QToolButton:hover{ border: none; padding: 0px; color: "
                       "#EEEEEE; font-weight: bold; background: #E81123}"));
    connect(button_filter_close, &QToolButton::clicked, parent, &GameList::OnFilterCloseClicked);
    layout_filter->setSpacing(10);
    layout_filter->addWidget(label_filter);
    layout_filter->addWidget(edit_filter);
    layout_filter->addWidget(label_filter_result);
    layout_filter->addWidget(button_filter_close);
    setLayout(layout_filter);
    RetranslateUI();
}

/**
 * Checks if all words separated by spaces are contained in another string
 * This offers a word order insensitive search function
 *
 * @param String that gets checked if it contains all words of the userinput string
 * @param String containing all words getting checked
 * @return true if the haystack contains all words of userinput
 */
static bool ContainsAllWords(const QString& haystack, const QString& userinput) {
    const QStringList userinput_split = userinput.split(QLatin1Char{' '}, Qt::SkipEmptyParts);

    return std::all_of(userinput_split.begin(), userinput_split.end(),
                       [&haystack](const QString& s) { return haystack.contains(s); });
}

// Syncs the expanded state of Game Directories with settings to persist across sessions
void GameList::OnItemExpanded(const QModelIndex& item) {
    const auto type = item.data(GameListItem::TypeRole).value<GameListItemType>();
    const bool is_dir = type == GameListItemType::CustomDir ||
                        type == GameListItemType::InstalledDir ||
                        type == GameListItemType::SystemDir;

    if (!is_dir) {
        return;
    }

    UISettings::values.game_dirs[item.data(GameListDir::GameDirRole).toInt()].expanded =
        tree_view->isExpanded(item);
}

// Event in order to filter the gamelist after editing the searchfield
void GameList::OnTextChanged(const QString& new_text) {
    const int folder_count = tree_view->model()->rowCount();
    QString edit_filter_text = new_text.toLower();
    QStandardItem* folder;
    int children_total = 0;

    // If the searchfield is empty every item is visible
    // Otherwise the filter gets applied
    if (edit_filter_text.isEmpty()) {
        for (int i = 0; i < folder_count; ++i) {
            folder = item_model->item(i, 0);
            const QModelIndex folder_index = folder->index();
            const int children_count = folder->rowCount();
            for (int j = 0; j < children_count; ++j) {
                ++children_total;
                tree_view->setRowHidden(j, folder_index, false);
            }
        }
        search_field->setFilterResult(children_total, children_total);
    } else {
        int result_count = 0;
        for (int i = 0; i < folder_count; ++i) {
            folder = item_model->item(i, 0);
            const QModelIndex folder_index = folder->index();
            const int children_count = folder->rowCount();
            for (int j = 0; j < children_count; ++j) {
                ++children_total;
                const QStandardItem* child = folder->child(j, 0);
                const QString file_path =
                    child->data(GameListItemPath::FullPathRole).toString().toLower();
                const QString file_title =
                    child->data(GameListItemPath::LongTitleRole).toString().toLower();
                const QString file_program_id =
                    child->data(GameListItemPath::ProgramIdRole).toString().toLower();

                // Only items which filename in combination with its title contains all words
                // that are in the searchfield will be visible in the gamelist
                // The search is case insensitive because of toLower()
                // I decided not to use Qt::CaseInsensitive in containsAllWords to prevent
                // multiple conversions of edit_filter_text for each game in the gamelist
                const QString file_name =
                    file_path.mid(file_path.lastIndexOf(QLatin1Char{'/'}) + 1) + QLatin1Char{' '} +
                    file_title;
                if (ContainsAllWords(file_name, edit_filter_text) ||
                    (file_program_id.length() == 16 &&
                     edit_filter_text.contains(file_program_id))) {
                    tree_view->setRowHidden(j, folder_index, false);
                    ++result_count;
                } else {
                    tree_view->setRowHidden(j, folder_index, true);
                }
                search_field->setFilterResult(result_count, children_total);
            }
        }
    }
}

void GameList::OnUpdateThemedIcons() {
    for (int i = 0; i < item_model->invisibleRootItem()->rowCount(); i++) {
        QStandardItem* child = item_model->invisibleRootItem()->child(i);

        const int icon_size = IconSizes.at(UISettings::values.game_list_icon_size.GetValue());
        switch (child->data(GameListItem::TypeRole).value<GameListItemType>()) {
        case GameListItemType::InstalledDir:
            child->setData(QIcon::fromTheme(QStringLiteral("sd_card")).pixmap(icon_size),
                           Qt::DecorationRole);
            break;
        case GameListItemType::SystemDir:
            child->setData(QIcon::fromTheme(QStringLiteral("chip")).pixmap(icon_size),
                           Qt::DecorationRole);
            break;
        case GameListItemType::CustomDir: {
            const UISettings::GameDir& game_dir =
                UISettings::values.game_dirs[child->data(GameListDir::GameDirRole).toInt()];
            const QString icon_name = QFileInfo::exists(game_dir.path)
                                          ? QStringLiteral("folder")
                                          : QStringLiteral("bad_folder");
            child->setData(QIcon::fromTheme(icon_name).pixmap(icon_size), Qt::DecorationRole);
            break;
        }
        case GameListItemType::AddDir:
            child->setData(QIcon::fromTheme(QStringLiteral("plus")).pixmap(icon_size),
                           Qt::DecorationRole);
            break;
        default:
            break;
        }
    }
}

void GameList::OnFilterCloseClicked() {
    main_window->filterBarSetChecked(false);
}

GameList::GameList(GMainWindow* parent) : QWidget{parent} {
    watcher = new QFileSystemWatcher(this);
    connect(watcher, &QFileSystemWatcher::directoryChanged, this, &GameList::RefreshGameDirectory,
            Qt::UniqueConnection);

    this->main_window = parent;
    layout = new QVBoxLayout;
    tree_view = new QTreeView;
    search_field = new GameListSearchField(this);
    item_model = new QStandardItemModel(tree_view);
    tree_view->setModel(item_model);

    tree_view->setAlternatingRowColors(true);
    tree_view->setSelectionMode(QHeaderView::SingleSelection);
    tree_view->setSelectionBehavior(QHeaderView::SelectRows);
    tree_view->setVerticalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setHorizontalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setSortingEnabled(true);
    tree_view->setEditTriggers(QHeaderView::NoEditTriggers);
    tree_view->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_view->setStyleSheet(QStringLiteral("QTreeView{ border: none; }"));
    tree_view->header()->setContextMenuPolicy(Qt::CustomContextMenu);

    UpdateColumnVisibility();

    item_model->insertColumns(0, COLUMN_COUNT);
    RetranslateUI();
    item_model->setSortRole(GameListItemPath::SortRole);

    connect(main_window, &GMainWindow::UpdateThemedIcons, this, &GameList::OnUpdateThemedIcons);
    connect(tree_view, &QTreeView::activated, this, &GameList::ValidateEntry);
    connect(tree_view, &QTreeView::customContextMenuRequested, this, &GameList::PopupContextMenu);
    connect(tree_view, &QTreeView::expanded, this, &GameList::OnItemExpanded);
    connect(tree_view, &QTreeView::collapsed, this, &GameList::OnItemExpanded);
    connect(tree_view->header(), &QHeaderView::customContextMenuRequested, this,
            &GameList::PopupHeaderContextMenu);

    // We must register all custom types with the Qt Automoc system so that we are able to use
    // it with signals/slots. In this case, QList falls under the umbrells of custom types.
    qRegisterMetaType<QList<QStandardItem*>>("QList<QStandardItem*>");

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tree_view);
    layout->addWidget(search_field);
    setLayout(layout);
}

GameList::~GameList() {
    emit ShouldCancelWorker();
}

void GameList::SetFilterFocus() {
    if (tree_view->model()->rowCount() > 0) {
        search_field->setFocus();
    }
}

void GameList::SetFilterVisible(bool visibility) {
    search_field->setVisible(visibility);
}

void GameList::SetDirectoryWatcherEnabled(bool enabled) {
    if (enabled) {
        connect(watcher, &QFileSystemWatcher::directoryChanged, this,
                &GameList::RefreshGameDirectory, Qt::UniqueConnection);
    } else {
        disconnect(watcher, &QFileSystemWatcher::directoryChanged, this,
                   &GameList::RefreshGameDirectory);
    }
}

void GameList::ClearFilter() {
    search_field->clear();
}

void GameList::AddDirEntry(GameListDir* entry_items) {
    item_model->invisibleRootItem()->appendRow(entry_items);
    tree_view->setExpanded(
        entry_items->index(),
        UISettings::values.game_dirs[entry_items->data(GameListDir::GameDirRole).toInt()].expanded);
}

void GameList::AddEntry(const QList<QStandardItem*>& entry_items, GameListDir* parent) {
    parent->appendRow(entry_items);
}

void GameList::ValidateEntry(const QModelIndex& item) {
    const auto selected = item.sibling(item.row(), 0);

    switch (selected.data(GameListItem::TypeRole).value<GameListItemType>()) {
    case GameListItemType::Game: {
        const QString file_path = selected.data(GameListItemPath::FullPathRole).toString();
        if (file_path.isEmpty())
            return;
        const QFileInfo file_info(file_path);
        if (!file_info.exists() || file_info.isDir())
            return;
        // Users usually want to run a different game after closing one
        search_field->clear();
        emit GameChosen(file_path);
        break;
    }
    case GameListItemType::AddDir:
        emit AddDirectory();
        break;
    default:
        break;
    }
}

bool GameList::IsEmpty() const {
    for (int i = 0; i < item_model->rowCount(); i++) {
        const QStandardItem* child = item_model->invisibleRootItem()->child(i);
        const auto type = static_cast<GameListItemType>(child->type());

        if (!child->hasChildren() &&
            (type == GameListItemType::InstalledDir || type == GameListItemType::SystemDir)) {
            item_model->invisibleRootItem()->removeRow(child->row());
            i--;
        }
    }

    return !item_model->invisibleRootItem()->hasChildren();
}

void GameList::DonePopulating(const QStringList& watch_list) {
    emit ShowList(!IsEmpty());

    item_model->invisibleRootItem()->appendRow(new GameListAddDir());

    // Clear out the old directories to watch for changes and add the new ones
    auto watch_dirs = watcher->directories();
    if (!watch_dirs.isEmpty()) {
        watcher->removePaths(watch_dirs);
    }
    // Workaround: Add the watch paths in chunks to allow the gui to refresh
    // This prevents the UI from stalling when a large number of watch paths are added
    // Also artificially caps the watcher to a certain number of directories
    constexpr qsizetype LIMIT_WATCH_DIRECTORIES = 5000;
    constexpr int SLICE_SIZE = 25;
    const qsizetype len = std::min(watch_list.length(), LIMIT_WATCH_DIRECTORIES);
    for (qsizetype i = 0; i < len; i += SLICE_SIZE) {
        watcher->addPaths(watch_list.mid(i, i + SLICE_SIZE));
        QCoreApplication::processEvents();
    }
    tree_view->setEnabled(true);
    const int folderCount = tree_view->model()->rowCount();
    int children_total = 0;
    for (int i = 0; i < folderCount; ++i) {
        children_total += item_model->item(i, 0)->rowCount();
    }
    search_field->setFilterResult(children_total, children_total);
    if (children_total > 0) {
        search_field->setFocus();
    }
    item_model->sort(tree_view->header()->sortIndicatorSection(),
                     tree_view->header()->sortIndicatorOrder());

    emit PopulatingCompleted();
}

void GameList::PopupContextMenu(const QPoint& menu_location) {
    QModelIndex item = tree_view->indexAt(menu_location);
    if (!item.isValid())
        return;

    const auto selected = item.sibling(item.row(), 0);
    QMenu context_menu;
    switch (selected.data(GameListItem::TypeRole).value<GameListItemType>()) {
    case GameListItemType::Game:
        AddGamePopup(context_menu, selected.data(GameListItemPath::FullPathRole).toString(),
                     selected.data(GameListItemPath::TitleRole).toString(),
                     selected.data(GameListItemPath::ProgramIdRole).toULongLong(),
                     selected.data(GameListItemPath::ExtdataIdRole).toULongLong(),
                     static_cast<Service::FS::MediaType>(
                         selected.data(GameListItemPath::MediaTypeRole).toUInt()));
        break;
    case GameListItemType::CustomDir:
        AddPermDirPopup(context_menu, selected);
        AddCustomDirPopup(context_menu, selected);
        break;
    case GameListItemType::InstalledDir:
    case GameListItemType::SystemDir:
        AddPermDirPopup(context_menu, selected);
        break;
    default:
        break;
    }

    context_menu.exec(tree_view->viewport()->mapToGlobal(menu_location));
}

void GameList::PopupHeaderContextMenu(const QPoint& menu_location) {
    const QModelIndex item = tree_view->indexAt(menu_location);
    if (!item.isValid()) {
        return;
    }

    QMenu context_menu;
    static const QMap<QString, Settings::Setting<bool>*> columns{
        {tr("Compatibility"), &UISettings::values.show_compat_column},
        {tr("Region"), &UISettings::values.show_region_column},
        {tr("File type"), &UISettings::values.show_type_column},
        {tr("Size"), &UISettings::values.show_size_column}};

    QActionGroup* column_group = new QActionGroup(this);
    column_group->setExclusive(false);
    for (const auto& key : columns.keys()) {
        QAction* action = column_group->addAction(context_menu.addAction(key));
        action->setCheckable(true);
        action->setChecked(columns[key]->GetValue());
        connect(action, &QAction::toggled, [this, key](bool value) {
            *columns[key] = !columns[key]->GetValue();
            UpdateColumnVisibility();
        });
    }

    context_menu.exec(tree_view->viewport()->mapToGlobal(menu_location));
}

void GameList::UpdateColumnVisibility() {
    tree_view->setColumnHidden(COLUMN_COMPATIBILITY, !UISettings::values.show_compat_column);
    tree_view->setColumnHidden(COLUMN_REGION, !UISettings::values.show_region_column);
    tree_view->setColumnHidden(COLUMN_FILE_TYPE, !UISettings::values.show_type_column);
    tree_view->setColumnHidden(COLUMN_SIZE, !UISettings::values.show_size_column);
}

#ifdef ENABLE_OPENGL
void ForEachOpenGLCacheFile(u64 program_id, auto func) {
    for (const std::string_view cache_type : {"separable", "conventional"}) {
        const std::string path = fmt::format("{}opengl/precompiled/{}/{:016X}.bin",
                                             FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir),
                                             cache_type, program_id);
        QFile file{QString::fromStdString(path)};
        func(file);
    }
}
#endif

void GameList::AddGamePopup(QMenu& context_menu, const QString& path, const QString& name,
                            u64 program_id, u64 extdata_id, Service::FS::MediaType media_type) {
    QAction* open_save_location = context_menu.addAction(tr("Open Save Data Location"));
    QAction* open_extdata_location = context_menu.addAction(tr("Open Extra Data Location"));
    QAction* open_application_location = context_menu.addAction(tr("Open Application Location"));
    QAction* open_update_location = context_menu.addAction(tr("Open Update Data Location"));
    QAction* open_dlc_location = context_menu.addAction(tr("Open DLC Data Location"));
    QAction* open_texture_dump_location = context_menu.addAction(tr("Open Texture Dump Location"));
    QAction* open_texture_load_location =
        context_menu.addAction(tr("Open Custom Texture Location"));
    QAction* open_mods_location = context_menu.addAction(tr("Open Mods Location"));
    QAction* dump_romfs = context_menu.addAction(tr("Dump RomFS"));

    QMenu* shader_menu = context_menu.addMenu(tr("Disk Shader Cache"));
    QAction* open_shader_cache_location = shader_menu->addAction(tr("Open Shader Cache Location"));
    shader_menu->addSeparator();
#ifdef ENABLE_OPENGL
    QAction* delete_opengl_disk_shader_cache =
        shader_menu->addAction(tr("Delete OpenGL Shader Cache"));
#endif

    QMenu* uninstall_menu = context_menu.addMenu(tr("Uninstall"));
    QAction* uninstall_all = uninstall_menu->addAction(tr("Everything"));
    uninstall_menu->addSeparator();
    QAction* uninstall_game = uninstall_menu->addAction(tr("Game"));
    QAction* uninstall_update = uninstall_menu->addAction(tr("Update"));
    QAction* uninstall_dlc = uninstall_menu->addAction(tr("DLC"));

    QAction* navigate_to_gamedb_entry = context_menu.addAction(tr("Navigate to GameDB entry"));
    context_menu.addSeparator();
    QAction* properties = context_menu.addAction(tr("Properties"));

    const u32 program_id_high = (program_id >> 32) & 0xFFFFFFFF;
    // TODO: Use proper bitmasks for these kinds of checks.
    const bool is_application = program_id_high == 0x00040000 || program_id_high == 0x00040002 ||
                                program_id_high == 0x00040010;

#ifdef ENABLE_OPENGL
    bool opengl_cache_exists = false;
    ForEachOpenGLCacheFile(
        program_id, [&opengl_cache_exists](QFile& file) { opengl_cache_exists |= file.exists(); });
#endif

    std::string sdmc_dir = FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir);
    open_save_location->setEnabled(
        is_application && FileUtil::Exists(FileSys::ArchiveSource_SDSaveData::GetSaveDataPathFor(
                              sdmc_dir, program_id)));

    if (extdata_id) {
        open_extdata_location->setEnabled(
            is_application &&
            FileUtil::Exists(FileSys::GetExtDataPathFromId(sdmc_dir, extdata_id)));
    } else {
        open_extdata_location->setVisible(false);
    }

    const auto update_program_id = program_id | 0xE00000000;
    const auto dlc_program_id = program_id | 0x8C00000000;

    const auto is_installed =
        media_type == Service::FS::MediaType::NAND || media_type == Service::FS::MediaType::SDMC;
    const auto has_update =
        is_application && FileUtil::Exists(Service::AM::GetTitlePath(Service::FS::MediaType::SDMC,
                                                                     update_program_id) +
                                           "content/");
    const auto has_dlc =
        is_application &&
        FileUtil::Exists(Service::AM::GetTitlePath(Service::FS::MediaType::SDMC, dlc_program_id) +
                         "content/");

    open_application_location->setEnabled(is_installed);
    open_update_location->setEnabled(has_update);
    open_dlc_location->setEnabled(has_dlc);
    open_texture_dump_location->setEnabled(is_application);
    open_texture_load_location->setEnabled(is_application);
    open_mods_location->setEnabled(is_application);
    dump_romfs->setEnabled(is_application);

#ifdef ENABLE_OPENGL
    delete_opengl_disk_shader_cache->setEnabled(opengl_cache_exists);
#endif

    uninstall_all->setEnabled(is_installed || has_update || has_dlc);
    uninstall_game->setEnabled(is_installed);
    uninstall_update->setEnabled(has_update);
    uninstall_dlc->setEnabled(has_dlc);

    auto it = FindMatchingCompatibilityEntry(compatibility_list, program_id);
    navigate_to_gamedb_entry->setVisible(it != compatibility_list.end());

    connect(open_save_location, &QAction::triggered, this, [this, program_id] {
        emit OpenFolderRequested(program_id, GameListOpenTarget::SAVE_DATA);
    });
    connect(open_extdata_location, &QAction::triggered, this, [this, extdata_id] {
        emit OpenFolderRequested(extdata_id, GameListOpenTarget::EXT_DATA);
    });
    connect(open_application_location, &QAction::triggered, this, [this, program_id] {
        emit OpenFolderRequested(program_id, GameListOpenTarget::APPLICATION);
    });
    connect(open_update_location, &QAction::triggered, this, [this, program_id] {
        emit OpenFolderRequested(program_id, GameListOpenTarget::UPDATE_DATA);
    });
    connect(open_texture_dump_location, &QAction::triggered, this, [this, program_id] {
        if (FileUtil::CreateFullPath(fmt::format("{}textures/{:016X}/",
                                                 FileUtil::GetUserPath(FileUtil::UserPath::DumpDir),
                                                 program_id))) {
            emit OpenFolderRequested(program_id, GameListOpenTarget::TEXTURE_DUMP);
        }
    });
    connect(open_texture_load_location, &QAction::triggered, this, [this, program_id] {
        if (FileUtil::CreateFullPath(fmt::format("{}textures/{:016X}/",
                                                 FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                                                 program_id))) {
            emit OpenFolderRequested(program_id, GameListOpenTarget::TEXTURE_LOAD);
        }
    });
    connect(open_mods_location, &QAction::triggered, this, [this, program_id] {
        if (FileUtil::CreateFullPath(fmt::format("{}mods/{:016X}/",
                                                 FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                                                 program_id))) {
            emit OpenFolderRequested(program_id, GameListOpenTarget::MODS);
        }
    });
    connect(open_dlc_location, &QAction::triggered, this, [this, program_id] {
        const u64 trimmed_id = program_id & 0xFFFFFFF;
        const std::string dlc_path =
            fmt::format("{}Nintendo 3DS/00000000000000000000000000000000/"
                        "00000000000000000000000000000000/title/0004008c/{:08x}/content/",
                        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), trimmed_id);
        fmt::print("DLC path {}\n", dlc_path);
        if (FileUtil::CreateFullPath(dlc_path)) {
            emit OpenFolderRequested(trimmed_id, GameListOpenTarget::DLC_DATA);
        }
    });
    connect(dump_romfs, &QAction::triggered, this,
            [this, path, program_id] { emit DumpRomFSRequested(path, program_id); });
    connect(navigate_to_gamedb_entry, &QAction::triggered, this, [this, program_id]() {
        emit NavigateToGamedbEntryRequested(program_id, compatibility_list);
    });
    connect(properties, &QAction::triggered, this,
            [this, path]() { emit OpenPerGameGeneralRequested(path); });
    connect(open_shader_cache_location, &QAction::triggered, this, [this, program_id] {
        if (FileUtil::CreateFullPath(FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir))) {
            emit OpenFolderRequested(program_id, GameListOpenTarget::SHADER_CACHE);
        }
    });
#ifdef ENABLE_OPENGL
    connect(delete_opengl_disk_shader_cache, &QAction::triggered, this, [program_id] {
        ForEachOpenGLCacheFile(program_id, [](QFile& file) { file.remove(); });
    });
#endif
    connect(uninstall_all, &QAction::triggered, this, [=, this] {
        QMessageBox::StandardButton answer = QMessageBox::question(
            this, tr("Citra"),
            tr("Are you sure you want to completely uninstall '%1'?\n\nThis will "
               "delete the game if installed, as well as any installed updates or DLC.")
                .arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            std::vector<std::tuple<Service::FS::MediaType, u64, QString>> titles;
            if (is_installed) {
                titles.emplace_back(media_type, program_id, name);
            }
            if (has_update) {
                titles.emplace_back(Service::FS::MediaType::SDMC, update_program_id,
                                    tr("%1 (Update)").arg(name));
            }
            if (has_dlc) {
                titles.emplace_back(Service::FS::MediaType::SDMC, dlc_program_id,
                                    tr("%1 (DLC)").arg(name));
            }
            main_window->UninstallTitles(titles);
        }
    });
    connect(uninstall_game, &QAction::triggered, this, [this, name, media_type, program_id] {
        QMessageBox::StandardButton answer = QMessageBox::question(
            this, tr("Citra"), tr("Are you sure you want to uninstall '%1'?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            std::vector<std::tuple<Service::FS::MediaType, u64, QString>> titles;
            titles.emplace_back(media_type, program_id, name);
            main_window->UninstallTitles(titles);
        }
    });
    connect(uninstall_update, &QAction::triggered, this, [this, name, update_program_id] {
        QMessageBox::StandardButton answer = QMessageBox::question(
            this, tr("Citra"),
            tr("Are you sure you want to uninstall the update for '%1'?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            std::vector<std::tuple<Service::FS::MediaType, u64, QString>> titles;
            titles.emplace_back(Service::FS::MediaType::SDMC, update_program_id,
                                tr("%1 (Update)").arg(name));
            main_window->UninstallTitles(titles);
        }
    });
    connect(uninstall_dlc, &QAction::triggered, this, [this, name, dlc_program_id] {
        QMessageBox::StandardButton answer = QMessageBox::question(
            this, tr("Citra"), tr("Are you sure you want to uninstall all DLC for '%1'?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            std::vector<std::tuple<Service::FS::MediaType, u64, QString>> titles;
            titles.emplace_back(Service::FS::MediaType::SDMC, dlc_program_id,
                                tr("%1 (DLC)").arg(name));
            main_window->UninstallTitles(titles);
        }
    });
}

void GameList::AddCustomDirPopup(QMenu& context_menu, QModelIndex selected) {
    UISettings::GameDir& game_dir =
        UISettings::values.game_dirs[selected.data(GameListDir::GameDirRole).toInt()];

    QAction* deep_scan = context_menu.addAction(tr("Scan Subfolders"));
    QAction* delete_dir = context_menu.addAction(tr("Remove Game Directory"));

    deep_scan->setCheckable(true);
    deep_scan->setChecked(game_dir.deep_scan);

    connect(deep_scan, &QAction::triggered, this, [this, &game_dir] {
        game_dir.deep_scan = !game_dir.deep_scan;
        PopulateAsync(UISettings::values.game_dirs);
    });
    connect(delete_dir, &QAction::triggered, this, [this, &game_dir, selected] {
        UISettings::values.game_dirs.removeOne(game_dir);
        item_model->invisibleRootItem()->removeRow(selected.row());
    });
}

void GameList::AddPermDirPopup(QMenu& context_menu, QModelIndex selected) {
    const int game_dir_index = selected.data(GameListDir::GameDirRole).toInt();

    QAction* move_up =
        context_menu.addAction(tr("Move Up").prepend(QString::fromWCharArray(L"\u25b2 ")));
    QAction* move_down =
        context_menu.addAction(tr("Move Down").prepend(QString::fromWCharArray(L"\u25bc ")));
    QAction* open_directory_location = context_menu.addAction(tr("Open Directory Location"));

    const int row = selected.row();

    move_up->setEnabled(row > 0);
    move_down->setEnabled(row < item_model->rowCount() - 2);

    connect(move_up, &QAction::triggered, this, [this, selected, row, game_dir_index] {
        const int other_index = selected.sibling(row - 1, 0).data(GameListDir::GameDirRole).toInt();
        // swap the items in the settings
        std::swap(UISettings::values.game_dirs[game_dir_index],
                  UISettings::values.game_dirs[other_index]);
        // swap the indexes held by the QVariants
        GetModel()->setData(selected, QVariant(other_index), GameListDir::GameDirRole);
        GetModel()->setData(selected.sibling(row - 1, 0), QVariant(game_dir_index),
                            GameListDir::GameDirRole);
        // move the treeview items
        QList<QStandardItem*> item = item_model->takeRow(row);
        item_model->invisibleRootItem()->insertRow(row - 1, item);
        tree_view->setExpanded(selected, UISettings::values.game_dirs[game_dir_index].expanded);
    });

    connect(move_down, &QAction::triggered, this, [this, selected, row, game_dir_index] {
        const int other_index = selected.sibling(row + 1, 0).data(GameListDir::GameDirRole).toInt();
        // swap the items in the settings
        std::swap(UISettings::values.game_dirs[game_dir_index],
                  UISettings::values.game_dirs[other_index]);
        // swap the indexes held by the QVariants
        GetModel()->setData(selected, QVariant(other_index), GameListDir::GameDirRole);
        GetModel()->setData(selected.sibling(row + 1, 0), QVariant(game_dir_index),
                            GameListDir::GameDirRole);
        // move the treeview items
        const QList<QStandardItem*> item = item_model->takeRow(row);
        item_model->invisibleRootItem()->insertRow(row + 1, item);
        tree_view->setExpanded(selected, UISettings::values.game_dirs[game_dir_index].expanded);
    });

    connect(open_directory_location, &QAction::triggered, this, [this, game_dir_index] {
        emit OpenDirectory(UISettings::values.game_dirs[game_dir_index].path);
    });
}

void GameList::LoadCompatibilityList() {
    QFile compat_list{QStringLiteral(":compatibility_list/compatibility_list.json")};

    if (!compat_list.open(QFile::ReadOnly | QFile::Text)) {
        LOG_ERROR(Frontend, "Unable to open game compatibility list");
        return;
    }

    if (compat_list.size() == 0) {
        LOG_WARNING(Frontend, "Game compatibility list is empty");
        return;
    }

    const QByteArray content = compat_list.readAll();
    if (content.isEmpty()) {
        LOG_ERROR(Frontend, "Unable to completely read game compatibility list");
        return;
    }

    const QJsonDocument json = QJsonDocument::fromJson(content);
    const QJsonArray arr = json.array();

    for (const QJsonValue& value : arr) {
        const QJsonObject game = value.toObject();
        const QString compatibility_key = QStringLiteral("compatibility");

        if (!game.contains(compatibility_key) || !game[compatibility_key].isDouble()) {
            continue;
        }

        const int compatibility = game[compatibility_key].toInt();
        const QString directory = game[QStringLiteral("directory")].toString();
        const QJsonArray ids = game[QStringLiteral("releases")].toArray();

        for (const QJsonValue& id_ref : ids) {
            const QJsonObject id_object = id_ref.toObject();
            const QString id = id_object[QStringLiteral("id")].toString();

            compatibility_list.emplace(id.toUpper().toStdString(),
                                       std::make_pair(QString::number(compatibility), directory));
        }
    }
}

void GameList::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void GameList::RetranslateUI() {
    item_model->setHeaderData(COLUMN_NAME, Qt::Horizontal, tr("Name"));
    item_model->setHeaderData(COLUMN_COMPATIBILITY, Qt::Horizontal, tr("Compatibility"));
    item_model->setHeaderData(COLUMN_REGION, Qt::Horizontal, tr("Region"));
    item_model->setHeaderData(COLUMN_FILE_TYPE, Qt::Horizontal, tr("File type"));
    item_model->setHeaderData(COLUMN_SIZE, Qt::Horizontal, tr("Size"));
}

void GameListSearchField::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void GameListSearchField::RetranslateUI() {
    label_filter->setText(tr("Filter:"));
    edit_filter->setPlaceholderText(tr("Enter pattern to filter"));
}

QStandardItemModel* GameList::GetModel() const {
    return item_model;
}

void GameList::PopulateAsync(QVector<UISettings::GameDir>& game_dirs) {
    tree_view->setEnabled(false);

    // Update the columns in case UISettings has changed
    UpdateColumnVisibility();

    // Delete any rows that might already exist if we're repopulating
    item_model->removeRows(0, item_model->rowCount());
    search_field->clear();

    emit ShouldCancelWorker();

    GameListWorker* worker = new GameListWorker(game_dirs, compatibility_list);

    connect(worker, &GameListWorker::EntryReady, this, &GameList::AddEntry, Qt::QueuedConnection);
    connect(worker, &GameListWorker::DirEntryReady, this, &GameList::AddDirEntry,
            Qt::QueuedConnection);
    connect(worker, &GameListWorker::Finished, this, &GameList::DonePopulating,
            Qt::QueuedConnection);
    // Use DirectConnection here because worker->Cancel() is thread-safe and we want it to
    // cancel without delay.
    connect(this, &GameList::ShouldCancelWorker, worker, &GameListWorker::Cancel,
            Qt::DirectConnection);

    QThreadPool::globalInstance()->start(worker);
    current_worker = std::move(worker);
}

void GameList::SaveInterfaceLayout() {
    UISettings::values.gamelist_header_state = tree_view->header()->saveState();
}

void GameList::LoadInterfaceLayout() {
    auto* header = tree_view->header();

    if (header->restoreState(UISettings::values.gamelist_header_state)) {
        return;
    }

    // We are using the name column to display icons and titles
    // so make it as large as possible as default.
    header->resizeSection(COLUMN_NAME, header->width());
}

const QStringList GameList::supported_file_extensions = {
    QStringLiteral("3ds"), QStringLiteral("3dsx"), QStringLiteral("elf"), QStringLiteral("axf"),
    QStringLiteral("cci"), QStringLiteral("cxi"),  QStringLiteral("app")};

void GameList::RefreshGameDirectory() {
    if (!UISettings::values.game_dirs.isEmpty() && current_worker != nullptr) {
        LOG_INFO(Frontend, "Change detected in the games directory. Reloading game list.");
        PopulateAsync(UISettings::values.game_dirs);
    }
}

QString GameList::FindGameByProgramID(u64 program_id, int role) {
    return FindGameByProgramID(item_model->invisibleRootItem(), program_id, role);
}

QString GameList::FindGameByProgramID(QStandardItem* current_item, u64 program_id, int role) {
    if (current_item->type() == static_cast<int>(GameListItemType::Game) &&
        current_item->data(GameListItemPath::ProgramIdRole).toULongLong() == program_id) {
        return current_item->data(role).toString();
    } else if (current_item->hasChildren()) {
        for (int child_id = 0; child_id < current_item->rowCount(); child_id++) {
            QString path = FindGameByProgramID(current_item->child(child_id, 0), program_id, role);
            if (!path.isEmpty())
                return path;
        }
    }
    return QString();
}

GameListPlaceholder::GameListPlaceholder(GMainWindow* parent) : QWidget{parent} {
    connect(parent, &GMainWindow::UpdateThemedIcons, this,
            &GameListPlaceholder::onUpdateThemedIcons);

    layout = new QVBoxLayout;
    image = new QLabel;
    text = new QLabel;
    layout->setAlignment(Qt::AlignCenter);
    image->setPixmap(QIcon::fromTheme(QStringLiteral("plus_folder")).pixmap(200));

    text->setText(tr("Double-click to add a new folder to the game list"));
    QFont font = text->font();
    font.setPointSize(20);
    text->setFont(font);
    text->setAlignment(Qt::AlignHCenter);
    image->setAlignment(Qt::AlignHCenter);

    layout->addWidget(image);
    layout->addWidget(text);
    setLayout(layout);
}

GameListPlaceholder::~GameListPlaceholder() = default;

void GameListPlaceholder::onUpdateThemedIcons() {
    image->setPixmap(QIcon::fromTheme(QStringLiteral("plus_folder")).pixmap(200));
}

void GameListPlaceholder::mouseDoubleClickEvent([[maybe_unused]] QMouseEvent* event) {
    emit GameListPlaceholder::AddDirectory();
}
