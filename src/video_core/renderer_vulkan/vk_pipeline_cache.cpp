// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/container/static_vector.hpp>

#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/loader/loader.h"
#include "video_core/pica/shader_setup.h"
#include "video_core/renderer_vulkan/pica_to_vk.h"
#include "video_core/renderer_vulkan/vk_descriptor_update_queue.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_render_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/shader/generator/glsl_fs_shader_gen.h"
#include "video_core/shader/generator/glsl_shader_gen.h"
#include "video_core/shader/generator/spv_fs_shader_gen.h"

using namespace Pica::Shader::Generator;
using Pica::Shader::FSConfig;

MICROPROFILE_DEFINE(Vulkan_Bind, "Vulkan", "Pipeline Bind", MP_RGB(192, 32, 32));

namespace Vulkan {

u32 AttribBytes(Pica::PipelineRegs::VertexAttributeFormat format, u32 size) {
    switch (format) {
    case Pica::PipelineRegs::VertexAttributeFormat::FLOAT:
        return sizeof(float) * size;
    case Pica::PipelineRegs::VertexAttributeFormat::SHORT:
        return sizeof(u16) * size;
    case Pica::PipelineRegs::VertexAttributeFormat::BYTE:
    case Pica::PipelineRegs::VertexAttributeFormat::UBYTE:
        return sizeof(u8) * size;
    }
    return 0;
}

AttribLoadFlags MakeAttribLoadFlag(Pica::PipelineRegs::VertexAttributeFormat format) {
    switch (format) {
    case Pica::PipelineRegs::VertexAttributeFormat::BYTE:
    case Pica::PipelineRegs::VertexAttributeFormat::SHORT:
        return AttribLoadFlags::Sint;
    case Pica::PipelineRegs::VertexAttributeFormat::UBYTE:
        return AttribLoadFlags::Uint;
    default:
        return AttribLoadFlags::Float;
    }
}

constexpr std::array<vk::DescriptorSetLayoutBinding, 6> BUFFER_BINDINGS = {{
    {0, vk::DescriptorType::eStorageBufferDynamic, 1, vk::ShaderStageFlagBits::eVertex},
    {1, vk::DescriptorType::eUniformBufferDynamic, 1,
     vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry},
    {2, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eFragment},
    {3, vk::DescriptorType::eUniformTexelBuffer, 1, vk::ShaderStageFlagBits::eFragment},
    {4, vk::DescriptorType::eUniformTexelBuffer, 1, vk::ShaderStageFlagBits::eFragment},
    {5, vk::DescriptorType::eUniformTexelBuffer, 1, vk::ShaderStageFlagBits::eFragment},
}};

template <u32 NumTex0>
constexpr std::array<vk::DescriptorSetLayoutBinding, 3> TEXTURE_BINDINGS = {{
    {0, vk::DescriptorType::eCombinedImageSampler, NumTex0,
     vk::ShaderStageFlagBits::eFragment},                                                  // tex0
    {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // tex1
    {2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // tex2
}};

constexpr std::array<vk::DescriptorSetLayoutBinding, 2> UTILITY_BINDINGS = {{
    {0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eFragment}, // shadow_buffer
    {1, vk::DescriptorType::eCombinedImageSampler, 1,
     vk::ShaderStageFlagBits::eFragment}, // tex_normal
}};

PipelineCache::PipelineCache(const Instance& instance_, Scheduler& scheduler_,
                             RenderManager& renderpass_cache_, DescriptorUpdateQueue& update_queue_)
    : instance{instance_}, scheduler{scheduler_}, renderpass_cache{renderpass_cache_},
      update_queue{update_queue_},
      num_worker_threads{std::max(std::thread::hardware_concurrency(), 2U) / 2},
      pipeline_workers{num_worker_threads, "Pipeline workers"},
      shader_workers{num_worker_threads, "Shader workers"},
      descriptor_heaps{
          DescriptorHeap{instance, scheduler.GetMasterSemaphore(), BUFFER_BINDINGS, 32},
          DescriptorHeap{instance, scheduler.GetMasterSemaphore(), TEXTURE_BINDINGS<1>},
          DescriptorHeap{instance, scheduler.GetMasterSemaphore(), UTILITY_BINDINGS, 32}},
      trivial_vertex_shader{
          instance, vk::ShaderStageFlagBits::eVertex,
          GLSL::GenerateTrivialVertexShader(instance.IsShaderClipDistanceSupported(), true)} {
    scheduler.RegisterOnDispatch([this] { update_queue.Flush(); });
    profile = Pica::Shader::Profile{
        .enable_accurate_mul = false,
        .has_separable_shaders = true,
        .has_clip_planes = instance.IsShaderClipDistanceSupported(),
        .has_geometry_shader = instance.UseGeometryShaders(),
        .has_custom_border_color = instance.IsCustomBorderColorSupported(),
        .has_fragment_shader_interlock = instance.IsFragmentShaderInterlockSupported(),
        .has_fragment_shader_barycentric = instance.IsFragmentShaderBarycentricSupported(),
        .has_blend_minmax_factor = false,
        .has_minus_one_to_one_range = false,
        .has_logic_op = !instance.NeedsLogicOpEmulation(),
        .vk_disable_spirv_optimizer = Settings::values.disable_spirv_optimizer.GetValue(),
        .vk_use_spirv_generator = Settings::values.spirv_shader_gen.GetValue(),
        .is_vulkan = true,
    };

    const auto& traits = instance.GetAllTraits();
    size_t i = 0;
    for (const auto& it : traits) {
        profile.vk_format_traits[i].transfer_support = it.transfer_support;
        profile.vk_format_traits[i].blit_support = it.blit_support;
        profile.vk_format_traits[i].attachment_support = it.attachment_support;
        profile.vk_format_traits[i].storage_support = it.storage_support;
        profile.vk_format_traits[i].needs_conversion = it.needs_conversion;
        profile.vk_format_traits[i].needs_emulation = it.needs_emulation;
        profile.vk_format_traits[i].usage_flags = static_cast<u32>(it.usage);
        profile.vk_format_traits[i].aspect_flags = static_cast<u32>(it.aspect);
        profile.vk_format_traits[i].native_format = static_cast<u32>(it.native);
        ++i;
    }

    BuildLayout();
}

void PipelineCache::BuildLayout() {
    std::array<vk::DescriptorSetLayout, NumRasterizerSets> descriptor_set_layouts;
    descriptor_set_layouts[0] = descriptor_heaps[0].Layout();
    descriptor_set_layouts[1] = descriptor_heaps[1].Layout();
    descriptor_set_layouts[2] = descriptor_heaps[2].Layout();

    const vk::PipelineLayoutCreateInfo layout_info = {
        .setLayoutCount = NumRasterizerSets,
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };
    pipeline_layout = instance.GetDevice().createPipelineLayoutUnique(layout_info);
}

PipelineCache::~PipelineCache() {
    pipeline_workers.WaitForRequests();
    shader_workers.WaitForRequests();
    SaveDriverPipelineDiskCache();
}

void PipelineCache::LoadCache(const std::atomic_bool& stop_loading,
                              const VideoCore::DiskResourceLoadCallback& callback) {
    LoadDriverPipelineDiskCache(stop_loading, callback);
    LoadDiskCache(stop_loading, callback);
}

void PipelineCache::SwitchCache(u64 title_id, const std::atomic_bool& stop_loading,
                                const VideoCore::DiskResourceLoadCallback& callback) {
    if (GetProgramID() == title_id) {
        LOG_DEBUG(Render_Vulkan,
                  "Skipping pipeline cache switch - already using cache for title_id={:016X}",
                  title_id);
        return;
    }

    // Make sure we have a valid pipeline cache before switching
    if (!driver_pipeline_cache) {
        vk::PipelineCacheCreateInfo cache_info{};
        try {
            driver_pipeline_cache = instance.GetDevice().createPipelineCacheUnique(cache_info);
        } catch (const vk::SystemError& err) {
            LOG_ERROR(Render_Vulkan, "Failed to create pipeline cache: {}", err.what());
            return;
        }
    }

    LOG_INFO(Render_Vulkan, "Switching pipeline cache to title_id={:016X}", title_id);

    // Save current driver cache, update program ID and load the new driver cache
    SaveDriverPipelineDiskCache();
    SetProgramID(title_id);
    LoadDriverPipelineDiskCache(stop_loading, nullptr);

    // Switch the disk shader cache after driver cache is switched
    SwitchDiskCache(title_id, stop_loading, callback);
}

void PipelineCache::LoadDriverPipelineDiskCache(
    const std::atomic_bool& stop_loading, const VideoCore::DiskResourceLoadCallback& callback) {
    vk::PipelineCacheCreateInfo cache_info{};

    if (callback) {
        callback(VideoCore::LoadCallbackStage::Build, 0, 1, "Driver Pipeline Cache");
    }

    auto load_cache = [this, &cache_info, &callback](bool allow_fallback) {
        const vk::Device device = instance.GetDevice();
        try {
            driver_pipeline_cache = device.createPipelineCacheUnique(cache_info);
        } catch (const vk::SystemError& err) {
            LOG_ERROR(Render_Vulkan, "Failed to create pipeline cache: {}", err.what());
            if (allow_fallback) {
                // Fall back to empty cache
                cache_info.initialDataSize = 0;
                cache_info.pInitialData = nullptr;
                try {
                    driver_pipeline_cache = device.createPipelineCacheUnique(cache_info);
                } catch (const vk::SystemError& err) {
                    LOG_ERROR(Render_Vulkan, "Failed to create fallback pipeline cache: {}",
                              err.what());
                }
            }
        }
        if (callback) {
            callback(VideoCore::LoadCallbackStage::Build, 1, 1, "Driver Pipeline Cache");
        }
    };

    // Try to load existing pipeline cache if disk cache is enabled and directories exist
    if (!Settings::values.use_disk_shader_cache || !EnsureDirectories()) {
        load_cache(false);
        return;
    }

    // Try to load existing pipeline cache for this game/device combination
    const auto cache_dir = GetPipelineCacheDir();
    const u32 vendor_id = instance.GetVendorID();
    const u32 device_id = instance.GetDeviceID();
    const u64 program_id = GetProgramID();
    const auto cache_file_path =
        fmt::format("{}{:016X}-{:X}{:X}.bin", cache_dir, program_id, vendor_id, device_id);

    std::vector<u8> cache_data;
    FileUtil::IOFile cache_file{cache_file_path, "rb"};

    if (!cache_file.IsOpen()) {
        LOG_INFO(Render_Vulkan, "No pipeline cache found for title_id={:016X}", program_id);
        load_cache(false);
        return;
    }

    const u64 cache_file_size = cache_file.GetSize();
    cache_data.resize(cache_file_size);

    if (cache_file.ReadBytes(cache_data.data(), cache_file_size) != cache_file_size) {
        LOG_ERROR(Render_Vulkan, "Error reading pipeline cache");
        load_cache(false);
        return;
    }

    if (!IsCacheValid(cache_data)) {
        LOG_WARNING(Render_Vulkan, "Pipeline cache invalid, removing");
        cache_file.Close();
        FileUtil::Delete(cache_file_path);
        load_cache(false);
        return;
    }

    LOG_INFO(Render_Vulkan, "Loading pipeline cache for title_id={:016X} with size {} KB",
             program_id, cache_file_size / 1024);

    cache_info.initialDataSize = cache_file_size;
    cache_info.pInitialData = cache_data.data();
    load_cache(true);
}

void PipelineCache::SaveDriverPipelineDiskCache() {
    // Save Vulkan pipeline cache
    if (!Settings::values.use_disk_shader_cache || !driver_pipeline_cache) {
        return;
    }

    const auto cache_dir = GetPipelineCacheDir();
    const u32 vendor_id = instance.GetVendorID();
    const u32 device_id = instance.GetDeviceID();
    const u64 program_id = GetProgramID();
    // Include both device info and program id in cache path to handle both GPU changes and
    // different games
    const auto cache_file_path =
        fmt::format("{}{:016X}-{:X}{:X}.bin", cache_dir, program_id, vendor_id, device_id);

    FileUtil::IOFile cache_file{cache_file_path, "wb"};
    if (!cache_file.IsOpen()) {
        LOG_ERROR(Render_Vulkan, "Unable to open pipeline cache for writing");
        return;
    }

    const vk::Device device = instance.GetDevice();
    const auto cache_data = device.getPipelineCacheData(*driver_pipeline_cache);
    if (cache_file.WriteBytes(cache_data.data(), cache_data.size()) != cache_data.size()) {
        LOG_ERROR(Render_Vulkan, "Error during pipeline cache write");
        return;
    }
}

void PipelineCache::LoadDiskCache(const std::atomic_bool& stop_loading,
                                  const VideoCore::DiskResourceLoadCallback& callback) {

    disk_caches.clear();
    curr_disk_cache =
        disk_caches.emplace_back(std::make_shared<ShaderDiskCache>(*this, GetProgramID()));

    curr_disk_cache->Init(stop_loading, callback);
}

void PipelineCache::SwitchDiskCache(u64 title_id, const std::atomic_bool& stop_loading,
                                    const VideoCore::DiskResourceLoadCallback& callback) {
    // NOTE: curr_disk_cache can be null if emulation restarted without calling
    // LoadDefaultDiskResources

    // Check if the current cache is for the specified TID.
    if (curr_disk_cache && curr_disk_cache->GetProgramID() == title_id) {
        return;
    }

    // Search for an existing manager
    size_t new_pos = 0;
    for (new_pos = 0; new_pos < disk_caches.size(); new_pos++) {
        if (disk_caches[new_pos]->GetProgramID() == title_id) {
            break;
        }
    }
    // Manager does not exist, create it and append to the end
    if (new_pos >= disk_caches.size()) {
        new_pos = disk_caches.size();
        auto& new_manager =
            disk_caches.emplace_back(std::make_shared<ShaderDiskCache>(*this, title_id));

        new_manager->Init(stop_loading, callback);
    }

    auto is_applet = [](u64 tid) {
        constexpr u32 APPLET_TID_HIGH = 0x00040030;
        return static_cast<u32>(tid >> 32) == APPLET_TID_HIGH;
    };

    bool prev_applet = curr_disk_cache ? is_applet(curr_disk_cache->GetProgramID()) : false;
    bool new_applet = is_applet(disk_caches[new_pos]->GetProgramID());
    curr_disk_cache = disk_caches[new_pos];

    if (prev_applet) {
        // If we came from an applet, clean up all other applets
        for (auto it = disk_caches.begin(); it != disk_caches.end();) {
            if (it == disk_caches.begin() || *it == curr_disk_cache ||
                !is_applet((*it)->GetProgramID())) {
                it++;
                continue;
            }
            it = disk_caches.erase(it);
        }
    }
    if (!new_applet) {
        // If we are going into a non-applet, clean up everything
        for (auto it = disk_caches.begin(); it != disk_caches.end();) {
            if (it == disk_caches.begin() || *it == curr_disk_cache) {
                it++;
                continue;
            }
            it = disk_caches.erase(it);
        }
    }
}

bool PipelineCache::BindPipeline(PipelineInfo& info, bool wait_built) {
    MICROPROFILE_SCOPE(Vulkan_Bind);

    for (u32 i = 0; i < MAX_SHADER_STAGES; i++) {
        info.state.shader_ids[i] = shader_hashes[i];
    }

    GraphicsPipeline* const pipeline = curr_disk_cache->GetPipeline(info);
    if (!pipeline->IsDone() && !pipeline->TryBuild(wait_built)) {
        return false;
    }

    const bool is_dirty = scheduler.IsStateDirty(StateFlags::Pipeline);
    const bool pipeline_dirty = (current_pipeline != pipeline) || is_dirty;
    scheduler.Record([this, is_dirty, pipeline_dirty, pipeline,
                      current_dynamic = current_info.dynamic_info, dynamic = info.dynamic_info,
                      descriptor_sets = bound_descriptor_sets, offsets = offsets,
                      current_rasterization = current_info.state.rasterization,
                      current_depth_stencil = current_info.state.depth_stencil,
                      rasterization = info.state.rasterization,
                      depth_stencil = info.state.depth_stencil](vk::CommandBuffer cmdbuf) {
        if (dynamic.viewport != current_dynamic.viewport || is_dirty) {
            const vk::Viewport vk_viewport = {
                .x = static_cast<f32>(dynamic.viewport.left),
                .y = static_cast<f32>(dynamic.viewport.top),
                .width = static_cast<f32>(dynamic.viewport.GetWidth()),
                .height = static_cast<f32>(dynamic.viewport.GetHeight()),
                .minDepth = 0.f,
                .maxDepth = 1.f,
            };
            cmdbuf.setViewport(0, vk_viewport);
        }

        if (dynamic.scissor != current_dynamic.scissor || is_dirty) {
            const vk::Rect2D scissor = {
                .offset{
                    .x = static_cast<s32>(dynamic.scissor.left),
                    .y = static_cast<s32>(dynamic.scissor.bottom),
                },
                .extent{
                    .width = dynamic.scissor.GetWidth(),
                    .height = dynamic.scissor.GetHeight(),
                },
            };
            cmdbuf.setScissor(0, scissor);
        }

        if (dynamic.stencil_compare_mask != current_dynamic.stencil_compare_mask || is_dirty) {
            cmdbuf.setStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack,
                                         dynamic.stencil_compare_mask);
        }

        if (dynamic.stencil_write_mask != current_dynamic.stencil_write_mask || is_dirty) {
            cmdbuf.setStencilWriteMask(vk::StencilFaceFlagBits::eFrontAndBack,
                                       dynamic.stencil_write_mask);
        }

        if (dynamic.stencil_reference != current_dynamic.stencil_reference || is_dirty) {
            cmdbuf.setStencilReference(vk::StencilFaceFlagBits::eFrontAndBack,
                                       dynamic.stencil_reference);
        }

        if (dynamic.blend_color != current_dynamic.blend_color || is_dirty) {
            const Common::Vec4f color = PicaToVK::ColorRGBA8(dynamic.blend_color);
            cmdbuf.setBlendConstants(color.AsArray());
        }

        if (instance.IsExtendedDynamicStateSupported()) {
            const bool needs_flip =
                rasterization.flip_viewport != current_rasterization.flip_viewport;
            if (rasterization.cull_mode != current_rasterization.cull_mode || needs_flip ||
                is_dirty) {
                cmdbuf.setCullModeEXT(
                    PicaToVK::CullMode(rasterization.cull_mode, rasterization.flip_viewport));
                cmdbuf.setFrontFaceEXT(PicaToVK::FrontFace(rasterization.cull_mode));
            }

            if (depth_stencil.depth_compare_op != current_depth_stencil.depth_compare_op ||
                is_dirty) {
                cmdbuf.setDepthCompareOpEXT(PicaToVK::CompareFunc(depth_stencil.depth_compare_op));
            }

            if (depth_stencil.depth_test_enable != current_depth_stencil.depth_test_enable ||
                is_dirty) {
                cmdbuf.setDepthTestEnableEXT(depth_stencil.depth_test_enable);
            }

            if (depth_stencil.depth_write_enable != current_depth_stencil.depth_write_enable ||
                is_dirty) {
                cmdbuf.setDepthWriteEnableEXT(depth_stencil.depth_write_enable);
            }

            if (rasterization.topology != current_rasterization.topology || is_dirty) {
                cmdbuf.setPrimitiveTopologyEXT(PicaToVK::PrimitiveTopology(rasterization.topology));
            }

            if (depth_stencil.stencil_test_enable != current_depth_stencil.stencil_test_enable ||
                is_dirty) {
                cmdbuf.setStencilTestEnableEXT(depth_stencil.stencil_test_enable);
            }

            if (depth_stencil.stencil_fail_op != current_depth_stencil.stencil_fail_op ||
                depth_stencil.stencil_pass_op != current_depth_stencil.stencil_pass_op ||
                depth_stencil.stencil_depth_fail_op !=
                    current_depth_stencil.stencil_depth_fail_op ||
                depth_stencil.stencil_compare_op != current_depth_stencil.stencil_compare_op ||
                is_dirty) {
                cmdbuf.setStencilOpEXT(vk::StencilFaceFlagBits::eFrontAndBack,
                                       PicaToVK::StencilOp(depth_stencil.stencil_fail_op),
                                       PicaToVK::StencilOp(depth_stencil.stencil_pass_op),
                                       PicaToVK::StencilOp(depth_stencil.stencil_depth_fail_op),
                                       PicaToVK::CompareFunc(depth_stencil.stencil_compare_op));
            }
        }

        if (pipeline_dirty) {
            if (!pipeline->IsDone()) {
                pipeline->WaitDone();
            }
            cmdbuf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->Handle());
        }

        cmdbuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline_layout, 0,
                                  descriptor_sets, offsets);
    });

    current_info = info;
    current_pipeline = pipeline;
    scheduler.MarkStateNonDirty(StateFlags::Pipeline | StateFlags::DescriptorSets);

    return true;
}

ExtraVSConfig PipelineCache::CalcExtraConfig(const PicaVSConfig& config) {
    auto res = ExtraVSConfig();

    // Enable the geometry-shader only if we are actually doing per-fragment lighting
    // and care about proper quaternions. Otherwise just use standard vertex+fragment shaders.
    // We also don't need the geometry shader if we have the barycentric extension.
    const bool use_geometry_shader = instance.UseGeometryShaders() &&
                                     !config.state.lighting_disable &&
                                     !instance.IsFragmentShaderBarycentricSupported();

    res.use_clip_planes = instance.IsShaderClipDistanceSupported();
    res.use_geometry_shader = use_geometry_shader;
    res.sanitize_mul = profile.enable_accurate_mul;
    res.separable_shader = true;
    res.load_flags.fill(AttribLoadFlags::Float);

    for (u32 i = 0; i < config.state.used_input_vertex_attributes; i++) {
        const auto& attr = config.state.input_vertex_attributes[i];
        const u32 location = attr.location;
        const Pica::PipelineRegs::VertexAttributeFormat type =
            static_cast<Pica::PipelineRegs::VertexAttributeFormat>(attr.type);
        const FormatTraits& traits = instance.GetTraits(type, attr.size);
        AttribLoadFlags& flags = res.load_flags[location];

        if (traits.needs_conversion) {
            flags = MakeAttribLoadFlag(type);
        }
        if (traits.needs_emulation) {
            flags |= AttribLoadFlags::ZeroW;
        }
    }

    return res;
}

bool PipelineCache::UseProgrammableVertexShader(const Pica::RegsInternal& regs,
                                                Pica::ShaderSetup& setup,
                                                const VertexLayout& layout) {

    auto res = curr_disk_cache->UseProgrammableVertexShader(regs, setup, layout);

    if (res.has_value()) {
        current_shaders[ProgramType::VS] = (*res).second;
        shader_hashes[ProgramType::VS] = (*res).first;
        return true;
    }

    return false;
}

void PipelineCache::UseTrivialVertexShader() {
    current_shaders[ProgramType::VS] = &trivial_vertex_shader;
    shader_hashes[ProgramType::VS] = 0;
}

bool PipelineCache::UseFixedGeometryShader(const Pica::RegsInternal& regs) {

    auto res = curr_disk_cache->UseFixedGeometryShader(regs);

    if (res.has_value()) {
        current_shaders[ProgramType::GS] = (*res).second;
        shader_hashes[ProgramType::GS] = (*res).first;
        return true;
    }

    return false;
}

void PipelineCache::UseTrivialGeometryShader() {
    current_shaders[ProgramType::GS] = nullptr;
    shader_hashes[ProgramType::GS] = 0;
}

void PipelineCache::UseFragmentShader(const Pica::RegsInternal& regs,
                                      const Pica::Shader::UserConfig& user) {

    auto res = curr_disk_cache->UseFragmentShader(regs, user);

    if (res.has_value()) {
        current_shaders[ProgramType::FS] = (*res).second;
        shader_hashes[ProgramType::FS] = (*res).first;
    }
}

bool PipelineCache::IsCacheValid(std::span<const u8> data) const {
    if (data.size() < sizeof(vk::PipelineCacheHeaderVersionOne)) {
        LOG_ERROR(Render_Vulkan, "Pipeline cache failed validation: Invalid header");
        return false;
    }

    vk::PipelineCacheHeaderVersionOne header;
    std::memcpy(&header, data.data(), sizeof(header));
    if (header.headerSize < sizeof(header)) {
        LOG_ERROR(Render_Vulkan, "Pipeline cache failed validation: Invalid header length");
        return false;
    }

    if (header.headerVersion != vk::PipelineCacheHeaderVersion::eOne) {
        LOG_ERROR(Render_Vulkan, "Pipeline cache failed validation: Invalid header version");
        return false;
    }

    if (u32 vendor_id = instance.GetVendorID(); header.vendorID != vendor_id) {
        LOG_ERROR(
            Render_Vulkan,
            "Pipeline cache failed validation: Incorrect vendor ID (file: {:#X}, device: {:#X})",
            header.vendorID, vendor_id);
        return false;
    }

    if (u32 device_id = instance.GetDeviceID(); header.deviceID != device_id) {
        LOG_ERROR(
            Render_Vulkan,
            "Pipeline cache failed validation: Incorrect device ID (file: {:#X}, device: {:#X})",
            header.deviceID, device_id);
        return false;
    }

    if (header.pipelineCacheUUID != instance.GetPipelineCacheUUID()) {
        LOG_ERROR(Render_Vulkan, "Pipeline cache failed validation: Incorrect UUID");
        return false;
    }

    return true;
}

bool PipelineCache::EnsureDirectories() const {
    const auto create_dir = [](const std::string& dir) {
        if (!FileUtil::CreateDir(dir)) {
            LOG_ERROR(Render_Vulkan, "Failed to create directory={}", dir);
            return false;
        }

        return true;
    };

    return create_dir(FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir)) &&
           create_dir(GetVulkanDir()) && create_dir(GetPipelineCacheDir()) &&
           create_dir(GetTransferableDir());
}

std::string PipelineCache::GetVulkanDir() const {
    return FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir) + "vulkan" + DIR_SEP;
}

std::string PipelineCache::GetPipelineCacheDir() const {
    return GetVulkanDir() + "pipeline" + DIR_SEP;
}

std::string PipelineCache::GetTransferableDir() const {
    return GetVulkanDir() + DIR_SEP + "transferable";
}

} // namespace Vulkan
