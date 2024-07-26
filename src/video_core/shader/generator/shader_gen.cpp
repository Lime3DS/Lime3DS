// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_set.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "video_core/pica/regs_internal.h"
#include "video_core/pica/shader_setup.h"
#include "video_core/shader/generator/shader_gen.h"

namespace Pica::Shader::Generator {

void PicaGSConfigState::Init(const Pica::RegsInternal& regs, bool use_clip_planes_) {
    use_clip_planes = use_clip_planes_;

    vs_output_attributes = Common::BitSet<u32>(regs.vs.output_mask).Count();
    gs_output_attributes = vs_output_attributes;

    semantic_maps.fill({16, 0});
    for (u32 attrib = 0; attrib < regs.rasterizer.vs_output_total; ++attrib) {
        const std::array semantics{
            regs.rasterizer.vs_output_attributes[attrib].map_x.Value(),
            regs.rasterizer.vs_output_attributes[attrib].map_y.Value(),
            regs.rasterizer.vs_output_attributes[attrib].map_z.Value(),
            regs.rasterizer.vs_output_attributes[attrib].map_w.Value(),
        };
        for (u32 comp = 0; comp < 4; ++comp) {
            const auto semantic = semantics[comp];
            if (static_cast<std::size_t>(semantic) < 24) {
                semantic_maps[static_cast<std::size_t>(semantic)] = {attrib, comp};
            } else if (semantic != Pica::RasterizerRegs::VSOutputAttributes::INVALID) {
                LOG_ERROR(Render, "Invalid/unknown semantic id: {}", semantic);
            }
        }
    }
}

void PicaVSConfigState::Init(const Pica::RegsInternal& regs, Pica::ShaderSetup& setup,
                             bool use_clip_planes_, bool use_geometry_shader_) {
    use_clip_planes = use_clip_planes_;
    use_geometry_shader = use_geometry_shader_;

    program_hash = setup.GetProgramCodeHash();
    swizzle_hash = setup.GetSwizzleDataHash();
    main_offset = regs.vs.main_offset;
    sanitize_mul = Settings::values.shaders_accurate_mul.GetValue();

    num_outputs = 0;
    load_flags.fill(AttribLoadFlags::Float);
    output_map.fill(16);

    for (u32 reg : Common::BitSet<u32>(regs.vs.output_mask)) {
        output_map[reg] = num_outputs++;
    }

    if (!use_geometry_shader_) {
        gs_state.Init(regs, use_clip_planes_);
    }
}

PicaVSConfig::PicaVSConfig(const Pica::RegsInternal& regs, Pica::ShaderSetup& setup,
                           bool use_clip_planes_, bool use_geometry_shader_) {
    state.Init(regs, setup, use_clip_planes_, use_geometry_shader_);
}

PicaFixedGSConfig::PicaFixedGSConfig(const Pica::RegsInternal& regs, bool use_clip_planes_) {
    state.Init(regs, use_clip_planes_);
}

} // namespace Pica::Shader::Generator
