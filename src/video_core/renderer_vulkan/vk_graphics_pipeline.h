// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/thread_worker.h"
#include "video_core/pica/regs_pipeline.h"
#include "video_core/pica/regs_rasterizer.h"
#include "video_core/rasterizer_cache/pixel_format.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Common {

struct AsyncHandle {
public:
    AsyncHandle(bool is_done_ = false) : is_done{is_done_} {}

    [[nodiscard]] bool IsDone() noexcept {
        return is_done.load(std::memory_order::relaxed);
    }

    void WaitDone() noexcept {
        std::unique_lock lock{mutex};
        condvar.wait(lock, [this] { return is_done.load(std::memory_order::relaxed); });
    }

    void MarkDone(bool done = true) noexcept {
        std::scoped_lock lock{mutex};
        is_done = done;
        condvar.notify_all();
    }

private:
    std::condition_variable condvar;
    std::mutex mutex;
    std::atomic_bool is_done{false};
};

} // namespace Common

namespace Vulkan {

class Instance;
class RenderManager;

constexpr u32 MAX_SHADER_STAGES = 3;
constexpr u32 MAX_VERTEX_ATTRIBUTES = 16;
constexpr u32 MAX_VERTEX_BINDINGS = 13;

/**
 * The pipeline state is tightly packed with bitfields to reduce
 * the overhead of hashing as much as possible
 */
union RasterizationState {
    u8 value = 0;
    BitField<0, 2, Pica::PipelineRegs::TriangleTopology> topology;
    BitField<4, 2, Pica::RasterizerRegs::CullMode> cull_mode;
};

union DepthStencilState {
    u32 value = 0;
    BitField<0, 1, u32> depth_test_enable;
    BitField<1, 1, u32> depth_write_enable;
    BitField<2, 1, u32> stencil_test_enable;
    BitField<3, 3, Pica::FramebufferRegs::CompareFunc> depth_compare_op;
    BitField<6, 3, Pica::FramebufferRegs::StencilAction> stencil_fail_op;
    BitField<9, 3, Pica::FramebufferRegs::StencilAction> stencil_pass_op;
    BitField<12, 3, Pica::FramebufferRegs::StencilAction> stencil_depth_fail_op;
    BitField<15, 3, Pica::FramebufferRegs::CompareFunc> stencil_compare_op;
};

struct BlendingState {
    u16 blend_enable;
    u16 color_write_mask;
    Pica::FramebufferRegs::LogicOp logic_op;
    union {
        u32 value = 0;
        BitField<0, 4, Pica::FramebufferRegs::BlendFactor> src_color_blend_factor;
        BitField<4, 4, Pica::FramebufferRegs::BlendFactor> dst_color_blend_factor;
        BitField<8, 3, Pica::FramebufferRegs::BlendEquation> color_blend_eq;
        BitField<11, 4, Pica::FramebufferRegs::BlendFactor> src_alpha_blend_factor;
        BitField<15, 4, Pica::FramebufferRegs::BlendFactor> dst_alpha_blend_factor;
        BitField<19, 3, Pica::FramebufferRegs::BlendEquation> alpha_blend_eq;
    };
};

struct DynamicState {
    u32 blend_color = 0;
    u8 stencil_reference;
    u8 stencil_compare_mask;
    u8 stencil_write_mask;

    Common::Rectangle<u32> scissor;
    Common::Rectangle<s32> viewport;

    bool operator==(const DynamicState& other) const noexcept {
        return std::memcmp(this, &other, sizeof(DynamicState)) == 0;
    }
};

union VertexBinding {
    u16 value = 0;
    BitField<0, 4, u16> binding;
    BitField<4, 1, u16> fixed;
    BitField<5, 11, u16> stride;
};

union VertexAttribute {
    u32 value = 0;
    BitField<0, 4, u32> binding;
    BitField<4, 4, u32> location;
    BitField<8, 3, Pica::PipelineRegs::VertexAttributeFormat> type;
    BitField<11, 3, u32> size;
    BitField<14, 11, u32> offset;
};

struct VertexLayout {
    u8 binding_count;
    u8 attribute_count;
    std::array<VertexBinding, MAX_VERTEX_BINDINGS> bindings;
    std::array<VertexAttribute, MAX_VERTEX_ATTRIBUTES> attributes;
};

struct AttachmentInfo {
    VideoCore::PixelFormat color;
    VideoCore::PixelFormat depth;
};

/**
 * Information about a graphics/compute pipeline
 */
struct PipelineInfo {
    BlendingState blending;
    AttachmentInfo attachments;
    RasterizationState rasterization;
    DepthStencilState depth_stencil;
    DynamicState dynamic;
    VertexLayout vertex_layout;

    [[nodiscard]] u64 Hash(const Instance& instance) const;

    [[nodiscard]] bool IsDepthWriteEnabled() const noexcept {
        const bool has_stencil = attachments.depth == VideoCore::PixelFormat::D24S8;
        const bool depth_write =
            depth_stencil.depth_test_enable && depth_stencil.depth_write_enable;
        const bool stencil_write =
            has_stencil && depth_stencil.stencil_test_enable && dynamic.stencil_write_mask != 0;

        return depth_write || stencil_write;
    }
};

struct Shader : public Common::AsyncHandle {
    explicit Shader(const Instance& instance);
    explicit Shader(const Instance& instance, vk::ShaderStageFlagBits stage, std::string code);
    ~Shader();

    [[nodiscard]] vk::ShaderModule Handle() const noexcept {
        return module;
    }

    vk::ShaderModule module;
    vk::Device device;
    std::string program;
};

class GraphicsPipeline : public Common::AsyncHandle {
public:
    explicit GraphicsPipeline(const Instance& instance, RenderManager& render_manager,
                              const PipelineInfo& info, vk::PipelineCache pipeline_cache,
                              vk::PipelineLayout layout, std::array<Shader*, 3> stages,
                              Common::ThreadWorker* worker);
    ~GraphicsPipeline();

    bool TryBuild(bool wait_built);

    bool Build(bool fail_on_compile_required = false);

    [[nodiscard]] vk::Pipeline Handle() const noexcept {
        return *pipeline;
    }

private:
    const Instance& instance;
    RenderManager& render_manager;
    Common::ThreadWorker* worker;

    vk::UniquePipeline pipeline;
    vk::PipelineLayout pipeline_layout;
    vk::PipelineCache pipeline_cache;

    PipelineInfo info;
    std::array<Shader*, 3> stages;
    bool is_pending{};
};

} // namespace Vulkan
