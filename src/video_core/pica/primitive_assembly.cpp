// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "video_core/pica/primitive_assembly.h"

namespace Pica {

PrimitiveAssembler::PrimitiveAssembler(PipelineRegs::TriangleTopology topology)
    : topology(topology) {}

void PrimitiveAssembler::SubmitVertex(const OutputVertex& vtx,
                                      const TriangleHandler& triangle_handler) {
    switch (topology) {
    case PipelineRegs::TriangleTopology::List:
    case PipelineRegs::TriangleTopology::Shader:
        if (buffer_index < 2) {
            buffer[buffer_index++] = vtx;
        } else {
            buffer_index = 0;
            if (topology == PipelineRegs::TriangleTopology::Shader && winding) {
                triangle_handler(buffer[1], buffer[0], vtx);
                winding = false;
            } else {
                triangle_handler(buffer[0], buffer[1], vtx);
            }
        }
        break;

    case PipelineRegs::TriangleTopology::Strip:
    case PipelineRegs::TriangleTopology::Fan:
        if (strip_ready) {
            triangle_handler(buffer[0], buffer[1], vtx);
        }

        buffer[buffer_index] = vtx;
        strip_ready |= (buffer_index == 1);

        if (topology == PipelineRegs::TriangleTopology::Strip) {
            buffer_index = !buffer_index;
        } else if (topology == PipelineRegs::TriangleTopology::Fan) {
            buffer_index = 1;
        }
        break;

    default:
        LOG_ERROR(HW_GPU, "Unknown triangle topology {:x}:", (int)topology);
        break;
    }
}

} // namespace Pica
