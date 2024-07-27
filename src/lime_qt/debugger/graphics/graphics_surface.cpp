// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QBoxLayout>
#include <QComboBox>
#include <QDebug>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include "common/color.h"
#include "core/core.h"
#include "core/memory.h"
#include "lime_qt/debugger/graphics/graphics_surface.h"
#include "lime_qt/util/spinbox.h"
#include "video_core/gpu.h"
#include "video_core/pica/pica_core.h"
#include "video_core/texture/texture_decode.h"
#include "video_core/utils.h"

SurfacePicture::SurfacePicture(QWidget* parent, GraphicsSurfaceWidget* surface_widget_)
    : QLabel(parent), surface_widget(surface_widget_) {}

SurfacePicture::~SurfacePicture() = default;

void SurfacePicture::mousePressEvent(QMouseEvent* event) {
    // Only do something while the left mouse button is held down
    if (!(event->buttons() & Qt::LeftButton))
        return;

    const QPixmap pixmap = this->pixmap(Qt::ReturnByValue);
    if (pixmap.isNull()) {
        return;
    }

    if (surface_widget) {
        const auto pos = event->position().toPoint();
        surface_widget->Pick(pos.x() * pixmap.width() / width(),
                             pos.y() * pixmap.height() / height());
    }
}

void SurfacePicture::mouseMoveEvent(QMouseEvent* event) {
    // We also want to handle the event if the user moves the mouse while holding down the LMB
    mousePressEvent(event);
}

GraphicsSurfaceWidget::GraphicsSurfaceWidget(Core::System& system_,
                                             std::shared_ptr<Pica::DebugContext> debug_context,
                                             QWidget* parent)
    : BreakPointObserverDock(debug_context, tr("Pica Surface Viewer"), parent), system{system_},
      surface_source(Source::ColorBuffer) {
    setObjectName(QStringLiteral("PicaSurface"));

    surface_source_list = new QComboBox;
    surface_source_list->addItem(tr("Color Buffer"));
    surface_source_list->addItem(tr("Depth Buffer"));
    surface_source_list->addItem(tr("Stencil Buffer"));
    surface_source_list->addItem(tr("Texture 0"));
    surface_source_list->addItem(tr("Texture 1"));
    surface_source_list->addItem(tr("Texture 2"));
    surface_source_list->addItem(tr("Custom"));
    surface_source_list->setCurrentIndex(static_cast<int>(surface_source));

    surface_address_control = new CSpinBox;
    surface_address_control->SetBase(16);
    surface_address_control->SetRange(0, 0xFFFFFFFF);
    surface_address_control->SetPrefix(QStringLiteral("0x"));

    unsigned max_dimension = 16384; // TODO: Find actual maximum

    surface_width_control = new QSpinBox;
    surface_width_control->setRange(0, max_dimension);

    surface_height_control = new QSpinBox;
    surface_height_control->setRange(0, max_dimension);

    surface_picker_x_control = new QSpinBox;
    surface_picker_x_control->setRange(0, max_dimension - 1);

    surface_picker_y_control = new QSpinBox;
    surface_picker_y_control->setRange(0, max_dimension - 1);

    // Color formats sorted by Pica texture format index
    const QStringList surface_formats{
        QStringLiteral("RGBA8"),
        QStringLiteral("RGB8"),
        QStringLiteral("RGB5A1"),
        QStringLiteral("RGB565"),
        QStringLiteral("RGBA4"),
        QStringLiteral("IA8"),
        QStringLiteral("RG8"),
        QStringLiteral("I8"),
        QStringLiteral("A8"),
        QStringLiteral("IA4"),
        QStringLiteral("I4"),
        QStringLiteral("A4"),
        QStringLiteral("ETC1"),
        QStringLiteral("ETC1A4"),
        QStringLiteral("D16"),
        QStringLiteral("D24"),
        QStringLiteral("D24X8"),
        QStringLiteral("X24S8"),
        tr("Unknown"),
    };

    surface_format_control = new QComboBox;
    surface_format_control->addItems(surface_formats);

    surface_info_label = new QLabel();
    surface_info_label->setWordWrap(true);

    surface_picture_label = new SurfacePicture(0, this);
    surface_picture_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    surface_picture_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    surface_picture_label->setScaledContents(false);

    auto scroll_area = new QScrollArea();
    scroll_area->setBackgroundRole(QPalette::Dark);
    scroll_area->setWidgetResizable(false);
    scroll_area->setWidget(surface_picture_label);

    save_surface = new QPushButton(QIcon::fromTheme(QStringLiteral("document-save")), tr("Save"));

    // Connections
    connect(this, &GraphicsSurfaceWidget::Update, this, &GraphicsSurfaceWidget::OnUpdate);
    connect(surface_source_list, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &GraphicsSurfaceWidget::OnSurfaceSourceChanged);
    connect(surface_address_control, &CSpinBox::ValueChanged, this,
            &GraphicsSurfaceWidget::OnSurfaceAddressChanged);
    connect(surface_width_control, qOverload<int>(&QSpinBox::valueChanged), this,
            &GraphicsSurfaceWidget::OnSurfaceWidthChanged);
    connect(surface_height_control, qOverload<int>(&QSpinBox::valueChanged), this,
            &GraphicsSurfaceWidget::OnSurfaceHeightChanged);
    connect(surface_format_control, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &GraphicsSurfaceWidget::OnSurfaceFormatChanged);
    connect(surface_picker_x_control, qOverload<int>(&QSpinBox::valueChanged), this,
            &GraphicsSurfaceWidget::OnSurfacePickerXChanged);
    connect(surface_picker_y_control, qOverload<int>(&QSpinBox::valueChanged), this,
            &GraphicsSurfaceWidget::OnSurfacePickerYChanged);
    connect(save_surface, &QPushButton::clicked, this, &GraphicsSurfaceWidget::SaveSurface);

    auto main_widget = new QWidget;
    auto main_layout = new QVBoxLayout;
    {
        auto sub_layout = new QHBoxLayout;
        sub_layout->addWidget(new QLabel(tr("Source:")));
        sub_layout->addWidget(surface_source_list);
        main_layout->addLayout(sub_layout);
    }
    {
        auto sub_layout = new QHBoxLayout;
        sub_layout->addWidget(new QLabel(tr("Physical Address:")));
        sub_layout->addWidget(surface_address_control);
        main_layout->addLayout(sub_layout);
    }
    {
        auto sub_layout = new QHBoxLayout;
        sub_layout->addWidget(new QLabel(tr("Width:")));
        sub_layout->addWidget(surface_width_control);
        main_layout->addLayout(sub_layout);
    }
    {
        auto sub_layout = new QHBoxLayout;
        sub_layout->addWidget(new QLabel(tr("Height:")));
        sub_layout->addWidget(surface_height_control);
        main_layout->addLayout(sub_layout);
    }
    {
        auto sub_layout = new QHBoxLayout;
        sub_layout->addWidget(new QLabel(tr("Format:")));
        sub_layout->addWidget(surface_format_control);
        main_layout->addLayout(sub_layout);
    }
    main_layout->addWidget(scroll_area);

    auto info_layout = new QHBoxLayout;
    {
        auto xy_layout = new QVBoxLayout;
        {
            {
                auto sub_layout = new QHBoxLayout;
                sub_layout->addWidget(new QLabel(tr("X:")));
                sub_layout->addWidget(surface_picker_x_control);
                xy_layout->addLayout(sub_layout);
            }
            {
                auto sub_layout = new QHBoxLayout;
                sub_layout->addWidget(new QLabel(tr("Y:")));
                sub_layout->addWidget(surface_picker_y_control);
                xy_layout->addLayout(sub_layout);
            }
        }
        info_layout->addLayout(xy_layout);
        surface_info_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        info_layout->addWidget(surface_info_label);
    }
    main_layout->addLayout(info_layout);

    main_layout->addWidget(save_surface);
    main_widget->setLayout(main_layout);
    setWidget(main_widget);

    // Load current data - TODO: Make sure this works when emulation is not running
    if (debug_context && debug_context->at_breakpoint) {
        emit Update();
        widget()->setEnabled(debug_context->at_breakpoint);
    } else {
        widget()->setEnabled(false);
    }
}

void GraphicsSurfaceWidget::OnBreakPointHit(Pica::DebugContext::Event event, const void* data) {
    emit Update();
    widget()->setEnabled(true);
}

void GraphicsSurfaceWidget::OnResumed() {
    widget()->setEnabled(false);
}

void GraphicsSurfaceWidget::OnSurfaceSourceChanged(int new_value) {
    surface_source = static_cast<Source>(new_value);
    emit Update();
}

void GraphicsSurfaceWidget::OnSurfaceAddressChanged(qint64 new_value) {
    if (surface_address != new_value) {
        surface_address = static_cast<unsigned>(new_value);

        surface_source_list->setCurrentIndex(static_cast<int>(Source::Custom));
        emit Update();
    }
}

void GraphicsSurfaceWidget::OnSurfaceWidthChanged(int new_value) {
    if (surface_width != static_cast<unsigned>(new_value)) {
        surface_width = static_cast<unsigned>(new_value);

        surface_source_list->setCurrentIndex(static_cast<int>(Source::Custom));
        emit Update();
    }
}

void GraphicsSurfaceWidget::OnSurfaceHeightChanged(int new_value) {
    if (surface_height != static_cast<unsigned>(new_value)) {
        surface_height = static_cast<unsigned>(new_value);

        surface_source_list->setCurrentIndex(static_cast<int>(Source::Custom));
        emit Update();
    }
}

void GraphicsSurfaceWidget::OnSurfaceFormatChanged(int new_value) {
    if (surface_format != static_cast<Format>(new_value)) {
        surface_format = static_cast<Format>(new_value);

        surface_source_list->setCurrentIndex(static_cast<int>(Source::Custom));
        emit Update();
    }
}

void GraphicsSurfaceWidget::OnSurfacePickerXChanged(int new_value) {
    if (surface_picker_x != new_value) {
        surface_picker_x = new_value;
        Pick(surface_picker_x, surface_picker_y);
    }
}

void GraphicsSurfaceWidget::OnSurfacePickerYChanged(int new_value) {
    if (surface_picker_y != new_value) {
        surface_picker_y = new_value;
        Pick(surface_picker_x, surface_picker_y);
    }
}

void GraphicsSurfaceWidget::Pick(int x, int y) {
    surface_picker_x_control->setValue(x);
    surface_picker_y_control->setValue(y);

    if (x < 0 || x >= static_cast<int>(surface_width) || y < 0 ||
        y >= static_cast<int>(surface_height)) {
        surface_info_label->setText(tr("Pixel out of bounds"));
        surface_info_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        return;
    }

    const u8* buffer = system.Memory().GetPhysicalPointer(surface_address);
    if (!buffer) {
        surface_info_label->setText(tr("(unable to access pixel data)"));
        surface_info_label->setAlignment(Qt::AlignCenter);
        return;
    }

    const u32 nibbles_per_pixel = GraphicsSurfaceWidget::NibblesPerPixel(surface_format);
    const u32 stride = nibbles_per_pixel * surface_width / 2;
    const bool nibble_mode = (nibbles_per_pixel == 1);
    const u32 bytes_per_pixel = [&] {
        if (nibble_mode) {
            // As nibbles are contained in a byte we still need to access one byte per nibble
            return 1u;
        } else {
            return nibbles_per_pixel / 2;
        }
    }();

    const u32 coarse_y = y & ~7;
    const u32 offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * stride;
    const u8* pixel = buffer + (nibble_mode ? (offset / 2) : offset);

    const auto get_text = [offset](Format format, const u8* pixel) {
        switch (format) {
        case Format::RGBA8: {
            const auto value = Common::Color::DecodeRGBA8(pixel) / 255.0f;
            return QStringLiteral("Red: %1, Green: %2, Blue: %3, Alpha: %4")
                .arg(QString::number(value.r(), 'f', 2), QString::number(value.g(), 'f', 2),
                     QString::number(value.b(), 'f', 2), QString::number(value.a(), 'f', 2));
        }
        case Format::RGB8: {
            const auto value = Common::Color::DecodeRGB8(pixel) / 255.0f;
            return QStringLiteral("Red: %1, Green: %2, Blue: %3")
                .arg(QString::number(value.r(), 'f', 2), QString::number(value.g(), 'f', 2),
                     QString::number(value.b(), 'f', 2));
        }
        case Format::RGB5A1: {
            const auto value = Common::Color::DecodeRGB5A1(pixel) / 255.0f;
            return QStringLiteral("Red: %1, Green: %2, Blue: %3, Alpha: %4")
                .arg(QString::number(value.r(), 'f', 2), QString::number(value.g(), 'f', 2),
                     QString::number(value.b(), 'f', 2), QString::number(value.a(), 'f', 2));
        }
        case Format::RGB565: {
            const auto value = Common::Color::DecodeRGB565(pixel) / 255.0f;
            return QStringLiteral("Red: %1, Green: %2, Blue: %3")
                .arg(QString::number(value.r(), 'f', 2), QString::number(value.g(), 'f', 2),
                     QString::number(value.b(), 'f', 2));
        }
        case Format::RGBA4: {
            const auto value = Common::Color::DecodeRGBA4(pixel) / 255.0f;
            return QStringLiteral("Red: %1, Green: %2, Blue: %3, Alpha: %4")
                .arg(QString::number(value.r(), 'f', 2), QString::number(value.g(), 'f', 2),
                     QString::number(value.b(), 'f', 2), QString::number(value.a(), 'f', 2));
        }
        case Format::IA8:
            return QStringLiteral("Index: %1, Alpha: %2").arg(pixel[0], pixel[1]);
        case Format::RG8: {
            const auto value = Common::Color::DecodeRG8(pixel) / 255.0f;
            return QStringLiteral("Red: %1, Green: %2")
                .arg(QString::number(value.r(), 'f', 2), QString::number(value.g(), 'f', 2));
        }
        case Format::I8:
            return QStringLiteral("Index: %1").arg(*pixel);
        case Format::A8:
            return QStringLiteral("Alpha: %1").arg(QString::number(*pixel / 255.0f, 'f', 2));
        case Format::IA4:
            return QStringLiteral("Index: %1, Alpha: %2").arg(*pixel & 0xF, (*pixel & 0xF0) >> 4);
        case Format::I4: {
            const u8 i = (*pixel >> ((offset % 2) ? 4 : 0)) & 0xF;
            return QStringLiteral("Index: %1").arg(i);
        }
        case Format::A4: {
            const u8 a = (*pixel >> ((offset % 2) ? 4 : 0)) & 0xF;
            return QStringLiteral("Alpha: %1").arg(QString::number(a / 15.0f, 'f', 2));
        }
        case Format::ETC1:
        case Format::ETC1A4:
            // TODO: Display block information or channel values?
            return QStringLiteral("Compressed data");
        case Format::D16: {
            const auto value = Common::Color::DecodeD16(pixel);
            return QStringLiteral("Depth: %1").arg(QString::number(value / (float)0xFFFF, 'f', 4));
        }
        case Format::D24: {
            const auto value = Common::Color::DecodeD24(pixel);
            return QStringLiteral("Depth: %1")
                .arg(QString::number(value / (float)0xFFFFFF, 'f', 4));
        }
        case Format::D24X8:
        case Format::X24S8: {
            const auto values = Common::Color::DecodeD24S8(pixel);
            return QStringLiteral("Depth: %1, Stencil: %2")
                .arg(QString::number(values[0] / (float)0xFFFFFF, 'f', 4), values[1]);
        }
        case Format::Unknown:
            return QStringLiteral("Unknown format");
        default:
            return QStringLiteral("Unhandled format");
        }
    };

    QString nibbles;
    for (u32 i = 0; i < nibbles_per_pixel; i++) {
        u32 nibble_index = i;
        if (nibble_mode) {
            nibble_index += (offset % 2) ? 0 : 1;
        }
        const u8 byte = pixel[nibble_index / 2];
        const u8 nibble = (byte >> ((nibble_index % 2) ? 0 : 4)) & 0xF;
        nibbles.append(QString::number(nibble, 16).toUpper());
    }

    surface_info_label->setText(
        QStringLiteral("Raw: 0x%3\n(%4)").arg(nibbles, get_text(surface_format, pixel)));
    surface_info_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

void GraphicsSurfaceWidget::OnUpdate() {
    QPixmap pixmap;

    const auto& regs = system.GPU().PicaCore().regs.internal;
    switch (surface_source) {
    case Source::ColorBuffer: {
        // TODO: Store a reference to the registers in the debug context instead of accessing them
        // directly...

        const auto& framebuffer = regs.framebuffer.framebuffer;
        surface_address = framebuffer.GetColorBufferPhysicalAddress();
        surface_width = framebuffer.GetWidth();
        surface_height = framebuffer.GetHeight();

        switch (framebuffer.color_format) {
        case Pica::FramebufferRegs::ColorFormat::RGBA8:
            surface_format = Format::RGBA8;
            break;

        case Pica::FramebufferRegs::ColorFormat::RGB8:
            surface_format = Format::RGB8;
            break;

        case Pica::FramebufferRegs::ColorFormat::RGB5A1:
            surface_format = Format::RGB5A1;
            break;

        case Pica::FramebufferRegs::ColorFormat::RGB565:
            surface_format = Format::RGB565;
            break;

        case Pica::FramebufferRegs::ColorFormat::RGBA4:
            surface_format = Format::RGBA4;
            break;

        default:
            surface_format = Format::Unknown;
            break;
        }

        break;
    }

    case Source::DepthBuffer: {
        const auto& framebuffer = regs.framebuffer.framebuffer;
        surface_address = framebuffer.GetDepthBufferPhysicalAddress();
        surface_width = framebuffer.GetWidth();
        surface_height = framebuffer.GetHeight();

        switch (framebuffer.depth_format) {
        case Pica::FramebufferRegs::DepthFormat::D16:
            surface_format = Format::D16;
            break;

        case Pica::FramebufferRegs::DepthFormat::D24:
            surface_format = Format::D24;
            break;

        case Pica::FramebufferRegs::DepthFormat::D24S8:
            surface_format = Format::D24X8;
            break;

        default:
            surface_format = Format::Unknown;
            break;
        }

        break;
    }

    case Source::StencilBuffer: {
        const auto& framebuffer = regs.framebuffer.framebuffer;
        surface_address = framebuffer.GetDepthBufferPhysicalAddress();
        surface_width = framebuffer.GetWidth();
        surface_height = framebuffer.GetHeight();

        switch (framebuffer.depth_format) {
        case Pica::FramebufferRegs::DepthFormat::D24S8:
            surface_format = Format::X24S8;
            break;

        default:
            surface_format = Format::Unknown;
            break;
        }

        break;
    }

    case Source::Texture0:
    case Source::Texture1:
    case Source::Texture2: {
        unsigned texture_index;
        if (surface_source == Source::Texture0)
            texture_index = 0;
        else if (surface_source == Source::Texture1)
            texture_index = 1;
        else if (surface_source == Source::Texture2)
            texture_index = 2;
        else {
            qDebug() << "Unknown texture source " << static_cast<int>(surface_source);
            break;
        }

        const auto texture = regs.texturing.GetTextures()[texture_index];
        auto info = Pica::Texture::TextureInfo::FromPicaRegister(texture.config, texture.format);

        surface_address = info.physical_address;
        surface_width = info.width;
        surface_height = info.height;
        surface_format = static_cast<Format>(info.format);

        if (surface_format > Format::MaxTextureFormat) {
            qDebug() << "Unknown texture format " << static_cast<int>(info.format);
        }
        break;
    }

    case Source::Custom: {
        // Keep user-specified values
        break;
    }

    default:
        qDebug() << "Unknown surface source " << static_cast<int>(surface_source);
        break;
    }

    surface_address_control->SetValue(surface_address);
    surface_width_control->setValue(surface_width);
    surface_height_control->setValue(surface_height);
    surface_format_control->setCurrentIndex(static_cast<int>(surface_format));

    // TODO: Implement a good way to visualize alpha components!

    QImage decoded_image(surface_width, surface_height, QImage::Format_ARGB32);
    const u8* buffer = system.Memory().GetPhysicalPointer(surface_address);

    if (!buffer) {
        surface_picture_label->hide();
        surface_info_label->setText(tr("(invalid surface address)"));
        surface_info_label->setAlignment(Qt::AlignCenter);
        surface_picker_x_control->setEnabled(false);
        surface_picker_y_control->setEnabled(false);
        save_surface->setEnabled(false);
        return;
    }

    if (surface_format == Format::Unknown) {
        surface_picture_label->hide();
        surface_info_label->setText(tr("(unknown surface format)"));
        surface_info_label->setAlignment(Qt::AlignCenter);
        surface_picker_x_control->setEnabled(false);
        surface_picker_y_control->setEnabled(false);
        save_surface->setEnabled(false);
        return;
    }

    surface_picture_label->show();

    if (surface_format <= Format::MaxTextureFormat) {
        // Generate a virtual texture
        Pica::Texture::TextureInfo info;
        info.physical_address = surface_address;
        info.width = surface_width;
        info.height = surface_height;
        info.format = static_cast<Pica::TexturingRegs::TextureFormat>(surface_format);
        info.SetDefaultStride();

        for (unsigned int y = 0; y < surface_height; ++y) {
            for (unsigned int x = 0; x < surface_width; ++x) {
                Common::Vec4<u8> color = Pica::Texture::LookupTexture(buffer, x, y, info, true);
                decoded_image.setPixel(x, y, qRgba(color.r(), color.g(), color.b(), color.a()));
            }
        }
    } else {
        // We handle depth formats here because DebugUtils only supports TextureFormats

        // TODO(yuriks): Convert to newer tile-based addressing
        unsigned nibbles_per_pixel = GraphicsSurfaceWidget::NibblesPerPixel(surface_format);
        unsigned stride = nibbles_per_pixel * surface_width / 2;

        ASSERT_MSG(nibbles_per_pixel >= 2,
                   "Depth decoder only supports formats with at least one byte per pixel");
        unsigned bytes_per_pixel = nibbles_per_pixel / 2;

        for (unsigned int y = 0; y < surface_height; ++y) {
            for (unsigned int x = 0; x < surface_width; ++x) {
                const u32 coarse_y = y & ~7;
                u32 offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * stride;
                const u8* pixel = buffer + offset;
                Common::Vec4<u8> color = {0, 0, 0, 0};

                switch (surface_format) {
                case Format::D16: {
                    u32 data = Common::Color::DecodeD16(pixel);
                    color.r() = data & 0xFF;
                    color.g() = (data >> 8) & 0xFF;
                    break;
                }
                case Format::D24: {
                    u32 data = Common::Color::DecodeD24(pixel);
                    color.r() = data & 0xFF;
                    color.g() = (data >> 8) & 0xFF;
                    color.b() = (data >> 16) & 0xFF;
                    break;
                }
                case Format::D24X8: {
                    Common::Vec2<u32> data = Common::Color::DecodeD24S8(pixel);
                    color.r() = data.x & 0xFF;
                    color.g() = (data.x >> 8) & 0xFF;
                    color.b() = (data.x >> 16) & 0xFF;
                    break;
                }
                case Format::X24S8: {
                    Common::Vec2<u32> data = Common::Color::DecodeD24S8(pixel);
                    color.r() = color.g() = color.b() = data.y;
                    break;
                }
                default:
                    qDebug() << "Unknown surface format " << static_cast<int>(surface_format);
                    break;
                }

                decoded_image.setPixel(x, y, qRgba(color.r(), color.g(), color.b(), 255));
            }
        }
    }

    pixmap = QPixmap::fromImage(decoded_image);
    surface_picture_label->setPixmap(pixmap);
    surface_picture_label->resize(pixmap.size());

    // Update the info with pixel data
    surface_picker_x_control->setEnabled(true);
    surface_picker_y_control->setEnabled(true);
    Pick(surface_picker_x, surface_picker_y);

    // Enable saving the converted pixmap to file
    save_surface->setEnabled(true);
}

void GraphicsSurfaceWidget::SaveSurface() {
    const QString png_filter = tr("Portable Network Graphic (*.png)");
    const QString bin_filter = tr("Binary data (*.bin)");

    QString selected_filter;
    const QString filename = QFileDialog::getSaveFileName(
        this, tr("Save Surface"),
        QStringLiteral("texture-0x%1.png").arg(QString::number(surface_address, 16)),
        QStringLiteral("%1;;%2").arg(png_filter, bin_filter), &selected_filter);

    if (filename.isEmpty()) {
        // If the user canceled the dialog, don't save anything.
        return;
    }

    if (selected_filter == png_filter) {
        const QPixmap pixmap = surface_picture_label->pixmap(Qt::ReturnByValue);
        ASSERT_MSG(!pixmap.isNull(), "No pixmap set");

        QFile file{filename};
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, tr("Error"), tr("Failed to open file '%1'").arg(filename));
            return;
        }

        if (!pixmap.save(&file, "PNG")) {
            QMessageBox::warning(this, tr("Error"),
                                 tr("Failed to save surface data to file '%1'").arg(filename));
        }
    } else if (selected_filter == bin_filter) {
        const u8* const buffer = system.Memory().GetPhysicalPointer(surface_address);
        ASSERT_MSG(buffer, "Memory not accessible");

        QFile file{filename};
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, tr("Error"), tr("Failed to open file '%1'").arg(filename));
            return;
        }

        const int size = surface_width * surface_height * NibblesPerPixel(surface_format) / 2;
        const QByteArray data(reinterpret_cast<const char*>(buffer), size);
        if (file.write(data) != data.size()) {
            QMessageBox::warning(
                this, tr("Error"),
                tr("Failed to completely write surface data to file. The saved data will "
                   "likely be corrupt."));
        }
    } else {
        UNREACHABLE_MSG("Unhandled filter selected");
    }
}

unsigned int GraphicsSurfaceWidget::NibblesPerPixel(GraphicsSurfaceWidget::Format format) {
    if (format <= Format::MaxTextureFormat) {
        return Pica::TexturingRegs::NibblesPerPixel(
            static_cast<Pica::TexturingRegs::TextureFormat>(format));
    }

    switch (format) {
    case Format::D24X8:
    case Format::X24S8:
        return 4 * 2;
    case Format::D24:
        return 3 * 2;
    case Format::D16:
        return 2 * 2;
    default:
        UNREACHABLE_MSG("GraphicsSurfaceWidget::BytesPerPixel: this should not be reached as this "
                        "function should be given a format which is in "
                        "GraphicsSurfaceWidget::Format. Instead got {}",
                        static_cast<int>(format));
        return 0;
    }
}
