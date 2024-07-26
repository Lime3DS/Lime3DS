// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/assert.h"
#include "video_core/pica/regs_internal.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace PicaToVK {

using TextureFilter = Pica::TexturingRegs::TextureConfig::TextureFilter;

inline vk::Filter TextureFilterMode(TextureFilter mode) {
    switch (mode) {
    case TextureFilter::Linear:
        return vk::Filter::eLinear;
    case TextureFilter::Nearest:
        return vk::Filter::eNearest;
    default:
        UNIMPLEMENTED_MSG("Unknown texture filtering mode {}", mode);
    }

    return vk::Filter::eLinear;
}

inline vk::SamplerMipmapMode TextureMipFilterMode(TextureFilter mip) {
    switch (mip) {
    case TextureFilter::Linear:
        return vk::SamplerMipmapMode::eLinear;
    case TextureFilter::Nearest:
        return vk::SamplerMipmapMode::eNearest;
    default:
        UNIMPLEMENTED_MSG("Unknown texture mipmap filtering mode {}", mip);
    }

    return vk::SamplerMipmapMode::eLinear;
}

inline vk::SamplerAddressMode WrapMode(Pica::TexturingRegs::TextureConfig::WrapMode mode) {
    static constexpr std::array<vk::SamplerAddressMode, 8> wrap_mode_table{{
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToBorder,
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eMirroredRepeat,
        // TODO(wwylele): ClampToEdge2 and ClampToBorder2 are not properly implemented here. See the
        // comments in enum WrapMode.
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToBorder,
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat,
    }};

    const auto index = static_cast<std::size_t>(mode);
    ASSERT_MSG(index < wrap_mode_table.size(), "Unknown texture wrap mode {}", index);
    return wrap_mode_table[index];
}

inline vk::BlendOp BlendEquation(Pica::FramebufferRegs::BlendEquation equation) {
    static constexpr std::array<vk::BlendOp, 5> blend_equation_table{{
        vk::BlendOp::eAdd,
        vk::BlendOp::eSubtract,
        vk::BlendOp::eReverseSubtract,
        vk::BlendOp::eMin,
        vk::BlendOp::eMax,
    }};

    const auto index = static_cast<std::size_t>(equation);
    if (index >= blend_equation_table.size()) {
        LOG_CRITICAL(Render_Vulkan, "Unknown blend equation {}", index);

        // This return value is hwtested, not just a stub
        return vk::BlendOp::eAdd;
    }
    return blend_equation_table[index];
}

inline vk::BlendFactor BlendFunc(Pica::FramebufferRegs::BlendFactor factor) {
    static constexpr std::array<vk::BlendFactor, 15> blend_func_table{{
        vk::BlendFactor::eZero,                  // BlendFactor::Zero
        vk::BlendFactor::eOne,                   // BlendFactor::One
        vk::BlendFactor::eSrcColor,              // BlendFactor::SourceColor
        vk::BlendFactor::eOneMinusSrcColor,      // BlendFactor::OneMinusSourceColor
        vk::BlendFactor::eDstColor,              // BlendFactor::DestColor
        vk::BlendFactor::eOneMinusDstColor,      // BlendFactor::OneMinusDestColor
        vk::BlendFactor::eSrcAlpha,              // BlendFactor::SourceAlpha
        vk::BlendFactor::eOneMinusSrcAlpha,      // BlendFactor::OneMinusSourceAlpha
        vk::BlendFactor::eDstAlpha,              // BlendFactor::DestAlpha
        vk::BlendFactor::eOneMinusDstAlpha,      // BlendFactor::OneMinusDestAlpha
        vk::BlendFactor::eConstantColor,         // BlendFactor::ConstantColor
        vk::BlendFactor::eOneMinusConstantColor, // BlendFactor::OneMinusConstantColor
        vk::BlendFactor::eConstantAlpha,         // BlendFactor::ConstantAlpha
        vk::BlendFactor::eOneMinusConstantAlpha, // BlendFactor::OneMinusConstantAlpha
        vk::BlendFactor::eSrcAlphaSaturate,      // BlendFactor::SourceAlphaSaturate
    }};

    const auto index = static_cast<std::size_t>(factor);
    if (index >= blend_func_table.size()) {
        LOG_CRITICAL(Render_Vulkan, "Unknown blend factor {}", index);
        return vk::BlendFactor::eOne;
    }
    return blend_func_table[index];
}

inline vk::LogicOp LogicOp(Pica::FramebufferRegs::LogicOp op) {
    static constexpr std::array<vk::LogicOp, 16> logic_op_table{{
        vk::LogicOp::eClear,        // Clear
        vk::LogicOp::eAnd,          // And
        vk::LogicOp::eAndReverse,   // AndReverse
        vk::LogicOp::eCopy,         // Copy
        vk::LogicOp::eSet,          // Set
        vk::LogicOp::eCopyInverted, // CopyInverted
        vk::LogicOp::eNoOp,         // NoOp
        vk::LogicOp::eInvert,       // Invert
        vk::LogicOp::eNand,         // Nand
        vk::LogicOp::eOr,           // Or
        vk::LogicOp::eNor,          // Nor
        vk::LogicOp::eXor,          // Xor
        vk::LogicOp::eEquivalent,   // Equiv
        vk::LogicOp::eAndInverted,  // AndInverted
        vk::LogicOp::eOrReverse,    // OrReverse
        vk::LogicOp::eOrInverted,   // OrInverted
    }};

    const auto index = static_cast<std::size_t>(op);
    ASSERT_MSG(index < logic_op_table.size(), "Unknown logic op {}", index);
    return logic_op_table[index];
}

inline vk::CompareOp CompareFunc(Pica::FramebufferRegs::CompareFunc func) {
    static constexpr std::array<vk::CompareOp, 8> compare_func_table{{
        vk::CompareOp::eNever,          // CompareFunc::Never
        vk::CompareOp::eAlways,         // CompareFunc::Always
        vk::CompareOp::eEqual,          // CompareFunc::Equal
        vk::CompareOp::eNotEqual,       // CompareFunc::NotEqual
        vk::CompareOp::eLess,           // CompareFunc::LessThan
        vk::CompareOp::eLessOrEqual,    // CompareFunc::LessThanOrEqual
        vk::CompareOp::eGreater,        // CompareFunc::GreaterThan
        vk::CompareOp::eGreaterOrEqual, // CompareFunc::GreaterThanOrEqual
    }};

    const auto index = static_cast<std::size_t>(func);
    ASSERT_MSG(index < compare_func_table.size(), "Unknown compare function {}", index);
    return compare_func_table[index];
}

inline vk::StencilOp StencilOp(Pica::FramebufferRegs::StencilAction action) {
    static constexpr std::array<vk::StencilOp, 8> stencil_op_table{{
        vk::StencilOp::eKeep,              // StencilAction::Keep
        vk::StencilOp::eZero,              // StencilAction::Zero
        vk::StencilOp::eReplace,           // StencilAction::Replace
        vk::StencilOp::eIncrementAndClamp, // StencilAction::Increment
        vk::StencilOp::eDecrementAndClamp, // StencilAction::Decrement
        vk::StencilOp::eInvert,            // StencilAction::Invert
        vk::StencilOp::eIncrementAndWrap,  // StencilAction::IncrementWrap
        vk::StencilOp::eDecrementAndWrap,  // StencilAction::DecrementWrap
    }};

    const auto index = static_cast<std::size_t>(action);
    ASSERT_MSG(index < stencil_op_table.size(), "Unknown stencil op {}", index);
    return stencil_op_table[index];
}

inline vk::PrimitiveTopology PrimitiveTopology(Pica::PipelineRegs::TriangleTopology topology) {
    switch (topology) {
    case Pica::PipelineRegs::TriangleTopology::Fan:
        return vk::PrimitiveTopology::eTriangleFan;
    case Pica::PipelineRegs::TriangleTopology::List:
    case Pica::PipelineRegs::TriangleTopology::Shader:
        return vk::PrimitiveTopology::eTriangleList;
    case Pica::PipelineRegs::TriangleTopology::Strip:
        return vk::PrimitiveTopology::eTriangleStrip;
    default:
        UNREACHABLE_MSG("Unknown triangle topology {}", topology);
    }
    return vk::PrimitiveTopology::eTriangleList;
}

inline vk::CullModeFlags CullMode(Pica::RasterizerRegs::CullMode mode) {
    switch (mode) {
    case Pica::RasterizerRegs::CullMode::KeepAll:
        return vk::CullModeFlagBits::eNone;
    case Pica::RasterizerRegs::CullMode::KeepClockWise:
    case Pica::RasterizerRegs::CullMode::KeepCounterClockWise:
        return vk::CullModeFlagBits::eBack;
    default:
        UNREACHABLE_MSG("Unknown cull mode {}", mode);
    }
    return vk::CullModeFlagBits::eNone;
}

inline vk::FrontFace FrontFace(Pica::RasterizerRegs::CullMode mode) {
    switch (mode) {
    case Pica::RasterizerRegs::CullMode::KeepAll:
    case Pica::RasterizerRegs::CullMode::KeepClockWise:
        return vk::FrontFace::eCounterClockwise;
    case Pica::RasterizerRegs::CullMode::KeepCounterClockWise:
        return vk::FrontFace::eClockwise;
    default:
        UNREACHABLE_MSG("Unknown cull mode {}", mode);
    }
    return vk::FrontFace::eClockwise;
}

inline Common::Vec4f ColorRGBA8(const u32 color) {
    const auto rgba =
        Common::Vec4u{color >> 0 & 0xFF, color >> 8 & 0xFF, color >> 16 & 0xFF, color >> 24 & 0xFF};
    return rgba / 255.0f;
}

} // namespace PicaToVK
