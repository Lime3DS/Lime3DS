// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QBrush>
#include <QString>
#include <QTreeWidgetItem>
#include <QtGlobal>
#include <fmt/format.h>
#include "common/assert.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/kernel/ipc_debugger/recorder.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/sm/sm.h"
#include "lime_qt/debugger/ipc/record_dialog.h"
#include "lime_qt/debugger/ipc/recorder.h"
#include "ui_recorder.h"

IPCRecorderWidget::IPCRecorderWidget(Core::System& system_, QWidget* parent)
    : QDockWidget(parent), ui(std::make_unique<Ui::IPCRecorder>()), system{system_} {

    ui->setupUi(this);
    qRegisterMetaType<IPCDebugger::RequestRecord>();

#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
    connect(ui->enabled, &QCheckBox::stateChanged, this,
            [this](int new_state) { SetEnabled(new_state == Qt::Checked); });
#else
    connect(ui->enabled, &QCheckBox::checkStateChanged, this,
            [this](int new_state) { SetEnabled(new_state == Qt::Checked); });
#endif
    connect(ui->clearButton, &QPushButton::clicked, this, &IPCRecorderWidget::Clear);
    connect(ui->filter, &QLineEdit::textChanged, this, &IPCRecorderWidget::ApplyFilterToAll);
    connect(ui->main, &QTreeWidget::itemDoubleClicked, this, &IPCRecorderWidget::OpenRecordDialog);
    connect(this, &IPCRecorderWidget::EntryUpdated, this, &IPCRecorderWidget::OnEntryUpdated);
}

IPCRecorderWidget::~IPCRecorderWidget() = default;

void IPCRecorderWidget::OnEmulationStarting() {
    Clear();
    id_offset = 1;

    // Update the enabled status when the system is powered on.
    SetEnabled(ui->enabled->isChecked());
}

QString IPCRecorderWidget::GetStatusStr(const IPCDebugger::RequestRecord& record) const {
    switch (record.status) {
    case IPCDebugger::RequestStatus::Invalid:
        return tr("Invalid");
    case IPCDebugger::RequestStatus::Sent:
        return tr("Sent");
    case IPCDebugger::RequestStatus::Handling:
        return tr("Handling");
    case IPCDebugger::RequestStatus::Handled:
        if (record.translated_reply_cmdbuf[1] == ResultSuccess.raw) {
            return tr("Success");
        }
        return tr("Error");
    case IPCDebugger::RequestStatus::HLEUnimplemented:
        return tr("HLE Unimplemented");
    default:
        UNREACHABLE();
        return QLatin1String{};
    }
}

void IPCRecorderWidget::OnEntryUpdated(IPCDebugger::RequestRecord record) {
    if (record.id < id_offset) { // The record has already been deleted by 'Clear'
        return;
    }

    QString service = GetServiceName(record);
    if (record.status == IPCDebugger::RequestStatus::Handling ||
        record.status == IPCDebugger::RequestStatus::Handled ||
        record.status == IPCDebugger::RequestStatus::HLEUnimplemented) {

        service = QStringLiteral("%1 (%2)").arg(service, record.is_hle ? tr("HLE") : tr("LLE"));
    }

    QTreeWidgetItem entry{
        {QString::number(record.id), GetStatusStr(record), service, GetFunctionName(record)}};

    const int row_id = record.id - id_offset;
    if (ui->main->invisibleRootItem()->childCount() > row_id) {
        records[row_id] = record;
        (*ui->main->invisibleRootItem()->child(row_id)) = entry;
    } else {
        records.emplace_back(record);
        ui->main->invisibleRootItem()->addChild(new QTreeWidgetItem(entry));
    }

    if (record.status == IPCDebugger::RequestStatus::HLEUnimplemented ||
        (record.status == IPCDebugger::RequestStatus::Handled &&
         record.translated_reply_cmdbuf[1] != ResultSuccess.raw)) { // Unimplemented / Error

        auto item = ui->main->invisibleRootItem()->child(row_id);
        for (int column = 0; column < item->columnCount(); ++column) {
            item->setBackground(column, QBrush(QColor::fromRgb(255, 0, 0)));
        }
    }

    ApplyFilter(row_id);
}

void IPCRecorderWidget::SetEnabled(bool enabled) {
    if (!system.IsPoweredOn()) {
        return;
    }

    auto& ipc_recorder = system.Kernel().GetIPCRecorder();
    ipc_recorder.SetEnabled(enabled);

    if (enabled) {
        handle = ipc_recorder.BindCallback(
            [this](const IPCDebugger::RequestRecord& record) { emit EntryUpdated(record); });
    } else if (handle) {
        ipc_recorder.UnbindCallback(handle);
    }
}

void IPCRecorderWidget::Clear() {
    id_offset += static_cast<int>(records.size());

    records.clear();
    ui->main->invisibleRootItem()->takeChildren();
}

QString IPCRecorderWidget::GetServiceName(const IPCDebugger::RequestRecord& record) const {
    if (system.IsPoweredOn() && record.client_port.id != -1) {
        const Service::SM::ServiceManager& sm = system.ServiceManager();
        const u32 port_id = static_cast<u32>(record.client_port.id);
        const auto service_name = sm.GetServiceNameByPortId(port_id);

        if (!service_name.empty()) {
            return QString::fromStdString(service_name);
        }
    }

    // Get a similar result from the server session name
    std::string session_name = record.server_session.name;
    session_name = Common::ReplaceAll(session_name, "_Server", "");
    session_name = Common::ReplaceAll(session_name, "_Client", "");
    return QString::fromStdString(session_name);
}

QString IPCRecorderWidget::GetFunctionName(const IPCDebugger::RequestRecord& record) const {
    if (record.untranslated_request_cmdbuf.empty()) { // Cmdbuf is not yet available
        return tr("Unknown");
    }
    const QString header_code =
        QStringLiteral("0x%1").arg(record.untranslated_request_cmdbuf[0], 8, 16, QLatin1Char('0'));
    if (record.function_name.empty()) {
        return header_code;
    }
    return QStringLiteral("%1 (%2)").arg(QString::fromStdString(record.function_name), header_code);
}

void IPCRecorderWidget::ApplyFilter(int index) {
    auto* item = ui->main->invisibleRootItem()->child(index);
    const QString filter = ui->filter->text();
    if (filter.isEmpty()) {
        item->setHidden(false);
        return;
    }

    for (int i = 0; i < item->columnCount(); ++i) {
        if (item->text(i).contains(filter)) {
            item->setHidden(false);
            return;
        }
    }

    item->setHidden(true);
}

void IPCRecorderWidget::ApplyFilterToAll() {
    for (int i = 0; i < ui->main->invisibleRootItem()->childCount(); ++i) {
        ApplyFilter(i);
    }
}

void IPCRecorderWidget::OpenRecordDialog(QTreeWidgetItem* item, [[maybe_unused]] int column) {
    int index = ui->main->invisibleRootItem()->indexOfChild(item);

    RecordDialog dialog(this, records[static_cast<std::size_t>(index)], item->text(2),
                        item->text(3));
    dialog.exec();
}
