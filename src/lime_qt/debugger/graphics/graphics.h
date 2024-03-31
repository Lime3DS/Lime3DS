// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QAbstractListModel>
#include <QDockWidget>
#include "video_core/gpu_debugger.h"

namespace Core {
class System;
}

class GPUCommandStreamItemModel : public QAbstractListModel,
                                  public VideoCore::GraphicsDebugger::DebuggerObserver {
    Q_OBJECT

public:
    explicit GPUCommandStreamItemModel(QObject* parent);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

public:
    void GXCommandProcessed(int total_command_count) override;

public slots:
    void OnGXCommandFinishedInternal(int total_command_count);

signals:
    void GXCommandFinished(int total_command_count);

private:
    int command_count;
};

class GPUCommandStreamWidget : public QDockWidget {
    Q_OBJECT

public:
    GPUCommandStreamWidget(Core::System& system, QWidget* parent = nullptr);

    void Register();
    void Unregister();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    Core::System& system;
    GPUCommandStreamItemModel model;
};
