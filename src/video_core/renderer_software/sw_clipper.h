// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/pica_types.h"

namespace Pica {
struct TexturingRegs;
}

namespace SwRenderer {

using Pica::f24;

// NOTE: Assuming that rasterizer coordinates are 12.4 fixed-point values
struct Fix12P4 {
    Fix12P4() {}
    Fix12P4(u16 val) : val(val) {}

    static Fix12P4 FromFloat24(f24 flt) {
        // TODO: Rounding here is necessary to prevent garbage pixels at
        //       triangle borders. Is it that the correct solution, though?
        return Fix12P4(static_cast<u16>(round(flt.ToFloat32() * 16.0f)));
    }

    static u16 FracMask() {
        return 0xF;
    }
    static u16 IntMask() {
        return static_cast<u16>(~0xF);
    }

    operator u16() const {
        return val;
    }

    bool operator<(const Fix12P4& oth) const {
        return (u16) * this < (u16)oth;
    }

private:
    u16 val;
};

struct Viewport {
    f24 halfsize_x;
    f24 offset_x;
    f24 halfsize_y;
    f24 offset_y;
    f24 zscale;
    f24 offset_z;
};

/**
 * Flips the quaternions if they are opposite to prevent
 * interpolating them over the wrong direction.
 */
void FlipQuaternionIfOpposite(Common::Vec4<f24>& a, const Common::Vec4<f24>& b);

/**
 * Calculate signed area of the triangle spanned by the three argument vertices.
 * The sign denotes an orientation.
 **/
int SignedArea(const Common::Vec2<Fix12P4>& vtx1, const Common::Vec2<Fix12P4>& vtx2,
               const Common::Vec2<Fix12P4>& vtx3);

/**
 * Convert a 3D vector for cube map coordinates to 2D texture coordinates along with the face name.
 **/
std::tuple<f24, f24, f24, PAddr> ConvertCubeCoord(f24 u, f24 v, f24 w,
                                                  const Pica::TexturingRegs& regs);

/**
 * Triangle filling rules: Pixels on the right-sided edge or on flat bottom edges are not
 * drawn. Pixels on any other triangle border are drawn. This is implemented with three bias
 * values which are added to the barycentric coordinates w0, w1 and w2, respectively.
 * NOTE: These are the PSP filling rules. Not sure if the 3DS uses the same ones...
 **/
bool IsRightSideOrFlatBottomEdge(const Common::Vec2<Fix12P4>& vtx,
                                 const Common::Vec2<Fix12P4>& line1,
                                 const Common::Vec2<Fix12P4>& line2);

} // namespace SwRenderer
