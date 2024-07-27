// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QListView>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeView>
#include <QVBoxLayout>
#include "common/vector_math.h"
#include "core/core.h"
#include "core/memory.h"
#include "lime_qt/debugger/graphics/graphics_cmdlists.h"
#include "lime_qt/util/util.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/gpu.h"
#include "video_core/pica/pica_core.h"
#include "video_core/texture/texture_decode.h"

namespace {
QImage LoadTexture(const u8* src, const Pica::Texture::TextureInfo& info) {
    QImage decoded_image(info.width, info.height, QImage::Format_ARGB32);
    for (u32 y = 0; y < info.height; ++y) {
        for (u32 x = 0; x < info.width; ++x) {
            Common::Vec4<u8> color = Pica::Texture::LookupTexture(src, x, y, info, true);
            decoded_image.setPixel(x, y, qRgba(color.r(), color.g(), color.b(), color.a()));
        }
    }

    return decoded_image;
}

class TextureInfoWidget : public QWidget {
public:
    TextureInfoWidget(const u8* src, const Pica::Texture::TextureInfo& info,
                      QWidget* parent = nullptr)
        : QWidget(parent) {

        QLabel* image_widget = new QLabel;
        QPixmap image_pixmap = QPixmap::fromImage(LoadTexture(src, info));
        image_pixmap = image_pixmap.scaled(200, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        image_widget->setPixmap(image_pixmap);

        QVBoxLayout* layout = new QVBoxLayout;
        layout->addWidget(image_widget);
        setLayout(layout);
    }
};
} // Anonymous namespace

GPUCommandListModel::GPUCommandListModel(QObject* parent) : QAbstractListModel(parent) {}

int GPUCommandListModel::rowCount([[maybe_unused]] const QModelIndex& parent) const {
    return static_cast<int>(pica_trace.writes.size());
}

int GPUCommandListModel::columnCount([[maybe_unused]] const QModelIndex& parent) const {
    return 4;
}

QVariant GPUCommandListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return QVariant();

    const auto& write = pica_trace.writes[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:
            return QString::fromLatin1(Pica::RegsInternal::GetRegisterName(write.cmd_id));
        case 1:
            return QStringLiteral("%1").arg(write.cmd_id, 3, 16, QLatin1Char('0'));
        case 2:
            return QStringLiteral("%1").arg(write.mask, 4, 2, QLatin1Char('0'));
        case 3:
            return QStringLiteral("%1").arg(write.value, 8, 16, QLatin1Char('0'));
        }
    } else if (role == CommandIdRole) {
        return QVariant::fromValue<int>(write.cmd_id);
    }

    return QVariant();
}

QVariant GPUCommandListModel::headerData(int section, [[maybe_unused]] Qt::Orientation orientation,
                                         int role) const {
    switch (role) {
    case Qt::DisplayRole: {
        switch (section) {
        case 0:
            return tr("Command Name");
        case 1:
            return tr("Register");
        case 2:
            return tr("Mask");
        case 3:
            return tr("New Value");
        }

        break;
    }
    }

    return QVariant();
}

void GPUCommandListModel::OnPicaTraceFinished(const Pica::DebugUtils::PicaTrace& trace) {
    beginResetModel();

    pica_trace = trace;

    endResetModel();
}

#define COMMAND_IN_RANGE(cmd_id, reg_name)                                                         \
    (cmd_id >= PICA_REG_INDEX(reg_name) && cmd_id <= PICA_REG_INDEX(reg_name))

void GPUCommandListWidget::OnCommandDoubleClicked(const QModelIndex& index) {
    const unsigned int command_id =
        list_widget->model()->data(index, GPUCommandListModel::CommandIdRole).toUInt();
    if (COMMAND_IN_RANGE(command_id, texturing.texture0) ||
        COMMAND_IN_RANGE(command_id, texturing.texture1) ||
        COMMAND_IN_RANGE(command_id, texturing.texture2)) {

        [[maybe_unused]] u32 texture_index;
        if (COMMAND_IN_RANGE(command_id, texturing.texture0)) {
            texture_index = 0;
        } else if (COMMAND_IN_RANGE(command_id, texturing.texture1)) {
            texture_index = 1;
        } else if (COMMAND_IN_RANGE(command_id, texturing.texture2)) {
            texture_index = 2;
        } else {
            UNREACHABLE_MSG("Unknown texture command");
        }

        // TODO: Open a surface debugger
    }
}

void GPUCommandListWidget::SetCommandInfo(const QModelIndex& index) {
    QWidget* new_info_widget = nullptr;

    const u32 command_id =
        list_widget->model()->data(index, GPUCommandListModel::CommandIdRole).toUInt();
    if (COMMAND_IN_RANGE(command_id, texturing.texture0) ||
        COMMAND_IN_RANGE(command_id, texturing.texture1) ||
        COMMAND_IN_RANGE(command_id, texturing.texture2)) {

        u32 texture_index;
        if (COMMAND_IN_RANGE(command_id, texturing.texture0)) {
            texture_index = 0;
        } else if (COMMAND_IN_RANGE(command_id, texturing.texture1)) {
            texture_index = 1;
        } else {
            texture_index = 2;
        }

        auto& pica = system.GPU().PicaCore();
        const auto texture = pica.regs.internal.texturing.GetTextures()[texture_index];
        const auto config = texture.config;
        const auto format = texture.format;

        const auto info = Pica::Texture::TextureInfo::FromPicaRegister(config, format);
        const u8* src = system.Memory().GetPhysicalPointer(config.GetPhysicalAddress());
        new_info_widget = new TextureInfoWidget(src, info);
    }
    if (command_info_widget) {
        delete command_info_widget;
        command_info_widget = nullptr;
    }
    if (new_info_widget) {
        widget()->layout()->addWidget(new_info_widget);
        command_info_widget = new_info_widget;
    }
}
#undef COMMAND_IN_RANGE

GPUCommandListWidget::GPUCommandListWidget(Core::System& system_, QWidget* parent)
    : QDockWidget(tr("Pica Command List"), parent), system{system_} {
    setObjectName(QStringLiteral("Pica Command List"));
    GPUCommandListModel* model = new GPUCommandListModel(this);

    QWidget* main_widget = new QWidget;

    list_widget = new QTreeView;
    list_widget->setModel(model);
    list_widget->setFont(GetMonospaceFont());
    list_widget->setRootIsDecorated(false);
    list_widget->setUniformRowHeights(true);
    list_widget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(list_widget->selectionModel(), &QItemSelectionModel::currentChanged, this,
            &GPUCommandListWidget::SetCommandInfo);
    connect(list_widget, &QTreeView::doubleClicked, this,
            &GPUCommandListWidget::OnCommandDoubleClicked);

    toggle_tracing = new QPushButton(tr("Start Tracing"));
    QPushButton* copy_all = new QPushButton(tr("Copy All"));

    connect(toggle_tracing, &QPushButton::clicked, this, &GPUCommandListWidget::OnToggleTracing);
    connect(this, &GPUCommandListWidget::TracingFinished, model,
            &GPUCommandListModel::OnPicaTraceFinished);

    connect(copy_all, &QPushButton::clicked, this, &GPUCommandListWidget::CopyAllToClipboard);

    command_info_widget = nullptr;

    QVBoxLayout* main_layout = new QVBoxLayout;
    main_layout->addWidget(list_widget);
    {
        QHBoxLayout* sub_layout = new QHBoxLayout;
        sub_layout->addWidget(toggle_tracing);
        sub_layout->addWidget(copy_all);
        main_layout->addLayout(sub_layout);
    }
    main_widget->setLayout(main_layout);

    setWidget(main_widget);
}

void GPUCommandListWidget::OnToggleTracing() {
    if (!Pica::DebugUtils::IsPicaTracing()) {
        Pica::DebugUtils::StartPicaTracing();
        toggle_tracing->setText(tr("Finish Tracing"));
    } else {
        pica_trace = Pica::DebugUtils::FinishPicaTracing();
        emit TracingFinished(*pica_trace);
        toggle_tracing->setText(tr("Start Tracing"));
    }
}

void GPUCommandListWidget::CopyAllToClipboard() {
    QClipboard* clipboard = QApplication::clipboard();
    QString text;

    QAbstractItemModel* model = static_cast<QAbstractItemModel*>(list_widget->model());

    for (int row = 0; row < model->rowCount({}); ++row) {
        for (int col = 0; col < model->columnCount({}); ++col) {
            QModelIndex index = model->index(row, col);
            text += model->data(index).value<QString>();
            text += QLatin1Char('\t');
        }
        text += QLatin1Char('\n');
    }

    clipboard->setText(text);
}
