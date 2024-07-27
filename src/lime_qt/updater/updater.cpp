// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Based on the original work by Felix Barx
// Copyright (c) 2015, Felix Barz
// All rights reserved.

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QXmlStreamReader>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "lime_qt/uisettings.h"
#include "lime_qt/updater/updater.h"
#include "lime_qt/updater/updater_p.h"

#ifdef Q_OS_MACOS
#define DEFAULT_TOOL_PATH QStringLiteral("../../../../maintenancetool")
#else
#define DEFAULT_TOOL_PATH QStringLiteral("../maintenancetool")
#endif

Updater::Updater(QObject* parent) : Updater(DEFAULT_TOOL_PATH, parent) {}

Updater::Updater(const QString& maintenance_tool_path, QObject* parent)
    : QObject(parent), backend(std::make_unique<UpdaterPrivate>(this)) {
    backend->tool_path = UpdaterPrivate::ToSystemExe(maintenance_tool_path);
}

Updater::~Updater() = default;

bool Updater::ExitedNormally() const {
    return backend->normal_exit;
}

int Updater::ErrorCode() const {
    return backend->last_error_code;
}

QByteArray Updater::ErrorLog() const {
    return backend->last_error_log;
}

bool Updater::IsRunning() const {
    return backend->running;
}

QList<Updater::UpdateInfo> Updater::LatestUpdateInfo() const {
    return backend->update_info;
}

bool Updater::HasUpdater() const {
    return backend->HasUpdater();
}

bool Updater::CheckForUpdates() {
    return backend->StartUpdateCheck();
}

void Updater::AbortUpdateCheck(int max_delay, bool async) {
    backend->StopUpdateCheck(max_delay, async);
}

void Updater::LaunchUI() {
    backend->LaunchUI();
}

void Updater::SilentlyUpdate() {
    backend->SilentlyUpdate();
}

void Updater::LaunchUIOnExit() {
    backend->LaunchUIOnExit();
}

Updater::UpdateInfo::UpdateInfo() = default;

Updater::UpdateInfo::UpdateInfo(const Updater::UpdateInfo&) = default;

Updater::UpdateInfo::UpdateInfo(QString name, QString version, quint64 size)
    : name(std::move(name)), version(std::move(version)), size(size) {}

UpdaterPrivate::UpdaterPrivate(Updater* parent_ptr) : QObject(nullptr), parent(parent_ptr) {
    connect(qApp, &QCoreApplication::aboutToQuit, this, &UpdaterPrivate::AboutToExit,
            Qt::DirectConnection);
    qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");
}

UpdaterPrivate::~UpdaterPrivate() {
    if (main_process && main_process->state() != QProcess::NotRunning) {
        main_process->kill();
        main_process->waitForFinished(1000);
    }
}

QString UpdaterPrivate::ToSystemExe(QString base_path) {
#if defined(Q_OS_WIN32)
    if (!base_path.endsWith(QStringLiteral(".exe")))
        return base_path + QStringLiteral(".exe");
    else
        return base_path;
#elif defined(Q_OS_MACOS)
    if (base_path.endsWith(QStringLiteral(".app")))
        base_path.truncate(base_path.lastIndexOf(QStringLiteral(".")));
    return base_path + QStringLiteral(".app/Contents/MacOS/") + QFileInfo(base_path).fileName();
#elif defined(Q_OS_UNIX)
    return base_path;
#endif
}

QFileInfo UpdaterPrivate::GetMaintenanceTool() const {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const auto appimage_path = QProcessEnvironment::systemEnvironment()
                                   .value(QStringLiteral("APPIMAGE"), {})
                                   .toStdString();
    if (!appimage_path.empty()) {
        const auto appimage_dir = FileUtil::GetParentPath(appimage_path);
        LOG_DEBUG(Frontend, "Detected app image directory: {}", appimage_dir);
        return QFileInfo(QString::fromStdString(std::string(appimage_dir)), tool_path);
    }
#endif
    return QFileInfo(QCoreApplication::applicationDirPath(), tool_path);
}

bool UpdaterPrivate::HasUpdater() const {
    return GetMaintenanceTool().exists();
}

bool UpdaterPrivate::StartUpdateCheck() {
    if (running || !HasUpdater()) {
        return false;
    }

    update_info.clear();
    normal_exit = true;
    last_error_code = EXIT_SUCCESS;
    last_error_log.clear();

    main_process = new QProcess(this);
    main_process->setProgram(GetMaintenanceTool().absoluteFilePath());
    main_process->setArguments({QStringLiteral("--checkupdates")});

    connect(main_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            &UpdaterPrivate::UpdaterReady, Qt::QueuedConnection);
    connect(main_process, qOverload<QProcess::ProcessError>(&QProcess::errorOccurred), this,
            &UpdaterPrivate::UpdaterError, Qt::QueuedConnection);

    main_process->start(QIODevice::ReadOnly);
    running = true;

    emit parent->UpdateInfoChanged(update_info);
    emit parent->RunningChanged(true);
    return true;
}

void UpdaterPrivate::StopUpdateCheck(int delay, bool async) {
    if (main_process == nullptr || main_process->state() == QProcess::NotRunning) {
        return;
    }

    if (delay > 0) {
        main_process->terminate();
        if (async) {
            QTimer* timer = new QTimer(this);
            timer->setSingleShot(true);

            connect(timer, &QTimer::timeout, this, [this, timer]() {
                StopUpdateCheck(0, false);
                timer->deleteLater();
            });

            timer->start(delay);
        } else {
            if (!main_process->waitForFinished(delay)) {
                main_process->kill();
                main_process->waitForFinished(100);
            }
        }
    } else {
        main_process->kill();
        main_process->waitForFinished(100);
    }
}

XMLParseResult UpdaterPrivate::ParseResult(const QByteArray& output,
                                           QList<Updater::UpdateInfo>& out) {
    const auto out_string = QString::fromUtf8(output);
    const auto xml_begin = out_string.indexOf(QStringLiteral("<updates>"));
    if (xml_begin < 0)
        return XMLParseResult::NoUpdate;
    const auto xml_end = out_string.indexOf(QStringLiteral("</updates>"), xml_begin);
    if (xml_end < 0)
        return XMLParseResult::NoUpdate;

    QList<Updater::UpdateInfo> updates;
    QXmlStreamReader reader(out_string.mid(xml_begin, (xml_end + 10) - xml_begin));

    reader.readNextStartElement();
    // should always work because it was search for
    if (reader.name() != QStringLiteral("updates")) {
        return XMLParseResult::InvalidXML;
    }

    while (reader.readNextStartElement()) {
        if (reader.name() != QStringLiteral("update"))
            return XMLParseResult::InvalidXML;

        auto ok = false;
        Updater::UpdateInfo info(
            reader.attributes().value(QStringLiteral("name")).toString(),
            reader.attributes().value(QStringLiteral("version")).toString(),
            reader.attributes().value(QStringLiteral("size")).toULongLong(&ok));

        if (info.name.isEmpty() || info.version.isNull() || !ok)
            return XMLParseResult::InvalidXML;
        if (reader.readNextStartElement())
            return XMLParseResult::InvalidXML;

        updates.append(info);
    }

    if (reader.hasError()) {
        LOG_ERROR(Frontend, "Cannot read xml for update: {}", reader.errorString().toStdString());
        return XMLParseResult::InvalidXML;
    }

    out = updates;
    return XMLParseResult::Success;
}

void UpdaterPrivate::UpdaterReady(int exit_code, QProcess::ExitStatus exit_status) {
    if (main_process == nullptr) {
        return;
    }

    if (exit_status != QProcess::NormalExit) {
        UpdaterError(QProcess::Crashed);
        return;
    }

    normal_exit = true;
    last_error_code = exit_code;
    last_error_log = main_process->readAllStandardError();
    const auto update_out = main_process->readAllStandardOutput();
    main_process->deleteLater();
    main_process = nullptr;

    running = false;
    emit parent->RunningChanged(false);

    QList<Updater::UpdateInfo> update_info;
    auto err = ParseResult(update_out, update_info);
    bool has_error = false;

    if (err == XMLParseResult::Success) {
        if (!update_info.isEmpty())
            emit parent->UpdateInfoChanged(update_info);
    } else if (err == XMLParseResult::InvalidXML) {
        has_error = true;
    }

    emit parent->CheckUpdatesDone(!update_info.isEmpty(), has_error);
}

void UpdaterPrivate::UpdaterError(QProcess::ProcessError error) {
    if (main_process) {
        normal_exit = false;
        last_error_code = error;
        last_error_log = main_process->errorString().toUtf8();
        main_process->deleteLater();
        main_process = nullptr;

        running = false;
        emit parent->RunningChanged(false);
        emit parent->CheckUpdatesDone(false, true);
    }
}

void UpdaterPrivate::LaunchWithArguments(const QStringList& args) {
    if (!HasUpdater()) {
        return;
    }

    QFileInfo tool_info = GetMaintenanceTool();

    if (!QProcess::startDetached(tool_info.absoluteFilePath(), args, tool_info.absolutePath())) {
        LOG_WARNING(Frontend, "Unable to start program {}",
                    tool_info.absoluteFilePath().toStdString());
    }
}

void UpdaterPrivate::LaunchUI() {
    LOG_INFO(Frontend, "Launching update UI...");
    LaunchWithArguments(run_arguments);
}

void UpdaterPrivate::SilentlyUpdate() {
    LOG_INFO(Frontend, "Launching silent update...");
    LaunchWithArguments(silent_arguments);
}

void UpdaterPrivate::AboutToExit() {
    if (launch_ui_on_exit) {
        LaunchUI();
    } else if (UISettings::values.update_on_close) {
        SilentlyUpdate();
    }
}

void UpdaterPrivate::LaunchUIOnExit() {
    launch_ui_on_exit = true;
}
