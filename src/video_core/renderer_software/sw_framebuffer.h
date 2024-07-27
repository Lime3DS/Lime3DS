// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/pica/regs_framebuffer.h"

namespace Memory {
class MemorySystem;
}

namespace Pica {
struct FramebufferRegs;
}

namespace SwRenderer {

class Framebuffer {
public:
    explicit Framebuffer(Memory::MemorySystem& memory, const Pica::FramebufferRegs& framebuffer);
    ~Framebuffer();

    /// Updates the framebuffer addresses from the PICA registers.
    void Bind();

    /// Draws a pixel at the specified coordinates.
    void DrawPixel(u32 x, u32 y, const Common::Vec4<u8>& color) const;

    /// Returns the current color at the specified coordinates.
    [[nodiscard]] const Common::Vec4<u8> GetPixel(u32 x, u32 y) const;

    /// Returns the depth value at the specified coordinates.
    [[nodiscard]] u32 GetDepth(u32 x, u32 y) const;

    /// Returns the stencil value at the specified coordinates.
    [[nodiscard]] u8 GetStencil(u32 x, u32 y) const;

    /// Stores the provided depth value at the specified coordinates.
    void SetDepth(u32 x, u32 y, u32 value) const;

    /// Stores the provided stencil value at the specified coordinates.
    void SetStencil(u32 x, u32 y, u8 value) const;

    /// Draws a pixel to the shadow buffer.
    void DrawShadowMapPixel(u32 x, u32 y, u32 depth, u8 stencil) const;

private:
    Memory::MemorySystem& memory;
    const Pica::FramebufferRegs& regs;
    PAddr color_addr;
    u8* color_buffer{};
    PAddr depth_addr;
    u8* depth_buffer{};
};

u8 PerformStencilAction(Pica::FramebufferRegs::StencilAction action, u8 old_stencil, u8 ref);

Common::Vec4<u8> EvaluateBlendEquation(const Common::Vec4<u8>& src,
                                       const Common::Vec4<u8>& srcfactor,
                                       const Common::Vec4<u8>& dest,
                                       const Common::Vec4<u8>& destfactor,
                                       Pica::FramebufferRegs::BlendEquation equation);

u8 LogicOp(u8 src, u8 dest, Pica::FramebufferRegs::LogicOp op);

} // namespace SwRenderer
