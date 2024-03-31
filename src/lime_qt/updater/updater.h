// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Based on the original work by Felix Barx
// Copyright (c) 2015, Felix Barz
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// * Neither the name of QtAutoUpdater nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <memory>
#include <QDateTime>
#include <QList>
#include <QScopedPointer>
#include <QString>
#include <QStringList>

class UpdaterPrivate;

/// The main updater. Can check for updates and run the maintenancetool as updater
class Updater : public QObject {
    Q_OBJECT;

    /// Specifies whether the updater is currently checking for updates or not
    Q_PROPERTY(bool running READ IsRunning NOTIFY RunningChanged);
    /// Holds extended information about the last update check
    Q_PROPERTY(QList<UpdateInfo> update_info READ LatestUpdateInfo NOTIFY UpdateInfoChanged);

public:
    /// Provides information about updates for components
    struct UpdateInfo {
        /// The name of the component that has an update
        QString name;
        /// The new version for that compontent
        QString version;
        /// The update download size (in Bytes)
        quint64 size = 0;

        /**
         * Default Constructor
         */
        UpdateInfo();

        /**
         * Copy Constructor
         */
        UpdateInfo(const UpdateInfo& other);

        /**
         * Constructor that takes name, version and size.
         */
        UpdateInfo(QString name, QString version, quint64 size);
    };

    /**
     * Default constructor
     **/
    explicit Updater(QObject* parent = nullptr);

    /**
     * Constructor with an explicitly set path
     **/
    explicit Updater(const QString& maintenance_tool_path, QObject* parent = nullptr);

    /**
     * Destroys the updater and kills the update check (if running)
     **/
    ~Updater();

    /**
     * Returns `true`, if the updater exited normally
     **/
    bool ExitedNormally() const;

    /**
     * Returns the mainetancetools error code from the last update check, if any.
     **/
    int ErrorCode() const;

    /**
     * Returns the error output (stderr) of the last update
     **/
    QByteArray ErrorLog() const;

    /**
     * Returns if a update check is running.
     **/
    bool IsRunning() const;

    /**
     * Returns the latest update information available, if any.
     **/
    QList<UpdateInfo> LatestUpdateInfo() const;

    /**
     * Launches the updater UI formally
     **/
    void LaunchUI();

    /**
     * Silently updates the application in the background
     **/
    void SilentlyUpdate();

    /**
     * Checks to see if a updater application is available
     **/
    bool HasUpdater() const;

    /**
     * Instead of silently updating, explictly open the UI on shutdown
     **/
    void LaunchUIOnExit();

public slots:
    /**
     * Starts checking for updates
     **/
    bool CheckForUpdates();

    /**
     * Aborts checking for updates
     **/
    void AbortUpdateCheck(int max_delay = 5000, bool async = false);

signals:
    /**
     * Will be emitted as soon as the updater finished checking for updates
     **/
    void CheckUpdatesDone(bool has_updates, bool has_error);

    /**
     * Emitted when a update check operation changes stage
     **/
    void RunningChanged(bool running);

    /**
     * Emitted when new update information has been found
     **/
    void UpdateInfoChanged(QList<Updater::UpdateInfo> update_info);

private:
    std::unique_ptr<UpdaterPrivate> backend;
};
