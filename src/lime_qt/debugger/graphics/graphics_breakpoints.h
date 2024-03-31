// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDockWidget>
#include "video_core/debug_utils/debug_utils.h"

class QLabel;
class QPushButton;
class QTreeView;

class BreakPointModel;

class GraphicsBreakPointsWidget : public QDockWidget, Pica::DebugContext::BreakPointObserver {
    Q_OBJECT

    using Event = Pica::DebugContext::Event;

public:
    explicit GraphicsBreakPointsWidget(std::shared_ptr<Pica::DebugContext> debug_context,
                                       QWidget* parent = nullptr);

    void OnPicaBreakPointHit(Pica::DebugContext::Event event, const void* data) override;
    void OnPicaResume() override;

signals:
    void Resumed();
    void BreakPointHit(Pica::DebugContext::Event event, const void* data);
    void BreakPointsChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

private:
    void OnBreakPointHit(Pica::DebugContext::Event event, const void* data);
    void OnItemDoubleClicked(const QModelIndex&);
    void OnResumeRequested();
    void OnResumed();

    QLabel* status_text;
    QPushButton* resume_button;

    BreakPointModel* breakpoint_model;
    QTreeView* breakpoint_list;
};
