// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "lime_qt/debugger/graphics/graphics_breakpoint_observer.h"

namespace Core {
class System;
}

class EmuThread;

class GraphicsTracingWidget : public BreakPointObserverDock {
    Q_OBJECT

public:
    explicit GraphicsTracingWidget(Core::System& system,
                                   std::shared_ptr<Pica::DebugContext> debug_context,
                                   QWidget* parent = nullptr);

    void OnEmulationStarting(EmuThread* emu_thread);
    void OnEmulationStopping();

private slots:
    void StartRecording();
    void StopRecording();
    void AbortRecording();

    void OnBreakPointHit(Pica::DebugContext::Event event, const void* data) override;
    void OnResumed() override;

signals:
    void SetStartTracingButtonEnabled(bool enable);
    void SetStopTracingButtonEnabled(bool enable);
    void SetAbortTracingButtonEnabled(bool enable);

private:
    Core::System& system;
};
