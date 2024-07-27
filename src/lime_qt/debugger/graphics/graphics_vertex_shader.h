// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QAbstractTableModel>
#include <QTreeView>
#include <nihstro/parser_shbin.h>
#include "lime_qt/debugger/graphics/graphics_breakpoint_observer.h"
#include "video_core/pica/output_vertex.h"
#include "video_core/shader/debug_data.h"

namespace Core {
class System;
}

class QLabel;
class QSpinBox;

class GraphicsVertexShaderWidget;

class GraphicsVertexShaderModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit GraphicsVertexShaderModel(GraphicsVertexShaderWidget* parent);

    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    GraphicsVertexShaderWidget* par;

    friend class GraphicsVertexShaderWidget;
};

class GraphicsVertexShaderWidget : public BreakPointObserverDock {
    Q_OBJECT

    using Event = Pica::DebugContext::Event;

public:
    GraphicsVertexShaderWidget(Core::System& system,
                               std::shared_ptr<Pica::DebugContext> debug_context,
                               QWidget* parent = nullptr);

private slots:
    void OnBreakPointHit(Pica::DebugContext::Event event, const void* data) override;
    void OnResumed() override;

    void OnInputAttributeChanged(int index);

    void OnCycleIndexChanged(int index);

    void DumpShader();

    /**
     * Reload widget based on the current PICA200 state
     * @param replace_vertex_data If true, invalidate all current vertex data
     * @param vertex_data New vertex data to use, as passed to OnBreakPointHit. May be nullptr to
     * specify that no valid vertex data can be retrieved currently. Only used if
     * replace_vertex_data is true.
     */
    void Reload(bool replace_vertex_data = false, const void* vertex_data = nullptr);

private:
    Core::System& system;
    QLabel* instruction_description;
    QTreeView* binary_list;
    GraphicsVertexShaderModel* model;

    // TODO: Move these into a single struct

    // A text box for each of the 4 components of up to 16 vertex attributes
    std::array<QLineEdit*, 4 * 16> input_data;
    // QWidget containing the QLayout containing each vertex attribute
    std::array<QWidget*, 16> input_data_container;
    // A QLabel denoting the shader input attribute which the vertex attribute maps to
    std::array<QLabel*, 16> input_data_mapping;

    // Text to be shown when input vertex data is not retrievable
    QLabel* breakpoint_warning;

    QSpinBox* cycle_index;

    nihstro::ShaderInfo info;
    Pica::Shader::DebugData<true> debug_data;
    Pica::AttributeBuffer input_vertex;

    friend class GraphicsVertexShaderModel;
};
