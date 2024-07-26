// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/bit_set.h"
#include "common/hash.h"
#include "common/logging/log.h"
#include "video_core/pica/regs_shader.h"
#include "video_core/pica/shader_setup.h"

namespace Pica {

ShaderSetup::ShaderSetup() = default;

ShaderSetup::~ShaderSetup() = default;

void ShaderSetup::WriteUniformBoolReg(u32 value) {
    const auto bits = BitSet32(value);
    for (u32 i = 0; i < uniforms.b.size(); ++i) {
        uniforms.b[i] = bits[i];
    }
}

void ShaderSetup::WriteUniformIntReg(u32 index, const Common::Vec4<u8> values) {
    ASSERT(index < uniforms.i.size());
    uniforms.i[index] = values;
}

std::optional<u32> ShaderSetup::WriteUniformFloatReg(ShaderRegs& config, u32 value) {
    auto& uniform_setup = config.uniform_setup;
    const bool is_float32 = uniform_setup.IsFloat32();
    if (!uniform_queue.Push(value, is_float32)) {
        return std::nullopt;
    }

    const auto uniform = uniform_queue.Get(is_float32);
    if (uniform_setup.index >= uniforms.f.size()) {
        LOG_ERROR(HW_GPU, "Invalid float uniform index {}", uniform_setup.index.Value());
        return std::nullopt;
    }

    const u32 index = uniform_setup.index.Value();
    uniforms.f[index] = uniform;
    uniform_setup.index.Assign(index + 1);
    return index;
}

u64 ShaderSetup::GetProgramCodeHash() {
    if (program_code_hash_dirty) {
        program_code_hash = Common::ComputeHash64(&program_code, sizeof(program_code));
        program_code_hash_dirty = false;
    }
    return program_code_hash;
}

u64 ShaderSetup::GetSwizzleDataHash() {
    if (swizzle_data_hash_dirty) {
        swizzle_data_hash = Common::ComputeHash64(&swizzle_data, sizeof(swizzle_data));
        swizzle_data_hash_dirty = false;
    }
    return swizzle_data_hash;
}

} // namespace Pica
