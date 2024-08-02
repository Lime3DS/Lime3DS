// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstddef>
#include <tuple>
#include "video_core/pica/regs_texturing.h"
#include "video_core/renderer_software/sw_clipper.h"

namespace SwRenderer {

using Pica::TexturingRegs;

void FlipQuaternionIfOpposite(Common::Vec4<f24>& a, const Common::Vec4<f24>& b) {
    if (Common::Dot(a, b) < f24::Zero()) {
        a *= f24::FromFloat32(-1.0f);
    }
};

int SignedArea(const Common::Vec2<Fix12P4>& vtx1, const Common::Vec2<Fix12P4>& vtx2,
               const Common::Vec2<Fix12P4>& vtx3) {
    const auto vec1 = Common::MakeVec(vtx2 - vtx1, 0);
    const auto vec2 = Common::MakeVec(vtx3 - vtx1, 0);
    // TODO: There is a very small chance this will overflow for sizeof(int) == 4
    return Common::Cross(vec1, vec2).z;
};

std::tuple<f24, f24, f24, PAddr> ConvertCubeCoord(f24 u, f24 v, f24 w,
                                                  const Pica::TexturingRegs& regs) {
    const float abs_u = std::abs(u.ToFloat32());
    const float abs_v = std::abs(v.ToFloat32());
    const float abs_w = std::abs(w.ToFloat32());
    f24 x, y, z;
    PAddr addr;
    if (abs_u > abs_v && abs_u > abs_w) {
        if (u > f24::Zero()) {
            addr = regs.GetCubePhysicalAddress(TexturingRegs::CubeFace::PositiveX);
            y = -v;
        } else {
            addr = regs.GetCubePhysicalAddress(TexturingRegs::CubeFace::NegativeX);
            y = v;
        }
        x = -w;
        z = u;
    } else if (abs_v > abs_w) {
        if (v > f24::Zero()) {
            addr = regs.GetCubePhysicalAddress(TexturingRegs::CubeFace::PositiveY);
            x = u;
        } else {
            addr = regs.GetCubePhysicalAddress(TexturingRegs::CubeFace::NegativeY);
            x = -u;
        }
        y = w;
        z = v;
    } else {
        if (w > f24::Zero()) {
            addr = regs.GetCubePhysicalAddress(TexturingRegs::CubeFace::PositiveZ);
            y = -v;
        } else {
            addr = regs.GetCubePhysicalAddress(TexturingRegs::CubeFace::NegativeZ);
            y = v;
        }
        x = u;
        z = w;
    }
    const f24 z_abs = f24::FromFloat32(std::abs(z.ToFloat32()));
    const f24 half = f24::FromFloat32(0.5f);
    return std::make_tuple(x / z * half + half, y / z * half + half, z_abs, addr);
}

bool IsRightSideOrFlatBottomEdge(const Common::Vec2<Fix12P4>& vtx,
                                 const Common::Vec2<Fix12P4>& line1,
                                 const Common::Vec2<Fix12P4>& line2) {
    if (line1.y == line2.y) {
        // Just check if vertex is above us => bottom line parallel to x-axis
        return vtx.y < line1.y;
    } else {
        // Check if vertex is on our left => right side
        // TODO: Not sure how likely this is to overflow
        const auto svtx = vtx.Cast<s32>();
        const auto sline1 = line1.Cast<s32>();
        const auto sline2 = line2.Cast<s32>();
        return svtx.x <
               sline1.x + (sline2.x - sline1.x) * (svtx.y - sline1.y) / (sline2.y - sline1.y);
    }
}

} // namespace SwRenderer
