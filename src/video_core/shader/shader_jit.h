// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/arch.h"
#if LIME3DS_ARCH(x86_64) || LIME3DS_ARCH(arm64)

#include <memory>
#include <unordered_map>
#include "common/common_types.h"
#include "video_core/shader/shader.h"

namespace Pica::Shader {

class JitShader;

class JitEngine final : public ShaderEngine {
public:
    JitEngine();
    ~JitEngine() override;

    void SetupBatch(ShaderSetup& setup, u32 entry_point) override;
    void Run(const ShaderSetup& setup, ShaderUnit& state) const override;

private:
    std::unordered_map<u64, std::unique_ptr<JitShader>> cache;
};

} // namespace Pica::Shader

#endif // LIME3DS_ARCH(x86_64) || LIME3DS_ARCH(arm64)
