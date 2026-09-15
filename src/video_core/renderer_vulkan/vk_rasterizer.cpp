// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "common/literals.h"
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/microprofile.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/loader/loader.h"
#include "core/memory.h"
#include "video_core/pica/pica_core.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/texture/texture_decode.h"

namespace Vulkan {

namespace {

MICROPROFILE_DEFINE(Vulkan_VS, "Vulkan", "Vertex Shader Setup", MP_RGB(192, 128, 128));
MICROPROFILE_DEFINE(Vulkan_GS, "Vulkan", "Geometry Shader Setup", MP_RGB(128, 192, 128));
MICROPROFILE_DEFINE(Vulkan_Drawing, "Vulkan", "Drawing", MP_RGB(128, 128, 192));

using TriangleTopology = Pica::PipelineRegs::TriangleTopology;
using VideoCore::SurfaceType;

using namespace Common::Literals;
using namespace Pica::Shader::Generator;

constexpr u64 STREAM_BUFFER_SIZE = 64_MiB;
constexpr u64 UNIFORM_BUFFER_SIZE = 8_MiB;
constexpr u64 TEXTURE_BUFFER_SIZE = 2_MiB;

constexpr vk::BufferUsageFlags BUFFER_USAGE =
    vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer;

struct DrawParams {
    u32 vertex_count;
    s32 vertex_offset;
    u32 binding_count;
    std::array<u32, 16> bindings;
    bool is_indexed;
};

[[nodiscard]] u64 TextureBufferSize(const Instance& instance) {
    // Use the smallest texel size from the texel views
    // which corresponds to eR32G32Sfloat
    const u64 max_size = instance.MaxTexelBufferElements() * 8;
    return std::min(max_size, TEXTURE_BUFFER_SIZE);
}

} // Anonymous namespace

RasterizerVulkan::RasterizerVulkan(Memory::MemorySystem& memory, Pica::PicaCore& pica,
                                   VideoCore::CustomTexManager& custom_tex_manager,
                                   VideoCore::RendererBase& renderer,
                                   Frontend::EmuWindow& emu_window, const Instance& instance,
                                   Scheduler& scheduler, RenderManager& renderpass_cache,
                                   DescriptorUpdateQueue& update_queue_, u32 image_count)
    : RasterizerAccelerated{memory, pica}, instance{instance}, scheduler{scheduler},
      renderpass_cache{renderpass_cache}, update_queue{update_queue_},
      pipeline_cache{instance, scheduler, renderpass_cache, update_queue},
      runtime{instance, scheduler, renderpass_cache, update_queue, image_count},
      res_cache{memory, custom_tex_manager, runtime, regs, renderer},
      stream_buffer{instance, scheduler, BUFFER_USAGE, STREAM_BUFFER_SIZE},
      uniform_buffer{instance, scheduler, vk::BufferUsageFlagBits::eUniformBuffer,
                     UNIFORM_BUFFER_SIZE},
      texture_buffer{instance, scheduler, vk::BufferUsageFlagBits::eUniformTexelBuffer,
                     TextureBufferSize(instance)},
      texture_lf_buffer{instance, scheduler, vk::BufferUsageFlagBits::eUniformTexelBuffer,
                        TextureBufferSize(instance)},
      async_shaders{Settings::values.async_shader_compilation.GetValue()} {

    vertex_buffers.fill(stream_buffer.Handle());

    // Query uniform buffer alignment.
    uniform_buffer_alignment = instance.UniformMinAlignment();
    uniform_size_aligned_vs_pica =
        Common::AlignUp<u32>(sizeof(VSPicaUniformData), uniform_buffer_alignment);
    uniform_size_aligned_vs = Common::AlignUp<u32>(sizeof(VSUniformData), uniform_buffer_alignment);
    uniform_size_aligned_fs = Common::AlignUp<u32>(sizeof(FSUniformData), uniform_buffer_alignment);

    // Define vertex layout for software shaders
    MakeSoftwareVertexLayout();
    pipeline_info.state.vertex_layout = software_layout;

    const vk::Device device = instance.GetDevice();
    texture_lf_view = device.createBufferViewUnique({
        .buffer = texture_lf_buffer.Handle(),
        .format = vk::Format::eR32G32Sfloat,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    });
    texture_rg_view = device.createBufferViewUnique({
        .buffer = texture_buffer.Handle(),
        .format = vk::Format::eR32G32Sfloat,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    });
    texture_rgba_view = device.createBufferViewUnique({
        .buffer = texture_buffer.Handle(),
        .format = vk::Format::eR32G32B32A32Sfloat,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    });

    scheduler.RegisterOnSubmit([&renderpass_cache] { renderpass_cache.EndRendering(); });

    // Prepare the static buffer descriptor set.
    const auto buffer_set = pipeline_cache.Acquire(DescriptorHeapType::Buffer);
    update_queue.AddBuffer(buffer_set, 0, uniform_buffer.Handle(), 0, sizeof(VSPicaUniformData));
    update_queue.AddBuffer(buffer_set, 1, uniform_buffer.Handle(), 0, sizeof(VSUniformData));
    update_queue.AddBuffer(buffer_set, 2, uniform_buffer.Handle(), 0, sizeof(FSUniformData));
    update_queue.AddTexelBuffer(buffer_set, 3, *texture_lf_view);
    update_queue.AddTexelBuffer(buffer_set, 4, *texture_rg_view);
    update_queue.AddTexelBuffer(buffer_set, 5, *texture_rgba_view);

    const auto texture_set = pipeline_cache.Acquire(DescriptorHeapType::Texture);
    Surface& null_surface = res_cache.GetSurface(VideoCore::NULL_SURFACE_ID);
    Sampler& null_sampler = res_cache.GetSampler(VideoCore::NULL_SAMPLER_ID);

    const vk::ImageView null_surface_view =
        instance.IsNullDescriptorSupported() ? vk::ImageView{} : null_surface.ImageView();
    const vk::ImageView null_surface_storage_view =
        instance.IsNullDescriptorSupported() ? vk::ImageView{} : null_surface.StorageView();

    // Prepare texture and utility descriptor sets.
    for (u32 i = 0; i < 3; i++) {
        update_queue.AddImageSampler(texture_set, i, 0, null_surface_view, null_sampler.Handle());
    }

    const auto utility_set = pipeline_cache.Acquire(DescriptorHeapType::Utility);
    update_queue.AddStorageImage(utility_set, 0, null_surface_storage_view);
    update_queue.AddImageSampler(utility_set, 1, 0, null_surface_view, null_sampler.Handle());
    update_queue.Flush();
}

RasterizerVulkan::~RasterizerVulkan() = default;

void RasterizerVulkan::TickFrame() {
    scheduler.WaitWorker();
    res_cache.TickFrame();
}

void RasterizerVulkan::LoadDefaultDiskResources(
    const std::atomic_bool& stop_loading, const VideoCore::DiskResourceLoadCallback& callback) {

    u64 program_id;
    if (Core::System::GetInstance().GetAppLoader().ReadProgramId(program_id) !=
        Loader::ResultStatus::Success) {
        program_id = 0;
    }

    if (callback) {
        callback(VideoCore::LoadCallbackStage::Prepare, 0, 0, "");
    }

    pipeline_cache.SetProgramID(program_id);
    pipeline_cache.SetAccurateMul(accurate_mul);
    pipeline_cache.LoadCache(stop_loading, callback);

    if (callback) {
        callback(VideoCore::LoadCallbackStage::Complete, 0, 0, "");
    }
}

void RasterizerVulkan::SyncDrawState() {
    SyncDrawUniforms();

    // SyncCullMode();
    pipeline_info.state.rasterization.cull_mode.Assign(regs.rasterizer.cull_mode);
    // If the framebuffer is flipped, request to also flip vulkan viewport
    const bool is_flipped = regs.framebuffer.framebuffer.IsFlipped();
    pipeline_info.state.rasterization.flip_viewport.Assign(is_flipped);
    // SyncBlendEnabled();
    pipeline_info.state.blending.blend_enable = regs.framebuffer.output_merger.alphablend_enable;
    // SyncBlendFuncs();
    pipeline_info.state.blending.color_blend_eq.Assign(
        regs.framebuffer.output_merger.alpha_blending.blend_equation_rgb);
    pipeline_info.state.blending.alpha_blend_eq.Assign(
        regs.framebuffer.output_merger.alpha_blending.blend_equation_a);
    pipeline_info.state.blending.src_color_blend_factor.Assign(
        regs.framebuffer.output_merger.alpha_blending.factor_source_rgb);
    pipeline_info.state.blending.dst_color_blend_factor.Assign(
        regs.framebuffer.output_merger.alpha_blending.factor_dest_rgb);
    pipeline_info.state.blending.src_alpha_blend_factor.Assign(
        regs.framebuffer.output_merger.alpha_blending.factor_source_a);
    pipeline_info.state.blending.dst_alpha_blend_factor.Assign(
        regs.framebuffer.output_merger.alpha_blending.factor_dest_a);
    // SyncBlendColor();
    pipeline_info.dynamic_info.blend_color = regs.framebuffer.output_merger.blend_const.raw;
    // SyncLogicOp();
    // SyncColorWriteMask();
    pipeline_info.state.blending.logic_op = regs.framebuffer.output_merger.logic_op;

    const u32 color_mask = regs.framebuffer.framebuffer.allow_color_write != 0
                               ? (regs.framebuffer.output_merger.depth_color_mask >> 8) & 0xF
                               : 0;
    pipeline_info.state.blending.color_write_mask = color_mask;

    // SyncStencilTest();
    const auto& stencil_test = regs.framebuffer.output_merger.stencil_test;
    const bool test_enable = stencil_test.enable && regs.framebuffer.framebuffer.depth_format ==
                                                        Pica::FramebufferRegs::DepthFormat::D24S8;

    pipeline_info.state.depth_stencil.stencil_test_enable.Assign(test_enable);
    pipeline_info.state.depth_stencil.stencil_fail_op.Assign(stencil_test.action_stencil_fail);
    pipeline_info.state.depth_stencil.stencil_pass_op.Assign(stencil_test.action_depth_pass);
    pipeline_info.state.depth_stencil.stencil_depth_fail_op.Assign(stencil_test.action_depth_fail);
    pipeline_info.state.depth_stencil.stencil_compare_op.Assign(stencil_test.func);
    pipeline_info.dynamic_info.stencil_reference = stencil_test.reference_value;
    pipeline_info.dynamic_info.stencil_compare_mask = stencil_test.input_mask;
    // SyncStencilWriteMask();
    pipeline_info.dynamic_info.stencil_write_mask =
        (regs.framebuffer.framebuffer.allow_depth_stencil_write != 0)
            ? static_cast<u32>(regs.framebuffer.output_merger.stencil_test.write_mask)
            : 0;
    // SyncDepthTest();
    const bool test_enabled = regs.framebuffer.output_merger.depth_test_enable == 1 ||
                              regs.framebuffer.output_merger.depth_write_enable == 1;
    const auto compare_op = regs.framebuffer.output_merger.depth_test_enable == 1
                                ? regs.framebuffer.output_merger.depth_test_func.Value()
                                : Pica::FramebufferRegs::CompareFunc::Always;

    pipeline_info.state.depth_stencil.depth_test_enable.Assign(test_enabled);
    pipeline_info.state.depth_stencil.depth_compare_op.Assign(compare_op);
    // SyncDepthWriteMask();
    const bool write_enable = (regs.framebuffer.framebuffer.allow_depth_stencil_write != 0 &&
                               regs.framebuffer.output_merger.depth_write_enable);
    pipeline_info.state.depth_stencil.depth_write_enable.Assign(write_enable);
}

void RasterizerVulkan::SetupVertexArray() {
    const auto [vs_input_index_min, vs_input_index_max, vs_input_size] = vertex_info;
    auto [array_ptr, array_offset, invalidate] = stream_buffer.Map(vs_input_size, 16);

    /**
     * The Nintendo 3DS has 12 attribute loaders which are used to tell the GPU
     * how to interpret vertex data. The program firsts sets GPUREG_ATTR_BUF_BASE to the base
     * address containing the vertex array data. The data for each attribute loader (i) can be found
     * by adding GPUREG_ATTR_BUFi_OFFSET to the base address. Attribute loaders can be thought
     * as something analogous to Vulkan bindings. The user can store attributes in separate loaders
     * or interleave them in the same loader.
     **/
    const auto& vertex_attributes = regs.pipeline.vertex_attributes;
    const PAddr base_address = vertex_attributes.GetPhysicalBaseAddress(); // GPUREG_ATTR_BUF_BASE
    const u32 stride_alignment = instance.GetMinVertexStrideAlignment();

    VertexLayout& layout = pipeline_info.state.vertex_layout;
    layout.binding_count = 0;
    layout.attribute_count = 16;
    enable_attributes.fill(false);

    u32 buffer_offset = 0;
    for (const auto& loader : vertex_attributes.attribute_loaders) {
        if (loader.component_count == 0 || loader.byte_count == 0) {
            continue;
        }

        // Analyze the attribute loader by checking which attributes it provides
        u32 offset = 0;
        for (u32 comp = 0; comp < loader.component_count && comp < 12; comp++) {
            const u32 attribute_index = loader.GetComponent(comp);
            if (attribute_index >= 12) {
                // Attribute ids 12, to 15 signify 4, 8, 12 and 16-byte paddings respectively.
                offset = Common::AlignUp(offset, 4);
                offset += (attribute_index - 11) * 4;
                continue;
            }

            const u32 size = vertex_attributes.GetNumElements(attribute_index);
            if (size == 0) {
                continue;
            }

            offset =
                Common::AlignUp(offset, vertex_attributes.GetElementSizeInBytes(attribute_index));

            const u32 input_reg = regs.vs.GetRegisterForAttribute(attribute_index);
            const auto format = vertex_attributes.GetFormat(attribute_index);

            VertexAttribute& attribute = layout.attributes[input_reg];
            attribute.binding.Assign(layout.binding_count);
            attribute.location.Assign(input_reg);
            attribute.offset.Assign(offset);
            attribute.type.Assign(format);
            attribute.size.Assign(size);

            enable_attributes[input_reg] = true;
            offset += vertex_attributes.GetStride(attribute_index);
        }

        const PAddr data_addr =
            base_address + loader.data_offset + (vs_input_index_min * loader.byte_count);
        const u32 vertex_num = vs_input_index_max - vs_input_index_min + 1;
        u32 data_size = loader.byte_count * vertex_num;
        res_cache.FlushRegion(data_addr, data_size);

        const MemoryRef src_ref = memory.GetPhysicalRef(data_addr);
        if (src_ref.GetSize() < data_size) {
            LOG_ERROR(Render_Vulkan,
                      "Vertex buffer size {} exceeds available space {} at address {:#016X}",
                      data_size, src_ref.GetSize(), data_addr);
        }

        const u8* src_ptr = src_ref.GetPtr();
        u8* dst_ptr = array_ptr + buffer_offset;

        // Align stride up if required by Vulkan implementation.
        const u32 aligned_stride =
            Common::AlignUp(static_cast<u32>(loader.byte_count), stride_alignment);
        if (aligned_stride == loader.byte_count) {
            std::memcpy(dst_ptr, src_ptr, data_size);
        } else {
            for (std::size_t vertex = 0; vertex < vertex_num; vertex++) {
                std::memcpy(dst_ptr + vertex * aligned_stride, src_ptr + vertex * loader.byte_count,
                            loader.byte_count);
            }
        }

        // Create the binding associated with this loader
        VertexBinding& binding = layout.bindings[layout.binding_count];
        binding.binding.Assign(layout.binding_count);
        binding.fixed.Assign(0);
        // Will be adjusted on pipeline build, to keep the info transferable.
        binding.byte_count.Assign(loader.byte_count);

        // Keep track of the binding offsets so we can bind the vertex buffer later
        binding_offsets[layout.binding_count++] = static_cast<u32>(array_offset + buffer_offset);
        buffer_offset += Common::AlignUp(aligned_stride * vertex_num, 4);
    }

    stream_buffer.Commit(buffer_offset);

    // Assign the rest of the attributes to the last binding
    SetupFixedAttribs();
}

void RasterizerVulkan::SetupFixedAttribs() {
    const auto& vertex_attributes = regs.pipeline.vertex_attributes;
    VertexLayout& layout = pipeline_info.state.vertex_layout;

    auto [fixed_ptr, fixed_offset, _] = stream_buffer.Map(16 * sizeof(Common::Vec4f), 0);
    binding_offsets[layout.binding_count] = static_cast<u32>(fixed_offset);

    // Reserve the last binding for fixed and default attributes
    // Place the default attrib at offset zero for easy access
    static const Common::Vec4f default_attrib{0.f, 0.f, 0.f, 1.f};
    std::memcpy(fixed_ptr, default_attrib.AsArray(), sizeof(Common::Vec4f));

    // Find all fixed attributes and assign them to the last binding
    u32 offset = sizeof(Common::Vec4f);
    for (std::size_t i = 0; i < 16; i++) {
        if (vertex_attributes.IsDefaultAttribute(i)) {
            const u32 reg = regs.vs.GetRegisterForAttribute(i);
            if (!enable_attributes[reg]) {
                const auto& attr = pica.input_default_attributes[i];
                const std::array data = {attr.x.ToFloat32(), attr.y.ToFloat32(), attr.z.ToFloat32(),
                                         attr.w.ToFloat32()};

                const u32 data_size = sizeof(float) * static_cast<u32>(data.size());
                std::memcpy(fixed_ptr + offset, data.data(), data_size);

                VertexAttribute& attribute = layout.attributes[reg];
                attribute.binding.Assign(layout.binding_count);
                attribute.location.Assign(reg);
                attribute.offset.Assign(offset);
                attribute.type.Assign(Pica::PipelineRegs::VertexAttributeFormat::FLOAT);
                attribute.size.Assign(4);

                offset += data_size;
                enable_attributes[reg] = true;
            }
        }
    }

    // Loop one more time to find unused attributes and assign them to the default one
    // If the attribute is just disabled, shove the default attribute to avoid
    // errors if the shader ever decides to use it.
    for (u32 i = 0; i < 16; i++) {
        if (!enable_attributes[i]) {
            VertexAttribute& attribute = layout.attributes[i];
            attribute.binding.Assign(layout.binding_count);
            attribute.location.Assign(i);
            attribute.offset.Assign(0);
            attribute.type.Assign(Pica::PipelineRegs::VertexAttributeFormat::FLOAT);
            attribute.size.Assign(4);
        }
    }

    // Define the fixed+default binding
    VertexBinding& binding = layout.bindings[layout.binding_count];
    binding.binding.Assign(layout.binding_count++);
    binding.fixed.Assign(1);
    binding.byte_count.Assign(offset);

    stream_buffer.Commit(offset);
}

bool RasterizerVulkan::SetupVertexShader() {
    MICROPROFILE_SCOPE(Vulkan_VS);
    return pipeline_cache.UseProgrammableVertexShader(regs, pica.vs_setup,
                                                      pipeline_info.state.vertex_layout);
}

bool RasterizerVulkan::SetupGeometryShader() {
    MICROPROFILE_SCOPE(Vulkan_GS);

    if (regs.pipeline.use_gs != Pica::PipelineRegs::UseGS::No) {
        LOG_ERROR(Render_Vulkan, "Accelerate draw doesn't support geometry shader");
        return false;
    }

    // Enable the quaternion fix-up geometry-shader only if we are actually doing per-fragment
    // lighting and care about proper quaternions. Otherwise just use standard vertex+fragment
    // shaders. We also don't need a geometry shader if the barycentric extension is supported,
    // but that will be decided later as the GS config needs to be cached anyways.
    if (regs.lighting.disable) {
        pipeline_cache.UseTrivialGeometryShader();
        return true;
    }

    return pipeline_cache.UseFixedGeometryShader(regs);
}

bool RasterizerVulkan::AccelerateDrawBatch(bool is_indexed) {
    if (regs.pipeline.use_gs != Pica::PipelineRegs::UseGS::No) {
        if (regs.pipeline.gs_config.mode != Pica::PipelineRegs::GSMode::Point) {
            return false;
        }
        if (regs.pipeline.triangle_topology != Pica::PipelineRegs::TriangleTopology::Shader) {
            return false;
        }
    }

    pipeline_info.state.rasterization.topology.Assign(regs.pipeline.triangle_topology);
    if (regs.pipeline.triangle_topology == TriangleTopology::Fan &&
        !instance.IsTriangleFanSupported()) {
        LOG_DEBUG(Render_Vulkan,
                  "Skipping accelerated draw with unsupported triangle fan topology");
        return false;
    }

    // Vertex data setup might involve scheduler flushes so perform it
    // early to avoid invalidating our state in the middle of the draw.
    vertex_info = AnalyzeVertexArray(is_indexed, instance.GetMinVertexStrideAlignment());
    if (vertex_info.Invalid()) {
        // Do not draw anything if the vertex array is invalid.
        return true;
    }
    SetupVertexArray();

    if (!SetupVertexShader()) {
        return false;
    }
    if (!SetupGeometryShader()) {
        return false;
    }

    return Draw(true, is_indexed);
}

bool RasterizerVulkan::AccelerateDrawBatchInternal(bool is_indexed) {
    if (is_indexed) {
        SetupIndexArray();
    }

    const bool wait_built = !async_shaders || regs.pipeline.num_vertices <= 6;
    if (!pipeline_cache.BindPipeline(pipeline_info, wait_built)) {
        return true;
    }

    const DrawParams params = {
        .vertex_count = regs.pipeline.num_vertices,
        .vertex_offset = -static_cast<s32>(vertex_info.vs_input_index_min),
        .binding_count = pipeline_info.state.vertex_layout.binding_count,
        .bindings = binding_offsets,
        .is_indexed = is_indexed,
    };

    scheduler.Record([this, params](vk::CommandBuffer cmdbuf) {
        std::array<vk::DeviceSize, 16> offsets;
        std::transform(params.bindings.begin(), params.bindings.end(), offsets.begin(),
                       [](u32 offset) { return static_cast<vk::DeviceSize>(offset); });
        cmdbuf.bindVertexBuffers(0, params.binding_count, vertex_buffers.data(), offsets.data());
        if (params.is_indexed) {
            cmdbuf.drawIndexed(params.vertex_count, 1, 0, params.vertex_offset, 0);
        } else {
            cmdbuf.draw(params.vertex_count, 1, 0, 0);
        }
    });

    return true;
}

void RasterizerVulkan::SetupIndexArray() {
    const bool index_u8 = regs.pipeline.index_array.format == 0;
    const bool native_u8 = index_u8 && instance.IsIndexTypeUint8Supported();
    const u32 index_buffer_size = regs.pipeline.num_vertices * (native_u8 ? 1 : 2);
    const vk::IndexType index_type = native_u8 ? vk::IndexType::eUint8EXT : vk::IndexType::eUint16;

    const u8* index_data =
        memory.GetPhysicalPointer(regs.pipeline.vertex_attributes.GetPhysicalBaseAddress() +
                                  regs.pipeline.index_array.offset);

    auto [index_ptr, index_offset, _] = stream_buffer.Map(index_buffer_size, 2);

    if (index_u8 && !native_u8) {
        u16* index_ptr_u16 = reinterpret_cast<u16*>(index_ptr);
        for (u32 i = 0; i < regs.pipeline.num_vertices; i++) {
            index_ptr_u16[i] = index_data[i];
        }
    } else {
        std::memcpy(index_ptr, index_data, index_buffer_size);
    }

    stream_buffer.Commit(index_buffer_size);

    scheduler.Record(
        [this, index_offset = index_offset, index_type = index_type](vk::CommandBuffer cmdbuf) {
            cmdbuf.bindIndexBuffer(stream_buffer.Handle(), index_offset, index_type);
        });
}

void RasterizerVulkan::DrawTriangles() {
    if (vertex_batch.empty()) {
        return;
    }

    pipeline_info.state.rasterization.topology.Assign(Pica::PipelineRegs::TriangleTopology::List);
    pipeline_info.state.vertex_layout = software_layout;

    pipeline_cache.UseTrivialVertexShader();
    pipeline_cache.UseTrivialGeometryShader();

    Draw(false, false);
}

bool RasterizerVulkan::Draw(bool accelerate, bool is_indexed) {
    MICROPROFILE_SCOPE(Vulkan_Drawing);
    SyncDrawState();

    const bool shadow_rendering = regs.framebuffer.IsShadowRendering();
    const bool has_stencil = regs.framebuffer.HasStencil();

    const bool write_color_fb = shadow_rendering || pipeline_info.GetFinalColorWriteMask(instance);
    const bool write_depth_fb = pipeline_info.IsDepthWriteEnabled();
    const bool using_color_fb =
        regs.framebuffer.framebuffer.GetColorBufferPhysicalAddress() != 0 && write_color_fb;
    const bool using_depth_fb =
        !shadow_rendering && regs.framebuffer.framebuffer.GetDepthBufferPhysicalAddress() != 0 &&
        (write_depth_fb || regs.framebuffer.output_merger.depth_test_enable != 0 ||
         (has_stencil && pipeline_info.state.depth_stencil.stencil_test_enable));

    const auto fb_helper = res_cache.GetFramebufferSurfaces(using_color_fb, using_depth_fb);
    const Framebuffer* framebuffer = fb_helper.Framebuffer();
    if (!framebuffer->Handle()) {
        return true;
    }

    const auto draw_rect = fb_helper.DrawRect();
    if (draw_rect.GetArea() == 0) {
        return true;
    }

    pipeline_info.state.attachments.color = framebuffer->Format(SurfaceType::Color);
    pipeline_info.state.attachments.depth = framebuffer->Format(SurfaceType::Depth);

    // Update scissor uniforms
    const auto [scissor_x1, scissor_y2, scissor_x2, scissor_y1] = fb_helper.Scissor();
    if (fs_data.scissor_x1 != scissor_x1 || fs_data.scissor_x2 != scissor_x2 ||
        fs_data.scissor_y1 != scissor_y1 || fs_data.scissor_y2 != scissor_y2) {

        fs_data.scissor_x1 = scissor_x1;
        fs_data.scissor_x2 = scissor_x2;
        fs_data.scissor_y1 = scissor_y1;
        fs_data.scissor_y2 = scissor_y2;
        fs_data_dirty = true;
    }

    // Sync and bind the texture surfaces
    SyncTextureUnits(framebuffer);
    SyncUtilityTextures(framebuffer);

    // Sync and bind the shader
    pipeline_cache.UseFragmentShader(regs, user_config);

    // Sync the LUTs within the texture buffer
    SyncAndUploadLUTs();
    SyncAndUploadLUTsLF();
    UploadUniforms(accelerate);

    // Begin rendering
    renderpass_cache.BeginRendering(framebuffer, draw_rect);

    // Configure viewport and scissor
    const auto viewport = fb_helper.Viewport();
    pipeline_info.dynamic_info.viewport = Common::Rectangle<s32>{
        viewport.x,
        viewport.y,
        viewport.x + viewport.width,
        viewport.y + viewport.height,
    };
    pipeline_info.dynamic_info.scissor = draw_rect;

    // Draw the vertex batch
    bool succeeded = true;
    if (accelerate) {
        succeeded = AccelerateDrawBatchInternal(is_indexed);
    } else {
        pipeline_cache.BindPipeline(pipeline_info, true);

        const u32 vertex_count = static_cast<u32>(vertex_batch.size());
        const u32 vertex_size = vertex_count * sizeof(HardwareVertex);
        const auto [buffer, offset, _] = stream_buffer.Map(vertex_size, sizeof(HardwareVertex));

        std::memcpy(buffer, vertex_batch.data(), vertex_size);
        stream_buffer.Commit(vertex_size);

        scheduler.Record([this, offset = offset, vertex_count](vk::CommandBuffer cmdbuf) {
            cmdbuf.bindVertexBuffers(0, stream_buffer.Handle(), offset);
            cmdbuf.draw(vertex_count, 1, 0, 0);
        });
    }

    vertex_batch.clear();
    return succeeded;
}

void RasterizerVulkan::SyncTextureUnits(const Framebuffer* framebuffer) {
    using TextureType = Pica::TexturingRegs::TextureConfig::TextureType;

    const auto pica_textures = regs.texturing.GetTextures();
    const bool use_cube_heap =
        pica_textures[0].enabled && pica_textures[0].config.type == TextureType::ShadowCube;
    const auto texture_set = pipeline_cache.Acquire(use_cube_heap ? DescriptorHeapType::Texture
                                                                  : DescriptorHeapType::Texture);

    for (u32 texture_index = 0; texture_index < pica_textures.size(); ++texture_index) {
        const auto& texture = pica_textures[texture_index];

        // If the texture unit is disabled bind a null surface to it
        if (!texture.enabled) {
            switch (texture.config.type.Value()) {
            case TextureType::TextureCube:
            case TextureType::ShadowCube: {
                const Sampler& null_sampler = res_cache.GetSampler(VideoCore::NULL_SURFACE_CUBE_ID);
                if (instance.IsNullDescriptorSupported()) {
                    update_queue.AddImageSampler(texture_set, texture_index, 0, vk::ImageView{},
                                                 null_sampler.Handle());
                } else {
                    Surface& null_surface = res_cache.GetSurface(VideoCore::NULL_SURFACE_CUBE_ID);
                    update_queue.AddImageSampler(texture_set, texture_index, 0,
                                                 null_surface.ImageView(), null_sampler.Handle());
                }
                break;
            }
            default: {
                const Sampler& null_sampler = res_cache.GetSampler(VideoCore::NULL_SURFACE_ID);
                if (instance.IsNullDescriptorSupported()) {
                    update_queue.AddImageSampler(texture_set, texture_index, 0, vk::ImageView{},
                                                 null_sampler.Handle());
                } else {
                    Surface& null_surface = res_cache.GetSurface(VideoCore::NULL_SURFACE_ID);
                    update_queue.AddImageSampler(texture_set, texture_index, 0,
                                                 null_surface.ImageView(), null_sampler.Handle());
                }
                break;
            }
            }
            continue;
        }

        // Handle special tex0 configurations
        if (texture_index == 0) {
            switch (texture.config.type.Value()) {
            case TextureType::Shadow2D: {
                Surface& surface = res_cache.GetTextureSurface(texture);
                Sampler& sampler = res_cache.GetSampler(texture.config);
                surface.flags |= VideoCore::SurfaceFlagBits::ShadowSource;
                update_queue.AddImageSampler(texture_set, texture_index, 0, surface.StorageView(),
                                             sampler.Handle());
                continue;
            }
            case TextureType::ShadowCube: {
                BindShadowCube(texture, texture_set);
                continue;
            }
            case TextureType::TextureCube: {
                BindTextureCube(texture, texture_set);
                continue;
            }
            default:
                break;
            }
        }

        // Bind the texture provided by the rasterizer cache
        Surface& surface = res_cache.GetTextureSurface(texture);
        Sampler& sampler = res_cache.GetSampler(texture.config);
        const vk::ImageView color_view = framebuffer->ImageView(SurfaceType::Color);
        const bool is_feedback_loop = color_view == surface.FramebufferView();
        const vk::ImageView texture_view =
            is_feedback_loop ? surface.CopyImageView() : surface.ImageView();
        update_queue.AddImageSampler(texture_set, texture_index, 0, texture_view, sampler.Handle());
    }
}

void RasterizerVulkan::SyncUtilityTextures(const Framebuffer* framebuffer) {
    const bool shadow_writing = regs.framebuffer.IsShadowRendering();
    bool shadow_reading = regs.lighting.config0.enable_shadow;
    // Ensure the shadow-texture slot is actually enabled
    if (shadow_reading) {
        const u32 shadow_texture_unit = regs.lighting.config0.shadow_selector.Value();
        const auto shadow_texture = regs.texturing.GetTextures()[shadow_texture_unit];
        shadow_reading &= shadow_texture.enabled;
    }

    const auto utility_set = pipeline_cache.Acquire(DescriptorHeapType::Utility);

    // Reading and writing are mutually exclusive
    assert(!(shadow_writing && shadow_reading));

    if (shadow_writing) {
        update_queue.AddStorageImage(utility_set, 0, framebuffer->ImageView(SurfaceType::Color));
    } else if (shadow_reading) {
        const u32 shadow_texture_unit = regs.lighting.config0.shadow_selector.Value();
        const auto shadow_texture = regs.texturing.GetTextures()[shadow_texture_unit];
        Surface& shadow_surface = res_cache.GetTextureSurface(shadow_texture);
        update_queue.AddStorageImage(utility_set, 0, shadow_surface.StorageView());
    } else {
        if (instance.IsNullDescriptorSupported()) {
            update_queue.AddStorageImage(utility_set, 0, vk::ImageView{});
        } else {
            Surface& null_surface = res_cache.GetSurface(VideoCore::NULL_SURFACE_ID);
            update_queue.AddStorageImage(utility_set, 0, null_surface.StorageView());
        }
    }
}

void RasterizerVulkan::BindShadowCube(const Pica::TexturingRegs::FullTextureConfig& texture,
                                      vk::DescriptorSet texture_set) {
    using CubeFace = Pica::TexturingRegs::CubeFace;
    auto info = Pica::Texture::TextureInfo::FromPicaRegister(texture.config, texture.format);
    constexpr std::array faces = {
        CubeFace::PositiveX, CubeFace::NegativeX, CubeFace::PositiveY,
        CubeFace::NegativeY, CubeFace::PositiveZ, CubeFace::NegativeZ,
    };

    Sampler& sampler = res_cache.GetSampler(texture.config);

    for (CubeFace face : faces) {
        const u32 binding = static_cast<u32>(face);
        info.physical_address = regs.texturing.GetCubePhysicalAddress(face);

        const VideoCore::SurfaceId surface_id = res_cache.GetTextureSurface(info);
        Surface& surface = res_cache.GetSurface(surface_id);
        surface.flags |= VideoCore::SurfaceFlagBits::ShadowSource;
        update_queue.AddImageSampler(texture_set, 0, binding, surface.StorageView(),
                                     sampler.Handle());
    }
}

void RasterizerVulkan::BindTextureCube(const Pica::TexturingRegs::FullTextureConfig& texture,
                                       vk::DescriptorSet texture_set) {
    using CubeFace = Pica::TexturingRegs::CubeFace;
    const VideoCore::TextureCubeConfig config = {
        .px = regs.texturing.GetCubePhysicalAddress(CubeFace::PositiveX),
        .nx = regs.texturing.GetCubePhysicalAddress(CubeFace::NegativeX),
        .py = regs.texturing.GetCubePhysicalAddress(CubeFace::PositiveY),
        .ny = regs.texturing.GetCubePhysicalAddress(CubeFace::NegativeY),
        .pz = regs.texturing.GetCubePhysicalAddress(CubeFace::PositiveZ),
        .nz = regs.texturing.GetCubePhysicalAddress(CubeFace::NegativeZ),
        .width = texture.config.width,
        .levels = texture.config.lod.max_level + 1,
        .format = texture.format,
    };

    Surface& surface = res_cache.GetTextureCube(config);
    Sampler& sampler = res_cache.GetSampler(texture.config);
    update_queue.AddImageSampler(texture_set, 0, 0, surface.ImageView(), sampler.Handle());
}

void RasterizerVulkan::FlushAll() {
    res_cache.FlushAll();
}

void RasterizerVulkan::FlushRegion(PAddr addr, u32 size) {
    res_cache.FlushRegion(addr, size);
}

void RasterizerVulkan::InvalidateRegion(PAddr addr, u32 size) {
    res_cache.InvalidateRegion(addr, size);
}

void RasterizerVulkan::FlushAndInvalidateRegion(PAddr addr, u32 size) {
    res_cache.FlushRegion(addr, size);
    res_cache.InvalidateRegion(addr, size);
}

void RasterizerVulkan::ClearAll(bool flush) {
    res_cache.ClearAll(flush);
}

bool RasterizerVulkan::AccelerateDisplayTransfer(const Pica::DisplayTransferConfig& config) {
    return res_cache.AccelerateDisplayTransfer(config);
}

bool RasterizerVulkan::AccelerateTextureCopy(const Pica::DisplayTransferConfig& config) {
    return res_cache.AccelerateTextureCopy(config);
}

bool RasterizerVulkan::AccelerateFill(const Pica::MemoryFillConfig& config) {
    return res_cache.AccelerateFill(config);
}

bool RasterizerVulkan::AccelerateDisplay(const Pica::FramebufferConfig& config,
                                         PAddr framebuffer_addr, u32 pixel_stride,
                                         ScreenInfo& screen_info) {
    if (framebuffer_addr == 0) [[unlikely]] {
        return false;
    }

    VideoCore::SurfaceParams src_params;
    src_params.addr = framebuffer_addr;
    src_params.width = std::min(config.width.Value(), pixel_stride);
    src_params.height = config.height;
    src_params.stride = pixel_stride;
    src_params.is_tiled = false;
    src_params.pixel_format = VideoCore::PixelFormatFromGPUPixelFormat(config.color_format);
    src_params.UpdateParams();

    const auto [src_surface_id, src_rect] =
        res_cache.GetSurfaceSubRect(src_params, VideoCore::ScaleMatch::Ignore, true);

    if (!src_surface_id) {
        return false;
    }

    Surface& src_surface = res_cache.GetSurface(src_surface_id);
    const u32 scaled_width = src_surface.GetScaledWidth();
    const u32 scaled_height = src_surface.GetScaledHeight();

    screen_info.texcoords = Common::Rectangle<f32>(
        (float)src_rect.bottom / (float)scaled_height, (float)src_rect.left / (float)scaled_width,
        (float)src_rect.top / (float)scaled_height, (float)src_rect.right / (float)scaled_width);

    screen_info.image_view = src_surface.ImageView();

    return true;
}

void RasterizerVulkan::MakeSoftwareVertexLayout() {
    constexpr std::array sizes = {4, 4, 2, 2, 2, 1, 4, 3};

    software_layout = VertexLayout{
        .binding_count = 1,
        .attribute_count = 8,
    };

    for (u32 i = 0; i < software_layout.binding_count; i++) {
        VertexBinding& binding = software_layout.bindings[i];
        binding.binding.Assign(i);
        binding.fixed.Assign(0);
        binding.byte_count.Assign(sizeof(HardwareVertex));
    }

    u32 offset = 0;
    for (u32 i = 0; i < 8; i++) {
        VertexAttribute& attribute = software_layout.attributes[i];
        attribute.binding.Assign(0);
        attribute.location.Assign(i);
        attribute.offset.Assign(offset);
        attribute.type.Assign(Pica::PipelineRegs::VertexAttributeFormat::FLOAT);
        attribute.size.Assign(sizes[i]);
        offset += sizes[i] * sizeof(float);
    }
}

void RasterizerVulkan::SyncAndUploadLUTsLF() {
    constexpr std::size_t max_size =
        sizeof(Common::Vec2f) * 256 * Pica::LightingRegs::NumLightingSampler +
        sizeof(Common::Vec2f) * 128; // fog

    if (!pica.lighting.lut_dirty && !pica.fog.lut_dirty) {
        return;
    }

    std::size_t bytes_used = 0;
    auto [buffer, offset, invalidate] = texture_lf_buffer.Map(max_size, sizeof(Common::Vec4f));

    if (invalidate) {
        pica.lighting.lut_dirty = pica.lighting.LutAllDirty;
        pica.fog.lut_dirty = true;
    }

    // Sync the lighting luts
    while (pica.lighting.lut_dirty) {
        u32 index = std::countr_zero(pica.lighting.lut_dirty);
        pica.lighting.lut_dirty &= ~(1 << index);

        Common::Vec2f* new_data = reinterpret_cast<Common::Vec2f*>(buffer + bytes_used);
        const auto& source_lut = pica.lighting.luts[index];
        for (u32 i = 0; i < source_lut.size(); i++) {
            new_data[i] = {source_lut[i].ToFloat(), source_lut[i].DiffToFloat()};
        }
        fs_data.lighting_lut_offset[index / 4][index % 4] =
            static_cast<int>((offset + bytes_used) / sizeof(Common::Vec2f));
        fs_data_dirty = true;
        bytes_used += source_lut.size() * sizeof(Common::Vec2f);
    }

    // Sync the fog lut
    if (pica.fog.lut_dirty) {
        Common::Vec2f* new_data = reinterpret_cast<Common::Vec2f*>(buffer + bytes_used);
        for (u32 i = 0; i < pica.fog.lut.size(); i++) {
            new_data[i] = {pica.fog.lut[i].ToFloat(), pica.fog.lut[i].DiffToFloat()};
        }
        fs_data.fog_lut_offset = static_cast<int>((offset + bytes_used) / sizeof(Common::Vec2f));
        fs_data_dirty = true;
        bytes_used += pica.fog.lut.size() * sizeof(Common::Vec2f);
        pica.fog.lut_dirty = false;
    }

    texture_lf_buffer.Commit(static_cast<u32>(bytes_used));
}

void RasterizerVulkan::SyncAndUploadLUTs() {
    const auto& proctex = pica.proctex;
    constexpr std::size_t max_size =
        sizeof(Common::Vec2f) * 128 * 3 + // proctex: noise + color + alpha
        sizeof(Common::Vec4f) * 256 +     // proctex
        sizeof(Common::Vec4f) * 256;      // proctex diff

    if (!pica.proctex.lut_dirty) {
        return;
    }

    std::size_t bytes_used = 0;
    auto [buffer, offset, invalidate] = texture_buffer.Map(max_size, sizeof(Common::Vec4f));

    if (invalidate) {
        pica.proctex.table_dirty = pica.proctex.TableAllDirty;
    }

    // helper function for SyncProcTexNoiseLUT/ColorMap/AlphaMap
    const auto sync_proctex_value_lut =
        [&](const std::array<Pica::PicaCore::ProcTex::ValueEntry, 128>& lut, int& lut_offset) {
            Common::Vec2f* new_data = reinterpret_cast<Common::Vec2f*>(buffer + bytes_used);
            for (u32 i = 0; i < lut.size(); i++) {
                new_data[i] = {lut[i].ToFloat(), lut[i].DiffToFloat()};
            }
            lut_offset = static_cast<int>((offset + bytes_used) / sizeof(Common::Vec2f));
            fs_data_dirty = true;
            bytes_used += lut.size() * sizeof(Common::Vec2f);
        };

    // Sync the proctex noise lut
    if (pica.proctex.noise_lut_dirty) {
        sync_proctex_value_lut(proctex.noise_table, fs_data.proctex_noise_lut_offset);
    }

    // Sync the proctex color map
    if (pica.proctex.color_map_dirty) {
        sync_proctex_value_lut(proctex.color_map_table, fs_data.proctex_color_map_offset);
    }

    // Sync the proctex alpha map
    if (pica.proctex.alpha_map_dirty) {
        sync_proctex_value_lut(proctex.alpha_map_table, fs_data.proctex_alpha_map_offset);
    }

    // Sync the proctex lut
    if (pica.proctex.lut_dirty) {
        Common::Vec4f* new_data = reinterpret_cast<Common::Vec4f*>(buffer + bytes_used);
        for (u32 i = 0; i < proctex.color_table.size(); i++) {
            new_data[i] = proctex.color_table[i].ToVector() / 255.0f;
        }
        fs_data.proctex_lut_offset =
            static_cast<int>((offset + bytes_used) / sizeof(Common::Vec4f));
        fs_data_dirty = true;
        bytes_used += proctex.color_table.size() * sizeof(Common::Vec4f);
    }

    // Sync the proctex difference lut
    if (pica.proctex.diff_lut_dirty) {
        Common::Vec4f* new_data = reinterpret_cast<Common::Vec4f*>(buffer + bytes_used);
        for (u32 i = 0; i < proctex.color_diff_table.size(); i++) {
            new_data[i] = proctex.color_diff_table[i].ToVector() / 255.0f;
        }
        fs_data.proctex_diff_lut_offset =
            static_cast<int>((offset + bytes_used) / sizeof(Common::Vec4f));
        fs_data_dirty = true;
        bytes_used += proctex.color_diff_table.size() * sizeof(Common::Vec4f);
    }

    pica.proctex.table_dirty = 0;

    texture_buffer.Commit(static_cast<u32>(bytes_used));
}

void RasterizerVulkan::UploadUniforms(bool accelerate_draw) {
    const bool sync_vs_pica = accelerate_draw && pica.vs_setup.uniforms_dirty;
    if (!sync_vs_pica && !vs_data_dirty && !fs_data_dirty) {
        return;
    }

    const u32 uniform_size =
        uniform_size_aligned_vs_pica + uniform_size_aligned_vs + uniform_size_aligned_fs;
    auto [uniforms, offset, invalidate] =
        uniform_buffer.Map(uniform_size, uniform_buffer_alignment);

    u32 used_bytes = 0;

    if (vs_data_dirty || invalidate) {
        std::memcpy(uniforms + used_bytes, &vs_data, sizeof(vs_data));
        pipeline_cache.UpdateRange(1, offset + used_bytes);
        vs_data_dirty = false;
        used_bytes += uniform_size_aligned_vs;
    }

    if (fs_data_dirty || invalidate) {
        std::memcpy(uniforms + used_bytes, &fs_data, sizeof(fs_data));
        pipeline_cache.UpdateRange(2, offset + used_bytes);
        fs_data_dirty = false;
        used_bytes += uniform_size_aligned_fs;
    }

    if (sync_vs_pica || invalidate) {
        VSPicaUniformData vs_uniforms;
        vs_uniforms.SetFromRegs(pica.vs_setup);
        std::memcpy(uniforms + used_bytes, &vs_uniforms, sizeof(vs_uniforms));
        pipeline_cache.UpdateRange(0, offset + used_bytes);
        pica.vs_setup.uniforms_dirty = false;
        used_bytes += uniform_size_aligned_vs_pica;
    }

    uniform_buffer.Commit(used_bytes);
}

void RasterizerVulkan::SwitchDiskResources(u64 title_id) {
    std::atomic_bool stop_loading = false;

    if (switch_disk_resources_callback) {
        switch_disk_resources_callback(VideoCore::LoadCallbackStage::Prepare, 0, 0, "");
    }

    pipeline_cache.SetAccurateMul(accurate_mul);
    pipeline_cache.SwitchCache(title_id, stop_loading, switch_disk_resources_callback);

    if (switch_disk_resources_callback) {
        switch_disk_resources_callback(VideoCore::LoadCallbackStage::Complete, 0, 0, "");
    }
}

} // namespace Vulkan
