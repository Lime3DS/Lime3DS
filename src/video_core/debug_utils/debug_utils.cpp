// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <nihstro/bit_field.h>
#include <nihstro/float24.h>
#include <nihstro/shader_binary.h>
#include "common/assert.h"
#include "common/bit_field.h"
#include "common/vector_math.h"
#include "core/core.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/gpu.h"
#include "video_core/pica/regs_shader.h"
#include "video_core/pica/shader_setup.h"
#include "video_core/renderer_base.h"

using nihstro::DVLBHeader;
using nihstro::DVLEHeader;
using nihstro::DVLPHeader;

namespace Pica {

void DebugContext::DoOnEvent(Event event, const void* data) {
    {
        std::unique_lock lock{breakpoint_mutex};

        // Commit the rasterizer's caches so framebuffers, render targets, etc. will show on debug
        // widgets
        Core::System::GetInstance().GPU().Renderer().Rasterizer()->FlushAll();

        // TODO: Should stop the CPU thread here once we multithread emulation.

        active_breakpoint = event;
        at_breakpoint = true;

        // Tell all observers that we hit a breakpoint
        for (auto& breakpoint_observer : breakpoint_observers) {
            breakpoint_observer->OnPicaBreakPointHit(event, data);
        }

        // Wait until another thread tells us to Resume()
        resume_from_breakpoint.wait(lock, [&] { return !at_breakpoint; });
    }
}

void DebugContext::Resume() {
    {
        std::lock_guard lock{breakpoint_mutex};

        // Tell all observers that we are about to resume
        for (auto& breakpoint_observer : breakpoint_observers) {
            breakpoint_observer->OnPicaResume();
        }

        // Resume the waiting thread (i.e. OnEvent())
        at_breakpoint = false;
    }

    resume_from_breakpoint.notify_one();
}

std::shared_ptr<DebugContext> g_debug_context; // TODO: Get rid of this global

namespace DebugUtils {

void DumpShader(const std::string& filename, const ShaderRegs& config, const ShaderSetup& setup,
                const RasterizerRegs::VSOutputAttributes* output_attributes) {
    struct StuffToWrite {
        const u8* pointer;
        u32 size;
    };
    std::vector<StuffToWrite> writing_queue;
    u32 write_offset = 0;

    auto QueueForWriting = [&writing_queue, &write_offset](const u8* pointer, u32 size) {
        writing_queue.push_back({pointer, size});
        u32 old_write_offset = write_offset;
        write_offset += size;
        return old_write_offset;
    };

    // First off, try to translate Pica state (one enum for output attribute type and component)
    // into shbin format (separate type and component mask).
    union OutputRegisterInfo {
        enum Type : u64 {
            POSITION = 0,
            QUATERNION = 1,
            COLOR = 2,
            TEXCOORD0 = 3,
            TEXCOORD1 = 5,
            TEXCOORD2 = 6,

            VIEW = 8,
        };

        BitField<0, 64, u64> hex;

        BitField<0, 16, Type> type;
        BitField<16, 16, u64> id;
        BitField<32, 4, u64> component_mask;
    };

    // This is put into a try-catch block to make sure we notice unknown configurations.
    std::vector<OutputRegisterInfo> output_info_table;
    for (unsigned i = 0; i < 7; ++i) {
        using OutputAttributes = Pica::RasterizerRegs::VSOutputAttributes;

        // TODO: It's still unclear how the attribute components map to the register!
        //       Once we know that, this code probably will not make much sense anymore.
        std::map<OutputAttributes::Semantic, std::pair<OutputRegisterInfo::Type, u32>> map = {
            {OutputAttributes::POSITION_X, {OutputRegisterInfo::POSITION, 1}},
            {OutputAttributes::POSITION_Y, {OutputRegisterInfo::POSITION, 2}},
            {OutputAttributes::POSITION_Z, {OutputRegisterInfo::POSITION, 4}},
            {OutputAttributes::POSITION_W, {OutputRegisterInfo::POSITION, 8}},
            {OutputAttributes::QUATERNION_X, {OutputRegisterInfo::QUATERNION, 1}},
            {OutputAttributes::QUATERNION_Y, {OutputRegisterInfo::QUATERNION, 2}},
            {OutputAttributes::QUATERNION_Z, {OutputRegisterInfo::QUATERNION, 4}},
            {OutputAttributes::QUATERNION_W, {OutputRegisterInfo::QUATERNION, 8}},
            {OutputAttributes::COLOR_R, {OutputRegisterInfo::COLOR, 1}},
            {OutputAttributes::COLOR_G, {OutputRegisterInfo::COLOR, 2}},
            {OutputAttributes::COLOR_B, {OutputRegisterInfo::COLOR, 4}},
            {OutputAttributes::COLOR_A, {OutputRegisterInfo::COLOR, 8}},
            {OutputAttributes::TEXCOORD0_U, {OutputRegisterInfo::TEXCOORD0, 1}},
            {OutputAttributes::TEXCOORD0_V, {OutputRegisterInfo::TEXCOORD0, 2}},
            {OutputAttributes::TEXCOORD1_U, {OutputRegisterInfo::TEXCOORD1, 1}},
            {OutputAttributes::TEXCOORD1_V, {OutputRegisterInfo::TEXCOORD1, 2}},
            {OutputAttributes::TEXCOORD2_U, {OutputRegisterInfo::TEXCOORD2, 1}},
            {OutputAttributes::TEXCOORD2_V, {OutputRegisterInfo::TEXCOORD2, 2}},
            {OutputAttributes::VIEW_X, {OutputRegisterInfo::VIEW, 1}},
            {OutputAttributes::VIEW_Y, {OutputRegisterInfo::VIEW, 2}},
            {OutputAttributes::VIEW_Z, {OutputRegisterInfo::VIEW, 4}},
        };

        for (const auto& semantic : std::vector<OutputAttributes::Semantic>{
                 output_attributes[i].map_x, output_attributes[i].map_y, output_attributes[i].map_z,
                 output_attributes[i].map_w}) {
            if (semantic == OutputAttributes::INVALID)
                continue;

            try {
                OutputRegisterInfo::Type type = map.at(semantic).first;
                u32 component_mask = map.at(semantic).second;

                auto it = std::find_if(output_info_table.begin(), output_info_table.end(),
                                       [&i, &type](const OutputRegisterInfo& info) {
                                           return info.id == i && info.type == type;
                                       });

                if (it == output_info_table.end()) {
                    output_info_table.emplace_back();
                    output_info_table.back().type.Assign(type);
                    output_info_table.back().component_mask.Assign(component_mask);
                    output_info_table.back().id.Assign(i);
                } else {
                    it->component_mask.Assign(it->component_mask | component_mask);
                }
            } catch (const std::out_of_range&) {
                DEBUG_ASSERT_MSG(false, "Unknown output attribute mapping");
                LOG_ERROR(HW_GPU,
                          "Unknown output attribute mapping: {:03x}, {:03x}, {:03x}, {:03x}",
                          (int)output_attributes[i].map_x.Value(),
                          (int)output_attributes[i].map_y.Value(),
                          (int)output_attributes[i].map_z.Value(),
                          (int)output_attributes[i].map_w.Value());
            }
        }
    }

    struct {
        DVLBHeader header;
        u32 dvle_offset;
    } dvlb{{DVLBHeader::MAGIC_WORD, 1}}; // 1 DVLE

    DVLPHeader dvlp{DVLPHeader::MAGIC_WORD};
    DVLEHeader dvle{DVLEHeader::MAGIC_WORD};

    QueueForWriting(reinterpret_cast<const u8*>(&dvlb), sizeof(dvlb));
    u32 dvlp_offset = QueueForWriting(reinterpret_cast<const u8*>(&dvlp), sizeof(dvlp));
    dvlb.dvle_offset = QueueForWriting(reinterpret_cast<const u8*>(&dvle), sizeof(dvle));

    // TODO: Reduce the amount of binary code written to relevant portions
    dvlp.binary_offset = write_offset - dvlp_offset;
    dvlp.binary_size_words = static_cast<uint32_t>(setup.program_code.size());
    QueueForWriting(reinterpret_cast<const u8*>(setup.program_code.data()),
                    static_cast<u32>(setup.program_code.size()) * sizeof(u32));

    dvlp.swizzle_info_offset = write_offset - dvlp_offset;
    dvlp.swizzle_info_num_entries = static_cast<uint32_t>(setup.swizzle_data.size());
    u32 dummy = 0;
    for (unsigned int i = 0; i < setup.swizzle_data.size(); ++i) {
        QueueForWriting(reinterpret_cast<const u8*>(&setup.swizzle_data[i]),
                        sizeof(setup.swizzle_data[i]));
        QueueForWriting(reinterpret_cast<const u8*>(&dummy), sizeof(dummy));
    }

    dvle.main_offset_words = config.main_offset;
    dvle.output_register_table_offset = write_offset - dvlb.dvle_offset;
    dvle.output_register_table_size = static_cast<u32>(output_info_table.size());
    QueueForWriting(reinterpret_cast<const u8*>(output_info_table.data()),
                    static_cast<u32>(output_info_table.size() * sizeof(OutputRegisterInfo)));

    // TODO: Create a label table for "main"

    std::vector<nihstro::ConstantInfo> constant_table;
    for (unsigned i = 0; i < setup.uniforms.b.size(); ++i) {
        nihstro::ConstantInfo constant;
        std::memset(&constant, 0, sizeof(constant));
        constant.type = nihstro::ConstantInfo::Bool;
        constant.regid = i;
        constant.b = setup.uniforms.b[i];
        constant_table.emplace_back(constant);
    }
    for (unsigned i = 0; i < setup.uniforms.i.size(); ++i) {
        nihstro::ConstantInfo constant;
        std::memset(&constant, 0, sizeof(constant));
        constant.type = nihstro::ConstantInfo::Int;
        constant.regid = i;
        constant.i.x = setup.uniforms.i[i].x;
        constant.i.y = setup.uniforms.i[i].y;
        constant.i.z = setup.uniforms.i[i].z;
        constant.i.w = setup.uniforms.i[i].w;
        constant_table.emplace_back(constant);
    }
    for (unsigned i = 0; i < sizeof(setup.uniforms.f) / sizeof(setup.uniforms.f[0]); ++i) {
        nihstro::ConstantInfo constant;
        std::memset(&constant, 0, sizeof(constant));
        constant.type = nihstro::ConstantInfo::Float;
        constant.regid = i;
        constant.f.x = nihstro::to_float24(setup.uniforms.f[i].x.ToFloat32());
        constant.f.y = nihstro::to_float24(setup.uniforms.f[i].y.ToFloat32());
        constant.f.z = nihstro::to_float24(setup.uniforms.f[i].z.ToFloat32());
        constant.f.w = nihstro::to_float24(setup.uniforms.f[i].w.ToFloat32());

        // Store constant if it's different from zero..
        if (setup.uniforms.f[i].x.ToFloat32() != 0.0 || setup.uniforms.f[i].y.ToFloat32() != 0.0 ||
            setup.uniforms.f[i].z.ToFloat32() != 0.0 || setup.uniforms.f[i].w.ToFloat32() != 0.0)
            constant_table.emplace_back(constant);
    }
    dvle.constant_table_offset = write_offset - dvlb.dvle_offset;
    dvle.constant_table_size = static_cast<uint32_t>(constant_table.size());
    for (const auto& constant : constant_table) {
        QueueForWriting(reinterpret_cast<const u8*>(&constant), sizeof(constant));
    }

    // Write data to file
    std::ofstream file(filename, std::ios_base::out | std::ios_base::binary);

    for (const auto& chunk : writing_queue) {
        file.write(reinterpret_cast<const char*>(chunk.pointer), chunk.size);
    }
}

static std::unique_ptr<PicaTrace> pica_trace;
static std::mutex pica_trace_mutex;
bool g_is_pica_tracing = false;

void StartPicaTracing() {
    if (g_is_pica_tracing) {
        LOG_WARNING(HW_GPU, "StartPicaTracing called even though tracing already running!");
        return;
    }

    std::lock_guard lock(pica_trace_mutex);
    pica_trace = std::make_unique<PicaTrace>();

    g_is_pica_tracing = true;
}

void OnPicaRegWrite(u16 cmd_id, u16 mask, u32 value) {
    std::lock_guard lock(pica_trace_mutex);

    if (!g_is_pica_tracing)
        return;

    pica_trace->writes.push_back(PicaTrace::Write{cmd_id, mask, value});
}

std::unique_ptr<PicaTrace> FinishPicaTracing() {
    if (!g_is_pica_tracing) {
        LOG_WARNING(HW_GPU, "FinishPicaTracing called even though tracing isn't running!");
        return {};
    }

    // signalize that no further tracing should be performed
    g_is_pica_tracing = false;

    // Wait until running tracing is finished
    std::lock_guard lock(pica_trace_mutex);
    std::unique_ptr<PicaTrace> ret(std::move(pica_trace));

    return ret;
}

} // namespace DebugUtils

} // namespace Pica
