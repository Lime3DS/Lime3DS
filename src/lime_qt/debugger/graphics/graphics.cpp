// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QListView>
#include "core/core.h"
#include "lime_qt/debugger/graphics/graphics.h"
#include "lime_qt/util/util.h"
#include "video_core/gpu.h"

GPUCommandStreamItemModel::GPUCommandStreamItemModel(QObject* parent)
    : QAbstractListModel(parent), command_count(0) {
    connect(this, &GPUCommandStreamItemModel::GXCommandFinished, this,
            &GPUCommandStreamItemModel::OnGXCommandFinishedInternal);
}

int GPUCommandStreamItemModel::rowCount([[maybe_unused]] const QModelIndex& parent) const {
    return command_count;
}

QVariant GPUCommandStreamItemModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !GetDebugger())
        return QVariant();

    int command_index = index.row();
    const Service::GSP::Command& command = GetDebugger()->ReadGXCommandHistory(command_index);
    if (role == Qt::DisplayRole) {
        std::map<Service::GSP::CommandId, const char*> command_names = {
            {Service::GSP::CommandId::RequestDma, "REQUEST_DMA"},
            {Service::GSP::CommandId::SubmitCmdList, "SUBMIT_GPU_CMDLIST"},
            {Service::GSP::CommandId::MemoryFill, "SET_MEMORY_FILL"},
            {Service::GSP::CommandId::DisplayTransfer, "SET_DISPLAY_TRANSFER"},
            {Service::GSP::CommandId::TextureCopy, "SET_TEXTURE_COPY"},
            {Service::GSP::CommandId::CacheFlush, "CACHE_FLUSH"},
        };
        const u32* command_data = reinterpret_cast<const u32*>(&command);
        QString str = QStringLiteral("%1 %2 %3 %4 %5 %6 %7 %8 %9")
                          .arg(QString::fromUtf8(command_names[command.id]))
                          .arg(command_data[0], 8, 16, QLatin1Char('0'))
                          .arg(command_data[1], 8, 16, QLatin1Char('0'))
                          .arg(command_data[2], 8, 16, QLatin1Char('0'))
                          .arg(command_data[3], 8, 16, QLatin1Char('0'))
                          .arg(command_data[4], 8, 16, QLatin1Char('0'))
                          .arg(command_data[5], 8, 16, QLatin1Char('0'))
                          .arg(command_data[6], 8, 16, QLatin1Char('0'))
                          .arg(command_data[7], 8, 16, QLatin1Char('0'));
        return QVariant(str);
    } else {
        return QVariant();
    }
}

void GPUCommandStreamItemModel::GXCommandProcessed(int total_command_count) {
    emit GXCommandFinished(total_command_count);
}

void GPUCommandStreamItemModel::OnGXCommandFinishedInternal(int total_command_count) {
    if (total_command_count == 0)
        return;

    int prev_command_count = command_count;
    command_count = total_command_count;
    emit dataChanged(index(prev_command_count, 0), index(total_command_count - 1, 0));
}

GPUCommandStreamWidget::GPUCommandStreamWidget(Core::System& system_, QWidget* parent)
    : QDockWidget(tr("Graphics Debugger"), parent), system{system_}, model(this) {
    setObjectName(QStringLiteral("GraphicsDebugger"));

    auto* command_list = new QListView;
    command_list->setModel(&model);
    command_list->setFont(GetMonospaceFont());

    setWidget(command_list);
}

void GPUCommandStreamWidget::Register() {
    auto& debugger = system.GPU().Debugger();
    debugger.RegisterObserver(&model);
}

void GPUCommandStreamWidget::Unregister() {
    auto& debugger = system.GPU().Debugger();
    debugger.UnregisterObserver(&model);
}

void GPUCommandStreamWidget::showEvent(QShowEvent* event) {
    if (system.IsPoweredOn()) {
        Register();
    }
    QDockWidget::showEvent(event);
}

void GPUCommandStreamWidget::hideEvent(QHideEvent* event) {
    if (system.IsPoweredOn()) {
        Unregister();
    }
    QDockWidget::hideEvent(event);
}
