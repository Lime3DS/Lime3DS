// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <bitset>
#include <tsl/robin_map.h>

#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_graphics_pipeline.h"
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
class DescriptorPool;

constexpr u32 NUM_RASTERIZER_SETS = 3;
constexpr u32 NUM_DYNAMIC_OFFSETS = 3;

/**
 * Stores a collection of rasterizer pipelines used during rendering.
 */
class PipelineCache {
public:
    explicit PipelineCache(const Instance& instance, Scheduler& scheduler,
                           RenderManager& render_manager, DescriptorPool& pool);
    ~PipelineCache();

    [[nodiscard]] DescriptorSetProvider& TextureProvider() noexcept {
        return descriptor_set_providers[1];
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

    /// Binds a texture to the specified binding
    void BindTexture(u32 binding, vk::ImageView image_view, vk::Sampler sampler);

    /// Binds a storage image to the specified binding
    void BindStorageImage(u32 binding, vk::ImageView image_view);

    /// Binds a buffer to the specified binding
    void BindBuffer(u32 binding, vk::Buffer buffer, u32 offset, u32 size);

    /// Binds a buffer to the specified binding
    void BindTexelBuffer(u32 binding, vk::BufferView buffer_view);

    /// Sets the dynamic offset for the uniform buffer at binding
    void SetBufferOffset(u32 binding, std::size_t offset);

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
    RenderManager& render_manager;
    DescriptorPool& pool;

    Pica::Shader::Profile profile{};
    vk::UniquePipelineCache pipeline_cache;
    vk::UniquePipelineLayout pipeline_layout;
    std::size_t num_worker_threads;
    Common::ThreadWorker workers;
    PipelineInfo current_info{};
    GraphicsPipeline* current_pipeline{};
    tsl::robin_map<u64, std::unique_ptr<GraphicsPipeline>, Common::IdentityHash<u64>>
        graphics_pipelines;

    std::array<DescriptorSetProvider, NUM_RASTERIZER_SETS> descriptor_set_providers;
    std::array<DescriptorSetData, NUM_RASTERIZER_SETS> update_data{};
    std::array<vk::DescriptorSet, NUM_RASTERIZER_SETS> bound_descriptor_sets{};
    std::array<u32, NUM_DYNAMIC_OFFSETS> offsets{};
    std::bitset<NUM_RASTERIZER_SETS> set_dirty{};

    std::array<u64, MAX_SHADER_STAGES> shader_hashes;
    std::array<Shader*, MAX_SHADER_STAGES> current_shaders;
    std::unordered_map<Pica::Shader::Generator::PicaVSConfig, Shader*> programmable_vertex_map;
    std::unordered_map<std::string, Shader> programmable_vertex_cache;
    std::unordered_map<Pica::Shader::Generator::PicaFixedGSConfig, Shader> fixed_geometry_shaders;
    std::unordered_map<Pica::Shader::FSConfig, Shader> fragment_shaders;
    Shader trivial_vertex_shader;
};

} // namespace Vulkan
