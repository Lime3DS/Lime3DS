// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <glad/glad.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "video_core/pica/regs_framebuffer.h"
#include "video_core/pica/regs_lighting.h"
#include "video_core/pica/regs_texturing.h"

namespace PicaToGL {

using TextureFilter = Pica::TexturingRegs::TextureConfig::TextureFilter;

inline GLenum TextureMagFilterMode(TextureFilter mode) {
    if (mode == TextureFilter::Linear) {
        return GL_LINEAR;
    }
    if (mode == TextureFilter::Nearest) {
        return GL_NEAREST;
    }
    LOG_CRITICAL(Render_OpenGL, "Unknown texture filtering mode {}", mode);
    UNIMPLEMENTED();
    return GL_LINEAR;
}

inline GLenum TextureMinFilterMode(TextureFilter min, TextureFilter mip) {
    if (min == TextureFilter::Linear) {
        if (mip == TextureFilter::Linear) {
            return GL_LINEAR_MIPMAP_LINEAR;
        }
        if (mip == TextureFilter::Nearest) {
            return GL_LINEAR_MIPMAP_NEAREST;
        }
    } else if (min == TextureFilter::Nearest) {
        if (mip == TextureFilter::Linear) {
            return GL_NEAREST_MIPMAP_LINEAR;
        }
        if (mip == TextureFilter::Nearest) {
            return GL_NEAREST_MIPMAP_NEAREST;
        }
    }
    LOG_CRITICAL(Render_OpenGL, "Unknown texture filtering mode {} and {}", min, mip);
    UNIMPLEMENTED();
    return GL_LINEAR_MIPMAP_LINEAR;
}

inline GLenum WrapMode(Pica::TexturingRegs::TextureConfig::WrapMode mode) {
    static constexpr std::array<GLenum, 8> wrap_mode_table{{
        GL_CLAMP_TO_EDGE,   // WrapMode::ClampToEdge
        GL_CLAMP_TO_BORDER, // WrapMode::ClampToBorder
        GL_REPEAT,          // WrapMode::Repeat
        GL_MIRRORED_REPEAT, // WrapMode::MirroredRepeat
        // TODO(wwylele): ClampToEdge2 and ClampToBorder2 are not properly implemented here. See the
        // comments in enum WrapMode.
        GL_CLAMP_TO_EDGE,   // WrapMode::ClampToEdge2
        GL_CLAMP_TO_BORDER, // WrapMode::ClampToBorder2
        GL_REPEAT,          // WrapMode::Repeat2
        GL_REPEAT,          // WrapMode::Repeat3
    }};

    const auto index = static_cast<std::size_t>(mode);

    // Range check table for input
    if (index >= wrap_mode_table.size()) {
        LOG_CRITICAL(Render_OpenGL, "Unknown texture wrap mode {}", index);
        UNREACHABLE();

        return GL_CLAMP_TO_EDGE;
    }

    if (index > 3) {
        LOG_WARNING(Render_OpenGL, "Using texture wrap mode {}", index);
    }

    GLenum gl_mode = wrap_mode_table[index];

    // Check for dummy values indicating an unknown mode
    if (gl_mode == 0) {
        LOG_CRITICAL(Render_OpenGL, "Unknown texture wrap mode {}", index);
        UNIMPLEMENTED();

        return GL_CLAMP_TO_EDGE;
    }

    return gl_mode;
}

inline GLenum BlendEquation(Pica::FramebufferRegs::BlendEquation equation, bool factor_minmax) {
    static constexpr std::array<GLenum, 5> blend_equation_table{{
        GL_FUNC_ADD,              // BlendEquation::Add
        GL_FUNC_SUBTRACT,         // BlendEquation::Subtract
        GL_FUNC_REVERSE_SUBTRACT, // BlendEquation::ReverseSubtract
        GL_MIN,                   // BlendEquation::Min
        GL_MAX,                   // BlendEquation::Max
    }};
    static constexpr std::array<GLenum, 5> blend_equation_table_minmax{{
        GL_FUNC_ADD,              // BlendEquation::Add
        GL_FUNC_SUBTRACT,         // BlendEquation::Subtract
        GL_FUNC_REVERSE_SUBTRACT, // BlendEquation::ReverseSubtract
        GL_FACTOR_MIN_AMD,        // BlendEquation::Min
        GL_FACTOR_MAX_AMD,        // BlendEquation::Max
    }};

    const auto index = static_cast<std::size_t>(equation);

    // Range check table for input
    if (index >= blend_equation_table.size()) {
        LOG_CRITICAL(Render_OpenGL, "Unknown blend equation {}", index);

        // This return value is hwtested, not just a stub
        return GL_FUNC_ADD;
    }

    return (factor_minmax ? blend_equation_table_minmax : blend_equation_table)[index];
}

inline GLenum BlendFunc(Pica::FramebufferRegs::BlendFactor factor) {
    static constexpr std::array<GLenum, 15> blend_func_table{{
        GL_ZERO,                     // BlendFactor::Zero
        GL_ONE,                      // BlendFactor::One
        GL_SRC_COLOR,                // BlendFactor::SourceColor
        GL_ONE_MINUS_SRC_COLOR,      // BlendFactor::OneMinusSourceColor
        GL_DST_COLOR,                // BlendFactor::DestColor
        GL_ONE_MINUS_DST_COLOR,      // BlendFactor::OneMinusDestColor
        GL_SRC_ALPHA,                // BlendFactor::SourceAlpha
        GL_ONE_MINUS_SRC_ALPHA,      // BlendFactor::OneMinusSourceAlpha
        GL_DST_ALPHA,                // BlendFactor::DestAlpha
        GL_ONE_MINUS_DST_ALPHA,      // BlendFactor::OneMinusDestAlpha
        GL_CONSTANT_COLOR,           // BlendFactor::ConstantColor
        GL_ONE_MINUS_CONSTANT_COLOR, // BlendFactor::OneMinusConstantColor
        GL_CONSTANT_ALPHA,           // BlendFactor::ConstantAlpha
        GL_ONE_MINUS_CONSTANT_ALPHA, // BlendFactor::OneMinusConstantAlpha
        GL_SRC_ALPHA_SATURATE,       // BlendFactor::SourceAlphaSaturate
    }};

    const auto index = static_cast<std::size_t>(factor);

    // Range check table for input
    if (index >= blend_func_table.size()) {
        LOG_CRITICAL(Render_OpenGL, "Unknown blend factor {}", index);
        return GL_ONE;
    }

    return blend_func_table[index];
}

inline GLenum LogicOp(Pica::FramebufferRegs::LogicOp op) {
    static constexpr std::array<GLenum, 16> logic_op_table{{
        GL_CLEAR,         // Clear
        GL_AND,           // And
        GL_AND_REVERSE,   // AndReverse
        GL_COPY,          // Copy
        GL_SET,           // Set
        GL_COPY_INVERTED, // CopyInverted
        GL_NOOP,          // NoOp
        GL_INVERT,        // Invert
        GL_NAND,          // Nand
        GL_OR,            // Or
        GL_NOR,           // Nor
        GL_XOR,           // Xor
        GL_EQUIV,         // Equiv
        GL_AND_INVERTED,  // AndInverted
        GL_OR_REVERSE,    // OrReverse
        GL_OR_INVERTED,   // OrInverted
    }};

    const auto index = static_cast<std::size_t>(op);

    // Range check table for input
    if (index >= logic_op_table.size()) {
        LOG_CRITICAL(Render_OpenGL, "Unknown logic op {}", index);
        UNREACHABLE();

        return GL_COPY;
    }

    return logic_op_table[index];
}

inline GLenum CompareFunc(Pica::FramebufferRegs::CompareFunc func) {
    static constexpr std::array<GLenum, 8> compare_func_table{{
        GL_NEVER,    // CompareFunc::Never
        GL_ALWAYS,   // CompareFunc::Always
        GL_EQUAL,    // CompareFunc::Equal
        GL_NOTEQUAL, // CompareFunc::NotEqual
        GL_LESS,     // CompareFunc::LessThan
        GL_LEQUAL,   // CompareFunc::LessThanOrEqual
        GL_GREATER,  // CompareFunc::GreaterThan
        GL_GEQUAL,   // CompareFunc::GreaterThanOrEqual
    }};

    const auto index = static_cast<std::size_t>(func);

    // Range check table for input
    if (index >= compare_func_table.size()) {
        LOG_CRITICAL(Render_OpenGL, "Unknown compare function {}", index);
        UNREACHABLE();

        return GL_ALWAYS;
    }

    return compare_func_table[index];
}

inline GLenum StencilOp(Pica::FramebufferRegs::StencilAction action) {
    static constexpr std::array<GLenum, 8> stencil_op_table{{
        GL_KEEP,      // StencilAction::Keep
        GL_ZERO,      // StencilAction::Zero
        GL_REPLACE,   // StencilAction::Replace
        GL_INCR,      // StencilAction::Increment
        GL_DECR,      // StencilAction::Decrement
        GL_INVERT,    // StencilAction::Invert
        GL_INCR_WRAP, // StencilAction::IncrementWrap
        GL_DECR_WRAP, // StencilAction::DecrementWrap
    }};

    const auto index = static_cast<std::size_t>(action);

    // Range check table for input
    if (index >= stencil_op_table.size()) {
        LOG_CRITICAL(Render_OpenGL, "Unknown stencil op {}", index);
        UNREACHABLE();

        return GL_KEEP;
    }

    return stencil_op_table[index];
}

inline Common::Vec4f ColorRGBA8(const u32 color) {
    const auto rgba =
        Common::Vec4u{color >> 0 & 0xFF, color >> 8 & 0xFF, color >> 16 & 0xFF, color >> 24 & 0xFF};
    return rgba / 255.0f;
}

inline Common::Vec3f LightColor(const Pica::LightingRegs::LightColor& color) {
    return Common::Vec3u{color.r, color.g, color.b} / 255.0f;
}

} // namespace PicaToGL
