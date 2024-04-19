// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <bitset>
#include <tsl/robin_map.h>

#include "video_core/renderer_vulkan/vk_graphics_pipeline.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"
#include "video_core/shader/generator/pica_fs_config.h"
#include "video_core/shader/generator/profile.h"
#include "video_core/shader/generator/shader_gen.h"

namespace Pica {
struct RegsInternal;
struct ShaderSetup;
} // namespace Pica

namespace Vulkan {

class Instance;
class Scheduler;
class RenderManager;
class DescriptorUpdateQueue;

enum class DescriptorHeapType : u32 {
    Buffer,
    Texture,
    Utility,
};

/**
 * Stores a collection of rasterizer pipelines used during rendering.
 */
class PipelineCache {
    static constexpr u32 NumRasterizerSets = 3;
    static constexpr u32 NumDescriptorHeaps = 3;
    static constexpr u32 NumDynamicOffsets = 3;

public:
    explicit PipelineCache(const Instance& instance, Scheduler& scheduler,
                           RenderManager& renderpass_cache, DescriptorUpdateQueue& update_queue);
    ~PipelineCache();

    /// Acquires and binds a free descriptor set from the appropriate heap.
    vk::DescriptorSet Acquire(DescriptorHeapType type) {
        const u32 index = static_cast<u32>(type);
        const auto descriptor_set = descriptor_heaps[index].Commit();
        bound_descriptor_sets[index] = descriptor_set;
        return descriptor_set;
    }

    /// Sets the dynamic offset for the uniform buffer at binding
    void UpdateRange(u8 binding, u32 offset) {
        offsets[binding] = offset;
    }

    /// Loads the pipeline cache stored to disk
    void LoadDiskCache();

    /// Stores the generated pipeline cache to disk
    void SaveDiskCache();

    /// Binds a pipeline using the provided information
    bool BindPipeline(const PipelineInfo& info, bool wait_built = false);

    /// Binds a PICA decompiled vertex shader
    bool UseProgrammableVertexShader(const Pica::RegsInternal& regs, Pica::ShaderSetup& setup,
                                     const VertexLayout& layout);

    /// Binds a passthrough vertex shader
    void UseTrivialVertexShader();

    /// Binds a PICA decompiled geometry shader
    bool UseFixedGeometryShader(const Pica::RegsInternal& regs);

    /// Binds a passthrough geometry shader
    void UseTrivialGeometryShader();

    /// Binds a fragment shader generated from PICA state
    void UseFragmentShader(const Pica::RegsInternal& regs, const Pica::Shader::UserConfig& user);

private:
    /// Builds the rasterizer pipeline layout
    void BuildLayout();

    /// Returns true when the disk data can be used by the current driver
    bool IsCacheValid(std::span<const u8> cache_data) const;

    /// Create shader disk cache directories. Returns true on success.
    bool EnsureDirectories() const;

    /// Returns the pipeline cache storage dir
    std::string GetPipelineCacheDir() const;

private:
    const Instance& instance;
    Scheduler& scheduler;
    RenderManager& renderpass_cache;
    DescriptorUpdateQueue& update_queue;

    Pica::Shader::Profile profile{};
    vk::UniquePipelineCache pipeline_cache;
    vk::UniquePipelineLayout pipeline_layout;
    std::size_t num_worker_threads;
    Common::ThreadWorker workers;
    PipelineInfo current_info{};
    GraphicsPipeline* current_pipeline{};
    tsl::robin_map<u64, std::unique_ptr<GraphicsPipeline>, Common::IdentityHash<u64>>
        graphics_pipelines;

    std::array<DescriptorHeap, NumDescriptorHeaps> descriptor_heaps;
    std::array<vk::DescriptorSet, NumRasterizerSets> bound_descriptor_sets{};
    std::array<u32, NumDynamicOffsets> offsets{};

    std::array<u64, MAX_SHADER_STAGES> shader_hashes;
    std::array<Shader*, MAX_SHADER_STAGES> current_shaders;
    std::unordered_map<Pica::Shader::Generator::PicaVSConfig, Shader*> programmable_vertex_map;
    std::unordered_map<std::string, Shader> programmable_vertex_cache;
    std::unordered_map<Pica::Shader::Generator::PicaFixedGSConfig, Shader> fixed_geometry_shaders;
    std::unordered_map<Pica::Shader::FSConfig, Shader> fragment_shaders;
    Shader trivial_vertex_shader;
};

} // namespace Vulkan
