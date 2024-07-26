// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <QBoxLayout>
#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <nihstro/float24.h>
#include "common/common_types.h"
#include "core/core.h"
#include "core/tracer/recorder.h"
#include "lime_qt/debugger/graphics/graphics_tracing.h"
#include "video_core/gpu.h"
#include "video_core/pica/pica_core.h"

GraphicsTracingWidget::GraphicsTracingWidget(Core::System& system_,
                                             std::shared_ptr<Pica::DebugContext> debug_context,
                                             QWidget* parent)
    : BreakPointObserverDock(debug_context, tr("CiTrace Recorder"), parent), system{system_} {

    setObjectName(QStringLiteral("CiTracing"));

    QPushButton* start_recording = new QPushButton(tr("Start Recording"));
    QPushButton* stop_recording =
        new QPushButton(QIcon::fromTheme(QStringLiteral("document-save")), tr("Stop and Save"));
    QPushButton* abort_recording = new QPushButton(tr("Abort Recording"));

    connect(this, &GraphicsTracingWidget::SetStartTracingButtonEnabled, start_recording,
            &QPushButton::setVisible);
    connect(this, &GraphicsTracingWidget::SetStopTracingButtonEnabled, stop_recording,
            &QPushButton::setVisible);
    connect(this, &GraphicsTracingWidget::SetAbortTracingButtonEnabled, abort_recording,
            &QPushButton::setVisible);
    connect(start_recording, &QPushButton::clicked, this, &GraphicsTracingWidget::StartRecording);
    connect(stop_recording, &QPushButton::clicked, this, &GraphicsTracingWidget::StopRecording);
    connect(abort_recording, &QPushButton::clicked, this, &GraphicsTracingWidget::AbortRecording);

    stop_recording->setVisible(false);
    abort_recording->setVisible(false);

    auto main_widget = new QWidget;
    auto main_layout = new QVBoxLayout;
    {
        auto sub_layout = new QHBoxLayout;
        sub_layout->addWidget(start_recording);
        sub_layout->addWidget(stop_recording);
        sub_layout->addWidget(abort_recording);
        main_layout->addLayout(sub_layout);
    }
    main_widget->setLayout(main_layout);
    setWidget(main_widget);
}

void GraphicsTracingWidget::StartRecording() {
    auto context = context_weak.lock();
    if (!context)
        return;

    auto& pica = system.GPU().PicaCore();
    auto shader_binary = pica.vs_setup.program_code;
    auto swizzle_data = pica.vs_setup.swizzle_data;

    // Encode floating point numbers to 24-bit values
    // TODO: Drop this explicit conversion once we store float24 values bit-correctly internally.
    std::array<u32, 4 * 16> default_attributes;
    for (u32 i = 0; i < 16; ++i) {
        for (u32 comp = 0; comp < 3; ++comp) {
            default_attributes[4 * i + comp] =
                nihstro::to_float24(pica.input_default_attributes[i][comp].ToFloat32());
        }
    }

    std::array<u32, 4 * 96> vs_float_uniforms;
    for (u32 i = 0; i < 96; ++i) {
        for (u32 comp = 0; comp < 3; ++comp) {
            vs_float_uniforms[4 * i + comp] =
                nihstro::to_float24(pica.vs_setup.uniforms.f[i][comp].ToFloat32());
        }
    }

    CiTrace::Recorder::InitialState state;

    const auto copy = [&](std::vector<u32>& dest, auto& data) {
        dest.resize(sizeof(data));
        std::memcpy(dest.data(), std::addressof(data), sizeof(data));
    };

    copy(state.pica_registers, pica.regs);
    copy(state.lcd_registers, pica.regs_lcd);
    copy(state.default_attributes, default_attributes);
    copy(state.vs_program_binary, shader_binary);
    copy(state.vs_swizzle_data, swizzle_data);
    copy(state.vs_float_uniforms, vs_float_uniforms);
    // copy(TODO: Not implemented, std::back_inserter(state.gs_program_binary));
    // copy(TODO: Not implemented, std::back_inserter(state.gs_swizzle_data));
    // copy(TODO: Not implemented, std::back_inserter(state.gs_float_uniforms));

    context->recorder = std::make_shared<CiTrace::Recorder>(state);

    emit SetStartTracingButtonEnabled(false);
    emit SetStopTracingButtonEnabled(true);
    emit SetAbortTracingButtonEnabled(true);
}

void GraphicsTracingWidget::StopRecording() {
    auto context = context_weak.lock();
    if (!context)
        return;

    QString filename = QFileDialog::getSaveFileName(
        this, tr("Save CiTrace"), QStringLiteral("citrace.ctf"), tr("CiTrace File (*.ctf)"));

    if (filename.isEmpty()) {
        // If the user canceled the dialog, keep recording
        return;
    }

    context->recorder->Finish(filename.toStdString());
    context->recorder = nullptr;

    emit SetStopTracingButtonEnabled(false);
    emit SetAbortTracingButtonEnabled(false);
    emit SetStartTracingButtonEnabled(true);
}

void GraphicsTracingWidget::AbortRecording() {
    auto context = context_weak.lock();
    if (!context)
        return;

    context->recorder = nullptr;

    emit SetStopTracingButtonEnabled(false);
    emit SetAbortTracingButtonEnabled(false);
    emit SetStartTracingButtonEnabled(true);
}

void GraphicsTracingWidget::OnBreakPointHit(Pica::DebugContext::Event event, const void* data) {
    widget()->setEnabled(true);
}

void GraphicsTracingWidget::OnResumed() {
    widget()->setEnabled(false);
}

void GraphicsTracingWidget::OnEmulationStarting(EmuThread* emu_thread) {
    // Disable tracing starting/stopping until a GPU breakpoint is reached
    widget()->setEnabled(false);
}

void GraphicsTracingWidget::OnEmulationStopping() {
    // TODO: Is it safe to access the context here?

    auto context = context_weak.lock();
    if (!context)
        return;

    if (context->recorder) {
        auto reply =
            QMessageBox::question(this, tr("CiTracing still active"),
                                  tr("A CiTrace is still being recorded. Do you want to save it? "
                                     "If not, all recorded data will be discarded."),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

        if (reply == QMessageBox::Yes) {
            StopRecording();
        } else {
            AbortRecording();
        }
    }

    // If the widget was disabled before, enable it now to allow starting
    // tracing before starting the next emulation session
    widget()->setEnabled(true);
}
