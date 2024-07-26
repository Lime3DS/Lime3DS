// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QMetaType>
#include "lime_qt/debugger/graphics/graphics_breakpoint_observer.h"

BreakPointObserverDock::BreakPointObserverDock(std::shared_ptr<Pica::DebugContext> debug_context,
                                               const QString& title, QWidget* parent)
    : QDockWidget(title, parent), BreakPointObserver(debug_context) {
    qRegisterMetaType<Pica::DebugContext::Event>("Pica::DebugContext::Event");

    connect(this, &BreakPointObserverDock::Resumed, this, &BreakPointObserverDock::OnResumed);

    // NOTE: This signal is emitted from a non-GUI thread, but connect() takes
    //       care of delaying its handling to the GUI thread.
    connect(this, &BreakPointObserverDock::BreakPointHit, this,
            &BreakPointObserverDock::OnBreakPointHit, Qt::BlockingQueuedConnection);
}

void BreakPointObserverDock::OnPicaBreakPointHit(Pica::DebugContext::Event event,
                                                 const void* data) {
    emit BreakPointHit(event, data);
}

void BreakPointObserverDock::OnPicaResume() {
    emit Resumed();
}
