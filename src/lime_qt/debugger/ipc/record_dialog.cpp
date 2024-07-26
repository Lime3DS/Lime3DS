// Copyright 2019 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fmt/format.h>
#include "common/assert.h"
#include "core/hle/kernel/ipc_debugger/recorder.h"
#include "lime_qt/debugger/ipc/record_dialog.h"
#include "ui_record_dialog.h"

QString RecordDialog::FormatObject(const IPCDebugger::ObjectInfo& object) const {
    if (object.id == -1) {
        return tr("null");
    }

    return QStringLiteral("%1 (0x%2)")
        .arg(QString::fromStdString(object.name))
        .arg(object.id, 8, 16, QLatin1Char('0'));
}

QString RecordDialog::FormatCmdbuf(std::span<const u32> cmdbuf) const {
    QString result;
    for (std::size_t i = 0; i < cmdbuf.size(); ++i) {
        result.append(
            QStringLiteral("[%1]: 0x%2\n").arg(i).arg(cmdbuf[i], 8, 16, QLatin1Char('0')));
    }
    return result;
}

RecordDialog::RecordDialog(QWidget* parent, const IPCDebugger::RequestRecord& record,
                           const QString& service, const QString& function)
    : QDialog(parent), ui(std::make_unique<Ui::RecordDialog>()) {

    ui->setupUi(this);

    ui->clientProcess->setText(FormatObject(record.client_process));
    ui->clientThread->setText(FormatObject(record.client_thread));
    ui->clientSession->setText(FormatObject(record.client_session));

    ui->serverProcess->setText(FormatObject(record.server_process));
    ui->serverThread->setText(FormatObject(record.server_thread));
    ui->serverSession->setText(FormatObject(record.server_session));

    ui->clientPort->setText(FormatObject(record.client_port));
    ui->service->setText(std::move(service));
    ui->function->setText(std::move(function));

    cmdbufs = {record.untranslated_request_cmdbuf, record.translated_request_cmdbuf,
               record.untranslated_reply_cmdbuf, record.translated_reply_cmdbuf};

    UpdateCmdbufDisplay();

    connect(ui->cmdbufSelection, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &RecordDialog::UpdateCmdbufDisplay);
    connect(ui->okButton, &QPushButton::clicked, this, &QDialog::close);
}

RecordDialog::~RecordDialog() = default;

void RecordDialog::UpdateCmdbufDisplay() {
    int index = ui->cmdbufSelection->currentIndex();

    ASSERT_MSG(0 <= index && index <= 3, "Index out of bound");
    ui->cmdbuf->setPlainText(FormatCmdbuf(cmdbufs[index]));
}
