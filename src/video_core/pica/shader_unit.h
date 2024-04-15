// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <span>
#include <boost/serialization/base_object.hpp>

#include "video_core/pica/output_vertex.h"

namespace Pica {

/// Handler type for receiving vertex outputs from vertex shader or geometry shader
using VertexHandler = std::function<void(const AttributeBuffer&)>;

/// Handler type for signaling to invert the vertex order of the next triangle
using WindingSetter = std::function<void()>;

struct ShaderRegs;
struct GeometryEmitter;

/**
 * This structure contains the state information that needs to be unique for a shader unit. The 3DS
 * has four shader units that process shaders in parallel.
 */
struct ShaderUnit {
    explicit ShaderUnit(GeometryEmitter* emitter = nullptr);
    ~ShaderUnit();

    void LoadInput(const ShaderRegs& config, const AttributeBuffer& input);

    void WriteOutput(const ShaderRegs& config, AttributeBuffer& output);

    static constexpr std::size_t InputOffset(s32 register_index) {
        return offsetof(ShaderUnit, input) + register_index * sizeof(Common::Vec4<f24>);
    }

    static constexpr std::size_t OutputOffset(s32 register_index) {
        return offsetof(ShaderUnit, output) + register_index * sizeof(Common::Vec4<f24>);
    }

    static constexpr std::size_t TemporaryOffset(s32 register_index) {
        return offsetof(ShaderUnit, temporary) + register_index * sizeof(Common::Vec4<f24>);
    }

public:
    s32 address_registers[3];
    bool conditional_code[2];
    alignas(16) std::array<Common::Vec4<f24>, 16> input;
    alignas(16) std::array<Common::Vec4<f24>, 16> temporary;
    alignas(16) std::array<Common::Vec4<f24>, 16> output;
    GeometryEmitter* emitter_ptr;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const u32 file_version) {
        ar & input;
        ar & temporary;
        ar & output;
        ar & conditional_code;
        ar & address_registers;
    }
};

struct Handlers {
    VertexHandler vertex_handler;
    WindingSetter winding_setter;
};

/// This structure contains state information for primitive emitting in geometry shader.
struct GeometryEmitter {
    void Emit(std::span<Common::Vec4<f24>, 16> output_regs);

public:
    std::array<AttributeBuffer, 3> buffer;
    u8 vertex_id;
    bool prim_emit;
    bool winding;
    u32 output_mask;
    Handlers* handlers;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const u32 file_version) {
        ar & buffer;
        ar & vertex_id;
        ar & prim_emit;
        ar & winding;
        ar & output_mask;
    }
};

/**
 * This is an extended shader unit state that represents the special unit that can run both vertex
 * shader and geometry shader. It contains an additional primitive emitter and utilities for
 * geometry shader.
 */
struct GeometryShaderUnit : public ShaderUnit {
    GeometryShaderUnit();
    ~GeometryShaderUnit();

    void SetVertexHandlers(VertexHandler vertex_handler, WindingSetter winding_setter);
    void ConfigOutput(const ShaderRegs& config);

    GeometryEmitter emitter;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const u32 file_version) {
        ar& boost::serialization::base_object<ShaderUnit>(*this);
        ar & emitter;
    }
};

} // namespace Pica
