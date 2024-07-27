// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/pica/output_vertex.h"
#include "video_core/shader/debug_data.h"
#include "video_core/shader/shader.h"

namespace Pica {
struct ShaderRegs;
}

namespace Pica::Shader {

class InterpreterEngine final : public ShaderEngine {
public:
    void SetupBatch(ShaderSetup& setup, u32 entry_point) override;
    void Run(const ShaderSetup& setup, ShaderUnit& state) const override;

    /**
     * Produce debug information based on the given shader and input vertex
     * @param setup  Shader engine state
     * @param input  Input vertex into the shader
     * @param config Configuration object for the shader pipeline
     * @return Debug information for this shader with regards to the given vertex
     */
    DebugData<true> ProduceDebugInfo(const ShaderSetup& setup, const AttributeBuffer& input,
                                     const ShaderRegs& config) const;
};

} // namespace Pica::Shader
