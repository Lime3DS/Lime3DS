// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <QList>
#include <QObject>
#include <QRunnable>
#include <QString>
#include <QVector>
#include "common/common_types.h"
#include "lime_qt/compatibility_list.h"
#include "lime_qt/play_time_manager.h"

namespace Service::FS {
enum class MediaType : u32;
}

class QStandardItem;

/**
 * Asynchronous worker object for populating the game list.
 * Communicates with other threads through Qt's signal/slot system.
 */
class GameListWorker : public QObject, public QRunnable {
    Q_OBJECT

public:
    GameListWorker(QVector<UISettings::GameDir>& game_dirs,
                   const CompatibilityList& compatibility_list,
                   const PlayTime::PlayTimeManager& play_time_manager_);
    ~GameListWorker() override;

    /// Starts the processing of directory tree information.
    void run() override;

    /// Tells the worker that it should no longer continue processing. Thread-safe.
    void Cancel();

signals:
    /**
     * The `EntryReady` signal is emitted once an entry has been prepared and is ready
     * to be added to the game list.
     * @param entry_items a list with `QStandardItem`s that make up the columns of the new entry.
     */
    void DirEntryReady(GameListDir* entry_items);
    void EntryReady(QList<QStandardItem*> entry_items, GameListDir* parent_dir);

    /**
     * After the worker has traversed the game directory looking for entries, this signal is emitted
     * with a list of folders that should be watched for changes as well.
     */
    void Finished(QStringList watch_list);

private:
    void AddFstEntriesToGameList(const std::string& dir_path, unsigned int recursion,
                                 GameListDir* parent_dir, Service::FS::MediaType media_type);

    QVector<UISettings::GameDir>& game_dirs;
    const CompatibilityList& compatibility_list;
    const PlayTime::PlayTimeManager& play_time_manager;

    QStringList watch_list;
    std::atomic_bool stop_processing;
};
