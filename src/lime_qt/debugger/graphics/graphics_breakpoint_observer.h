// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QDockWidget>
#include "video_core/debug_utils/debug_utils.h"

/**
 * Utility class which forwards calls to OnPicaBreakPointHit and OnPicaResume to public slots.
 * This is because the Pica breakpoint callbacks are called from a non-GUI thread, while
 * the widget usually wants to perform reactions in the GUI thread.
 */
class BreakPointObserverDock : public QDockWidget,
                               protected Pica::DebugContext::BreakPointObserver {
    Q_OBJECT

public:
    BreakPointObserverDock(std::shared_ptr<Pica::DebugContext> debug_context, const QString& title,
                           QWidget* parent = nullptr);

    void OnPicaBreakPointHit(Pica::DebugContext::Event event, const void* data) override;
    void OnPicaResume() override;

signals:
    void Resumed();
    void BreakPointHit(Pica::DebugContext::Event event, const void* data);

private:
    virtual void OnBreakPointHit(Pica::DebugContext::Event event, const void* data) = 0;
    virtual void OnResumed() = 0;
};
