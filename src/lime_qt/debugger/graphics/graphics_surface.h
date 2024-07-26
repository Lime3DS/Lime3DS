// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QLabel>
#include <QPushButton>
#include "lime_qt/debugger/graphics/graphics_breakpoint_observer.h"

class QComboBox;
class QSpinBox;
class CSpinBox;

class GraphicsSurfaceWidget;

namespace Core {
class System;
}

class SurfacePicture : public QLabel {
    Q_OBJECT

public:
    explicit SurfacePicture(QWidget* parent = nullptr,
                            GraphicsSurfaceWidget* surface_widget = nullptr);
    ~SurfacePicture() override;

protected slots:
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    GraphicsSurfaceWidget* surface_widget;
};

class GraphicsSurfaceWidget : public BreakPointObserverDock {
    Q_OBJECT

    using Event = Pica::DebugContext::Event;

    enum class Source {
        ColorBuffer = 0,
        DepthBuffer = 1,
        StencilBuffer = 2,
        Texture0 = 3,
        Texture1 = 4,
        Texture2 = 5,
        Custom = 6,
    };

    enum class Format {
        // These must match the TextureFormat type!
        RGBA8 = 0,
        RGB8 = 1,
        RGB5A1 = 2,
        RGB565 = 3,
        RGBA4 = 4,
        IA8 = 5,
        RG8 = 6, ///< @note Also called HILO8 in 3DBrew.
        I8 = 7,
        A8 = 8,
        IA4 = 9,
        I4 = 10,
        A4 = 11,
        ETC1 = 12, // compressed
        ETC1A4 = 13,
        MaxTextureFormat = 13,
        D16 = 14,
        D24 = 15,
        D24X8 = 16,
        X24S8 = 17,
        Unknown = 18,
    };

    static unsigned int NibblesPerPixel(Format format);

public:
    explicit GraphicsSurfaceWidget(Core::System& system,
                                   std::shared_ptr<Pica::DebugContext> debug_context,
                                   QWidget* parent = nullptr);
    void Pick(int x, int y);

public slots:
    void OnSurfaceSourceChanged(int new_value);
    void OnSurfaceAddressChanged(qint64 new_value);
    void OnSurfaceWidthChanged(int new_value);
    void OnSurfaceHeightChanged(int new_value);
    void OnSurfaceFormatChanged(int new_value);
    void OnSurfacePickerXChanged(int new_value);
    void OnSurfacePickerYChanged(int new_value);
    void OnUpdate();

signals:
    void Update();

private:
    void OnBreakPointHit(Pica::DebugContext::Event event, const void* data) override;
    void OnResumed() override;

    void SaveSurface();

    Core::System& system;
    QComboBox* surface_source_list;
    CSpinBox* surface_address_control;
    QSpinBox* surface_width_control;
    QSpinBox* surface_height_control;
    QComboBox* surface_format_control;

    SurfacePicture* surface_picture_label;
    QSpinBox* surface_picker_x_control;
    QSpinBox* surface_picker_y_control;
    QLabel* surface_info_label;
    QPushButton* save_surface;

    Source surface_source;
    unsigned surface_address;
    unsigned surface_width;
    unsigned surface_height;
    Format surface_format;
    int surface_picker_x = 0;
    int surface_picker_y = 0;
};
