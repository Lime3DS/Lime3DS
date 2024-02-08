// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/microprofile.h"
#include "common/settings.h"
#include "common/thread.h"
#include "core/frontend/emu_window.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_platform.h"
#include "video_core/renderer_vulkan/vk_present_window.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"
#include "vk_platform.h"

#include <vk_mem_alloc.h>

MICROPROFILE_DEFINE(Vulkan_WaitPresent, "Vulkan", "Wait For Present", MP_RGB(128, 128, 128));

namespace Vulkan {

namespace {

bool CanBlitToSwapchain(const vk::PhysicalDevice& physical_device, vk::Format format) {
    const vk::FormatProperties props{physical_device.getFormatProperties(format)};
    return static_cast<bool>(props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst);
}

[[nodiscard]] vk::ImageSubresourceLayers MakeImageSubresourceLayers() {
    return vk::ImageSubresourceLayers{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
}

[[nodiscard]] vk::ImageBlit MakeImageBlit(s32 frame_width, s32 frame_height, s32 swapchain_width,
                                          s32 swapchain_height) {
    return vk::ImageBlit{
        .srcSubresource = MakeImageSubresourceLayers(),
        .srcOffsets =
            std::array{
                vk::Offset3D{
                    .x = 0,
                    .y = 0,
                    .z = 0,
                },
                vk::Offset3D{
                    .x = frame_width,
                    .y = frame_height,
                    .z = 1,
                },
            },
        .dstSubresource = MakeImageSubresourceLayers(),
        .dstOffsets =
            std::array{
                vk::Offset3D{
                    .x = 0,
                    .y = 0,
                    .z = 0,
                },
                vk::Offset3D{
                    .x = swapchain_width,
                    .y = swapchain_height,
                    .z = 1,
                },
            },
    };
}

[[nodiscard]] vk::ImageCopy MakeImageCopy(u32 frame_width, u32 frame_height, u32 swapchain_width,
                                          u32 swapchain_height) {
    return vk::ImageCopy{
        .srcSubresource = MakeImageSubresourceLayers(),
        .srcOffset =
            vk::Offset3D{
                .x = 0,
                .y = 0,
                .z = 0,
            },
        .dstSubresource = MakeImageSubresourceLayers(),
        .dstOffset =
            vk::Offset3D{
                .x = 0,
                .y = 0,
                .z = 0,
            },
        .extent =
            vk::Extent3D{
                .width = std::min(frame_width, swapchain_width),
                .height = std::min(frame_height, swapchain_height),
                .depth = 1,
            },
    };
}

} // Anonymous namespace

PresentWindow::PresentWindow(Frontend::EmuWindow& emu_window_, const Instance& instance_,
                             Scheduler& scheduler_)
    : emu_window{emu_window_}, instance{instance_}, scheduler{scheduler_},
      surface{CreateSurface(instance.GetInstance(), emu_window)},
      next_surface{surface}, swapchain{instance, emu_window.GetFramebufferLayout().width,
                                       emu_window.GetFramebufferLayout().height, surface},
      graphics_queue{instance.GetGraphicsQueue()}, present_renderpass{CreateRenderpass()},
      vsync_enabled{Settings::values.use_vsync_new.GetValue()},
      blit_supported{
          CanBlitToSwapchain(instance.GetPhysicalDevice(), swapchain.GetSurfaceFormat().format)},
      use_present_thread{Settings::values.async_presentation.GetValue()},
      last_render_surface{emu_window.GetWindowInfo().render_surface} {

    const u32 num_images = swapchain.GetImageCount();
    const vk::Device device = instance.GetDevice();

    const vk::CommandPoolCreateInfo pool_info = {
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
                 vk::CommandPoolCreateFlagBits::eTransient,
        .queueFamilyIndex = instance.GetGraphicsQueueFamilyIndex(),
    };
    command_pool = device.createCommandPool(pool_info);

    const vk::CommandBufferAllocateInfo alloc_info = {
        .commandPool = command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = num_images,
    };
    const std::vector command_buffers = device.allocateCommandBuffers(alloc_info);

    swap_chain.resize(num_images);
    for (u32 i = 0; i < num_images; i++) {
        Frame& frame = swap_chain[i];
        frame.cmdbuf = command_buffers[i];
        frame.render_ready = device.createSemaphore({});
        frame.present_done = device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled});
        free_queue.push(&frame);
    }

    if (instance.HasDebuggingToolAttached()) {
        for (u32 i = 0; i < num_images; ++i) {
            SetObjectName(device, swap_chain[i].cmdbuf, "Swapchain Command Buffer {}", i);
            SetObjectName(device, swap_chain[i].render_ready,
                          "Swapchain Semaphore: render_ready {}", i);
            SetObjectName(device, swap_chain[i].present_done, "Swapchain Fence: present_done {}",
                          i);
        }
    }

    if (use_present_thread) {
        present_thread = std::jthread([this](std::stop_token token) { PresentThread(token); });
    }
}

PresentWindow::~PresentWindow() {
    scheduler.Finish();
    const vk::Device device = instance.GetDevice();
    device.destroyCommandPool(command_pool);
    device.destroyRenderPass(present_renderpass);
    for (auto& frame : swap_chain) {
        device.destroyImageView(frame.image_view);
        device.destroyFramebuffer(frame.framebuffer);
        device.destroySemaphore(frame.render_ready);
        device.destroyFence(frame.present_done);
        vmaDestroyImage(instance.GetAllocator(), frame.image, frame.allocation);
    }
}

void PresentWindow::RecreateFrame(Frame* frame, u32 width, u32 height) {
    vk::Device device = instance.GetDevice();
    if (frame->framebuffer) {
        device.destroyFramebuffer(frame->framebuffer);
    }
    if (frame->image_view) {
        device.destroyImageView(frame->image_view);
    }
    if (frame->image) {
        vmaDestroyImage(instance.GetAllocator(), frame->image, frame->allocation);
    }

    const vk::Format format = swapchain.GetSurfaceFormat().format;
    const vk::ImageCreateInfo image_info = {
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
    };

    const VmaAllocationCreateInfo alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = 0,
        .preferredFlags = 0,
        .pool = VK_NULL_HANDLE,
        .pUserData = nullptr,
    };

    VkImage unsafe_image{};
    VkImageCreateInfo unsafe_image_info = static_cast<VkImageCreateInfo>(image_info);

    VkResult result = vmaCreateImage(instance.GetAllocator(), &unsafe_image_info, &alloc_info,
                                     &unsafe_image, &frame->allocation, nullptr);
    if (result != VK_SUCCESS) [[unlikely]] {
        LOG_CRITICAL(Render_Vulkan, "Failed allocating texture with error {}", result);
        UNREACHABLE();
    }
    frame->image = vk::Image{unsafe_image};

    const vk::ImageViewCreateInfo view_info = {
        .image = frame->image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    frame->image_view = device.createImageView(view_info);

    const vk::FramebufferCreateInfo framebuffer_info = {
        .renderPass = present_renderpass,
        .attachmentCount = 1,
        .pAttachments = &frame->image_view,
        .width = width,
        .height = height,
        .layers = 1,
    };
    frame->framebuffer = instance.GetDevice().createFramebuffer(framebuffer_info);

    frame->width = width;
    frame->height = height;
}

Frame* PresentWindow::GetRenderFrame() {
    MICROPROFILE_SCOPE(Vulkan_WaitPresent);

    // Wait for free presentation frames
    std::unique_lock lock{free_mutex};
    free_cv.wait(lock, [this] { return !free_queue.empty(); });

    // Take the frame from the queue
    Frame* frame = free_queue.front();
    free_queue.pop();

    vk::Device device = instance.GetDevice();
    vk::Result result{};

    const auto wait = [&]() {
        result = device.waitForFences(frame->present_done, false, std::numeric_limits<u64>::max());
        return result;
    };

    // Wait for the presentation to be finished so all frame resources are free
    while (wait() != vk::Result::eSuccess) {
        // Retry if the waiting times out
        if (result == vk::Result::eTimeout) {
            continue;
        }

        // eErrorInitializationFailed occurs on Mali GPU drivers due to them
        // using the ppoll() syscall which isn't correctly restarted after a signal,
        // we need to manually retry waiting in that case
        if (result == vk::Result::eErrorInitializationFailed) {
            continue;
        }
    }

    device.resetFences(frame->present_done);
    return frame;
}

void PresentWindow::Present(Frame* frame) {
    if (!use_present_thread) {
        scheduler.WaitWorker();
        CopyToSwapchain(frame);
        free_queue.push(frame);
        return;
    }

    scheduler.Record([this, frame](vk::CommandBuffer) {
        std::unique_lock lock{queue_mutex};
        present_queue.push(frame);
        frame_cv.notify_one();
    });
}

void PresentWindow::WaitPresent() {
    if (!use_present_thread) {
        return;
    }

    // Wait for the present queue to be empty
    {
        std::unique_lock queue_lock{queue_mutex};
        frame_cv.wait(queue_lock, [this] { return present_queue.empty(); });
    }

    // The above condition will be satisfied when the last frame is taken from the queue.
    // To ensure that frame has been presented as well take hold of the swapchain
    // mutex.
    std::scoped_lock swapchain_lock{swapchain_mutex};
}

void PresentWindow::PresentThread(std::stop_token token) {
    Common::SetCurrentThreadName("VulkanPresent");
    while (!token.stop_requested()) {
        std::unique_lock lock{queue_mutex};

        // Wait for presentation frames
        Common::CondvarWait(frame_cv, lock, token, [this] { return !present_queue.empty(); });
        if (token.stop_requested()) {
            return;
        }

        // Take the frame and notify anyone waiting
        Frame* frame = present_queue.front();
        present_queue.pop();
        frame_cv.notify_one();

        // By exchanging the lock ownership we take the swapchain lock
        // before the queue lock goes out of scope. This way the swapchain
        // lock in WaitPresent is guaranteed to occur after here.
        std::exchange(lock, std::unique_lock{swapchain_mutex});

        CopyToSwapchain(frame);

        // Free the frame for reuse
        std::scoped_lock fl{free_mutex};
        free_queue.push(frame);
        free_cv.notify_one();
    }
}

void PresentWindow::NotifySurfaceChanged() {
#ifdef ANDROID
    std::scoped_lock lock{recreate_surface_mutex};
    next_surface = CreateSurface(instance.GetInstance(), emu_window);
    recreate_surface_cv.notify_one();
#endif
}

void PresentWindow::CopyToSwapchain(Frame* frame) {
    const auto recreate_swapchain = [&] {
#ifdef ANDROID
        {
            std::unique_lock lock{recreate_surface_mutex};
            recreate_surface_cv.wait(lock, [this]() { return surface != next_surface; });
            surface = next_surface;
        }
#endif
        std::scoped_lock submit_lock{scheduler.submit_mutex};
        graphics_queue.waitIdle();
        swapchain.Create(frame->width, frame->height, surface);
    };

#ifndef ANDROID
    const bool use_vsync = Settings::values.use_vsync_new.GetValue();
    const bool size_changed =
        swapchain.GetWidth() != frame->width || swapchain.GetHeight() != frame->height;
    const bool vsync_changed = vsync_enabled != use_vsync;
    if (vsync_changed || size_changed) [[unlikely]] {
        vsync_enabled = use_vsync;
        recreate_swapchain();
    }
#endif

    while (!swapchain.AcquireNextImage()) {
        recreate_swapchain();
    }

    const vk::Image swapchain_image = swapchain.Image();

    const vk::CommandBufferBeginInfo begin_info = {
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    };
    const vk::CommandBuffer cmdbuf = frame->cmdbuf;
    cmdbuf.begin(begin_info);

    const vk::Extent2D extent = swapchain.GetExtent();
    const std::array pre_barriers{
        vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eTransferDstOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapchain_image,
            .subresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        },
        vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .dstAccessMask = vk::AccessFlagBits::eTransferRead,
            .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
            .newLayout = vk::ImageLayout::eTransferSrcOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = frame->image,
            .subresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        },
    };
    const vk::ImageMemoryBarrier post_barrier{
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = vk::AccessFlagBits::eMemoryRead,
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::ePresentSrcKHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = swapchain_image,
        .subresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
    };

    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                           vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion,
                           {}, {}, pre_barriers);

    if (blit_supported) {
        cmdbuf.blitImage(frame->image, vk::ImageLayout::eTransferSrcOptimal, swapchain_image,
                         vk::ImageLayout::eTransferDstOptimal,
                         MakeImageBlit(frame->width, frame->height, extent.width, extent.height),
                         vk::Filter::eLinear);
    } else {
        cmdbuf.copyImage(frame->image, vk::ImageLayout::eTransferSrcOptimal, swapchain_image,
                         vk::ImageLayout::eTransferDstOptimal,
                         MakeImageCopy(frame->width, frame->height, extent.width, extent.height));
    }

    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                           vk::PipelineStageFlagBits::eAllCommands,
                           vk::DependencyFlagBits::eByRegion, {}, {}, post_barrier);

    cmdbuf.end();

    static constexpr std::array<vk::PipelineStageFlags, 2> wait_stage_masks = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eAllGraphics,
    };

    const vk::Semaphore present_ready = swapchain.GetPresentReadySemaphore();
    const vk::Semaphore image_acquired = swapchain.GetImageAcquiredSemaphore();
    const std::array wait_semaphores = {image_acquired, frame->render_ready};

    vk::SubmitInfo submit_info = {
        .waitSemaphoreCount = static_cast<u32>(wait_semaphores.size()),
        .pWaitSemaphores = wait_semaphores.data(),
        .pWaitDstStageMask = wait_stage_masks.data(),
        .commandBufferCount = 1u,
        .pCommandBuffers = &cmdbuf,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &present_ready,
    };

    std::scoped_lock submit_lock{scheduler.submit_mutex, recreate_surface_mutex};

    try {
        graphics_queue.submit(submit_info, frame->present_done);
    } catch (vk::DeviceLostError& err) {
        LOG_CRITICAL(Render_Vulkan, "Device lost during present submit: {}", err.what());
        UNREACHABLE();
    }

    swapchain.Present();
}

vk::RenderPass PresentWindow::CreateRenderpass() {
    const vk::AttachmentReference color_ref = {
        .attachment = 0,
        .layout = vk::ImageLayout::eGeneral,
    };

    const vk::SubpassDescription subpass = {
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = 1u,
        .pColorAttachments = &color_ref,
        .pResolveAttachments = 0,
        .pDepthStencilAttachment = nullptr,
    };

    const vk::AttachmentDescription color_attachment = {
        .format = swapchain.GetSurfaceFormat().format,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eTransferSrcOptimal,
    };

    const vk::RenderPassCreateInfo renderpass_info = {
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 0,
        .pDependencies = nullptr,
    };

    return instance.GetDevice().createRenderPass(renderpass_info);
}

} // namespace Vulkan
