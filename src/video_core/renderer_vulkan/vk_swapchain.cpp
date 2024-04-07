// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <limits>
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/settings.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"

MICROPROFILE_DEFINE(Vulkan_Acquire, "Vulkan", "Swapchain Acquire", MP_RGB(185, 66, 245));
MICROPROFILE_DEFINE(Vulkan_Present, "Vulkan", "Swapchain Present", MP_RGB(66, 185, 245));

namespace Vulkan {

Swapchain::Swapchain(const Instance& instance_, u32 width, u32 height, vk::SurfaceKHR surface_)
    : instance{instance_}, surface{surface_} {
    FindPresentFormat();
    SetPresentMode();
    Create(width, height, surface);
}

Swapchain::~Swapchain() {
    Destroy();
    instance.GetInstance().destroySurfaceKHR(surface);
}

void Swapchain::Create(u32 width_, u32 height_, vk::SurfaceKHR surface_) {
    width = width_;
    height = height_;
    surface = surface_;
    needs_recreation = false;

    Destroy();

    SetPresentMode();
    SetSurfaceProperties();

    const std::array queue_family_indices = {
        instance.GetGraphicsQueueFamilyIndex(),
        instance.GetPresentQueueFamilyIndex(),
    };

    const bool exclusive = queue_family_indices[0] == queue_family_indices[1];
    const u32 queue_family_indices_count = exclusive ? 1u : 2u;
    const vk::SharingMode sharing_mode =
        exclusive ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent;
    const vk::SwapchainCreateInfoKHR swapchain_info = {
        .surface = surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
                      vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
        .imageSharingMode = sharing_mode,
        .queueFamilyIndexCount = queue_family_indices_count,
        .pQueueFamilyIndices = queue_family_indices.data(),
        .preTransform = transform,
        .compositeAlpha = composite_alpha,
        .presentMode = present_mode,
        .clipped = true,
        .oldSwapchain = nullptr,
    };

    try {
        swapchain = instance.GetDevice().createSwapchainKHR(swapchain_info);
    } catch (vk::SystemError& err) {
        LOG_CRITICAL(Render_Vulkan, "{}", err.what());
        UNREACHABLE();
    }

    SetupImages();
    RefreshSemaphores();
}

bool Swapchain::AcquireNextImage() {
    if (needs_recreation) {
        return false;
    }

    MICROPROFILE_SCOPE(Vulkan_Acquire);
    const vk::Device device = instance.GetDevice();
    const vk::Result result =
        device.acquireNextImageKHR(swapchain, std::numeric_limits<u64>::max(),
                                   image_acquired[frame_index], VK_NULL_HANDLE, &image_index);

    switch (result) {
    case vk::Result::eSuccess:
        break;
    case vk::Result::eSuboptimalKHR:
    case vk::Result::eErrorSurfaceLostKHR:
    case vk::Result::eErrorOutOfDateKHR:
        needs_recreation = true;
        break;
    default:
        LOG_CRITICAL(Render_Vulkan, "Swapchain acquire returned unknown result {}", result);
        UNREACHABLE();
        break;
    }

    return !needs_recreation;
}

void Swapchain::Present() {
    const vk::PresentInfoKHR present_info = {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &present_ready[image_index],
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &image_index,
    };

    MICROPROFILE_SCOPE(Vulkan_Present);
    try {
        [[maybe_unused]] vk::Result result = instance.GetPresentQueue().presentKHR(present_info);
    } catch (vk::OutOfDateKHRError&) {
        needs_recreation = true;
        return;
    } catch (vk::SurfaceLostKHRError&) {
        needs_recreation = true;
        return;
    } catch (const vk::SystemError& err) {
        LOG_CRITICAL(Render_Vulkan, "Swapchain presentation failed {}", err.what());
        UNREACHABLE();
    }

    frame_index = (frame_index + 1) % image_count;
}

void Swapchain::FindPresentFormat() {
    const auto formats = instance.GetPhysicalDevice().getSurfaceFormatsKHR(surface);

    // If there is a single undefined surface format, the device doesn't care, so we'll just use
    // RGBA.
    if (formats[0].format == vk::Format::eUndefined) {
        surface_format.format = vk::Format::eR8G8B8A8Unorm;
        surface_format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        return;
    }

    // Try to find a suitable format.
    for (const vk::SurfaceFormatKHR& sformat : formats) {
        vk::Format format = sformat.format;
        if (format != vk::Format::eR8G8B8A8Unorm && format != vk::Format::eB8G8R8A8Unorm) {
            continue;
        }

        surface_format.format = format;
        surface_format.colorSpace = sformat.colorSpace;
        return;
    }

    UNREACHABLE_MSG("Unable to find required swapchain format!");
}

void Swapchain::SetPresentMode() {
    const auto modes = instance.GetPhysicalDevice().getSurfacePresentModesKHR(surface);
    const bool use_vsync = Settings::values.use_vsync_new.GetValue();
    const auto find_mode = [&modes](vk::PresentModeKHR requested) {
        const auto it =
            std::find_if(modes.begin(), modes.end(),
                         [&requested](vk::PresentModeKHR mode) { return mode == requested; });

        return it != modes.end();
    };

    present_mode = vk::PresentModeKHR::eFifo;
    const bool has_immediate = find_mode(vk::PresentModeKHR::eImmediate);
    const bool has_mailbox = find_mode(vk::PresentModeKHR::eMailbox);
    if (!has_immediate && !has_mailbox) {
        LOG_WARNING(Render_Vulkan, "Forcing Fifo present mode as no alternatives are available");
        return;
    }

    // If the user has disabled vsync use immediate mode for the least latency.
    // This may have screen tearing.
    if (!use_vsync) {
        present_mode =
            has_immediate ? vk::PresentModeKHR::eImmediate : vk::PresentModeKHR::eMailbox;
        return;
    }
    // If vsync is enabled attempt to use mailbox mode in case the user wants to speedup/slowdown
    // the game. If mailbox is not available use immediate and warn about it.
    if (use_vsync && Settings::values.frame_limit.GetValue() > 100) {
        present_mode = has_mailbox ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eImmediate;
        if (!has_mailbox) {
            LOG_WARNING(
                Render_Vulkan,
                "Vsync enabled while frame limiting and no mailbox support, expect tearing");
        }
        return;
    }
}

void Swapchain::SetSurfaceProperties() {
    const vk::SurfaceCapabilitiesKHR capabilities =
        instance.GetPhysicalDevice().getSurfaceCapabilitiesKHR(surface);

    extent = capabilities.currentExtent;
    if (capabilities.currentExtent.width == std::numeric_limits<u32>::max()) {
        extent.width = std::max(capabilities.minImageExtent.width,
                                std::min(capabilities.maxImageExtent.width, width));
        extent.height = std::max(capabilities.minImageExtent.height,
                                 std::min(capabilities.maxImageExtent.height, height));
    }

    // Select number of images in swap chain, we prefer one buffer in the background to work on
    image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) {
        image_count = std::min(image_count, capabilities.maxImageCount);
    }

    // Prefer identity transform if possible
    transform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
    if (!(capabilities.supportedTransforms & transform)) {
        transform = capabilities.currentTransform;
    }

    // Opaque is not supported everywhere.
    composite_alpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    if (!(capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque)) {
        composite_alpha = vk::CompositeAlphaFlagBitsKHR::eInherit;
    }
}

void Swapchain::Destroy() {
    vk::Device device = instance.GetDevice();
    if (swapchain) {
        device.destroySwapchainKHR(swapchain);
    }
    for (u32 i = 0; i < image_count; i++) {
        device.destroySemaphore(image_acquired[i]);
        device.destroySemaphore(present_ready[i]);
    }
    image_acquired.clear();
    present_ready.clear();
}

void Swapchain::RefreshSemaphores() {
    const vk::Device device = instance.GetDevice();
    image_acquired.resize(image_count);
    present_ready.resize(image_count);

    for (vk::Semaphore& semaphore : image_acquired) {
        semaphore = device.createSemaphore({});
    }
    for (vk::Semaphore& semaphore : present_ready) {
        semaphore = device.createSemaphore({});
    }

    if (instance.HasDebuggingToolAttached()) {
        for (u32 i = 0; i < image_count; ++i) {
            Vulkan::SetObjectName(device, image_acquired[i],
                                  "Swapchain Semaphore: image_acquired {}", i);
            Vulkan::SetObjectName(device, present_ready[i], "Swapchain Semaphore: present_ready {}",
                                  i);
        }
    }
}

void Swapchain::SetupImages() {
    vk::Device device = instance.GetDevice();
    images = device.getSwapchainImagesKHR(swapchain);
    image_count = static_cast<u32>(images.size());

    if (instance.HasDebuggingToolAttached()) {
        for (u32 i = 0; i < image_count; ++i) {
            Vulkan::SetObjectName(device, images[i], "Swapchain Image {}", i);
        }
    }
}

} // namespace Vulkan
