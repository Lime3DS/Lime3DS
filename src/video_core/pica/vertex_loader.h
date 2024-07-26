// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/memory.h"
#include "video_core/pica/output_vertex.h"
#include "video_core/pica/regs_pipeline.h"

namespace Memory {
class MemorySystem;
}

namespace Pica {

class VertexLoader {
public:
    explicit VertexLoader(Memory::MemorySystem& memory_, const PipelineRegs& regs);
    ~VertexLoader();

    void LoadVertex(PAddr base_address, u32 index, u32 vertex, AttributeBuffer& input,
                    AttributeBuffer& input_default_attributes) const;

    template <typename T>
    void LoadAttribute(PAddr source_addr, u32 attrib, AttributeBuffer& out) const {
        const T* data = reinterpret_cast<const T*>(memory.GetPhysicalPointer(source_addr));
        for (u32 comp = 0; comp < vertex_attribute_elements[attrib]; ++comp) {
            out[attrib][comp] = f24::FromFloat32(data[comp]);
        }
    }

    int GetNumTotalAttributes() const {
        return num_total_attributes;
    }

private:
    Memory::MemorySystem& memory;
    std::array<u32, 16> vertex_attribute_sources;
    std::array<u32, 16> vertex_attribute_strides{};
    std::array<PipelineRegs::VertexAttributeFormat, 16> vertex_attribute_formats;
    std::array<u32, 16> vertex_attribute_elements{};
    std::array<bool, 16> vertex_attribute_is_default;
    int num_total_attributes = 0;
};

} // namespace Pica
