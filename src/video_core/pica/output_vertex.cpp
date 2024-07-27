// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/pica/output_vertex.h"
#include "video_core/pica/regs_rasterizer.h"

namespace Pica {

OutputVertex::OutputVertex(const RasterizerRegs& regs, const AttributeBuffer& output) {
    // Attributes can be used without being set in GPUREG_SH_OUTMAP_Oi
    // Hardware tests have shown that they are initialized to 1 in this case.
    std::array<f24, 32> vertex_slots_overflow;
    vertex_slots_overflow.fill(f24::One());

    const u32 num_attributes = regs.vs_output_total & 7;
    for (std::size_t attrib = 0; attrib < num_attributes; ++attrib) {
        const auto output_register_map = regs.vs_output_attributes[attrib];
        vertex_slots_overflow[output_register_map.map_x] = output[attrib][0];
        vertex_slots_overflow[output_register_map.map_y] = output[attrib][1];
        vertex_slots_overflow[output_register_map.map_z] = output[attrib][2];
        vertex_slots_overflow[output_register_map.map_w] = output[attrib][3];
    }

    // Copy to result
    std::memcpy(this, vertex_slots_overflow.data(), sizeof(OutputVertex));

    // The hardware takes the absolute and saturates vertex colors, *before* doing interpolation
    for (u32 i = 0; i < 4; ++i) {
        const f32 c = std::fabs(color[i].ToFloat32());
        color[i] = f24::FromFloat32(c < 1.0f ? c : 1.0f);
    }
}

#define ASSERT_POS(var, pos)                                                                       \
    static_assert(offsetof(OutputVertex, var) == pos * sizeof(f24), "Semantic at wrong "           \
                                                                    "offset.")

ASSERT_POS(pos, RasterizerRegs::VSOutputAttributes::POSITION_X);
ASSERT_POS(quat, RasterizerRegs::VSOutputAttributes::QUATERNION_X);
ASSERT_POS(color, RasterizerRegs::VSOutputAttributes::COLOR_R);
ASSERT_POS(tc0, RasterizerRegs::VSOutputAttributes::TEXCOORD0_U);
ASSERT_POS(tc1, RasterizerRegs::VSOutputAttributes::TEXCOORD1_U);
ASSERT_POS(tc0_w, RasterizerRegs::VSOutputAttributes::TEXCOORD0_W);
ASSERT_POS(view, RasterizerRegs::VSOutputAttributes::VIEW_X);
ASSERT_POS(tc2, RasterizerRegs::VSOutputAttributes::TEXCOORD2_U);

#undef ASSERT_POS

} // namespace Pica
