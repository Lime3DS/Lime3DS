// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QAbstractListModel>
#include <QDockWidget>
#include "video_core/debug_utils/debug_utils.h"

class QPushButton;
class QTreeView;

namespace Core {
class System;
}

class GPUCommandListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum {
        CommandIdRole = Qt::UserRole,
    };

    explicit GPUCommandListModel(QObject* parent);

    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

public slots:
    void OnPicaTraceFinished(const Pica::DebugUtils::PicaTrace& trace);

private:
    Pica::DebugUtils::PicaTrace pica_trace;
};

class GPUCommandListWidget : public QDockWidget {
    Q_OBJECT

public:
    explicit GPUCommandListWidget(Core::System& system, QWidget* parent = nullptr);

public slots:
    void OnToggleTracing();
    void OnCommandDoubleClicked(const QModelIndex&);

    void SetCommandInfo(const QModelIndex&);

    void CopyAllToClipboard();

signals:
    void TracingFinished(const Pica::DebugUtils::PicaTrace&);

private:
    std::unique_ptr<Pica::DebugUtils::PicaTrace> pica_trace;
    Core::System& system;
    QTreeView* list_widget;
    QWidget* command_info_widget;
    QPushButton* toggle_tracing;
};
