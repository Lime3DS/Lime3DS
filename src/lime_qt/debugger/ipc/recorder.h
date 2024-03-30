// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDockWidget>
#include "core/hle/kernel/ipc_debugger/recorder.h"

class QTreeWidgetItem;

namespace Ui {
class IPCRecorder;
}

namespace Core {
class System;
}

class IPCRecorderWidget : public QDockWidget {
    Q_OBJECT

public:
    explicit IPCRecorderWidget(Core::System& system, QWidget* parent = nullptr);
    ~IPCRecorderWidget();

    void OnEmulationStarting();

signals:
    void EntryUpdated(IPCDebugger::RequestRecord record);

private:
    QString GetStatusStr(const IPCDebugger::RequestRecord& record) const;
    void OnEntryUpdated(IPCDebugger::RequestRecord record);
    void SetEnabled(bool enabled);
    void Clear();
    void ApplyFilter(int index);
    void ApplyFilterToAll();
    QString GetServiceName(const IPCDebugger::RequestRecord& record) const;
    QString GetFunctionName(const IPCDebugger::RequestRecord& record) const;
    void OpenRecordDialog(QTreeWidgetItem* item, int column);

private:
    std::unique_ptr<Ui::IPCRecorder> ui;
    IPCDebugger::CallbackHandle handle;
    Core::System& system;
    // The offset between record id and row id, assuming record ids are assigned
    // continuously and only the 'Clear' action can be performed, this is enough.
    // The initial value is 1, which means record 1 = row 0.
    int id_offset = 1;
    std::vector<IPCDebugger::RequestRecord> records;
};

Q_DECLARE_METATYPE(IPCDebugger::RequestRecord);
