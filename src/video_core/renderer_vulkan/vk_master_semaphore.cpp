// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <limits>
#include <mutex>
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_master_semaphore.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Vulkan {

constexpr u64 WAIT_TIMEOUT = std::numeric_limits<u64>::max();

MasterSemaphoreTimeline::MasterSemaphoreTimeline(const Instance& instance_) : instance{instance_} {
    const vk::StructureChain semaphore_chain = {
        vk::SemaphoreCreateInfo{},
        vk::SemaphoreTypeCreateInfoKHR{
            .semaphoreType = vk::SemaphoreType::eTimeline,
            .initialValue = 0,
        },
    };
    semaphore = instance.GetDevice().createSemaphoreUnique(semaphore_chain.get());
}

MasterSemaphoreTimeline::~MasterSemaphoreTimeline() = default;

void MasterSemaphoreTimeline::Refresh() {
    u64 this_tick{};
    u64 counter{};
    do {
        this_tick = gpu_tick.load(std::memory_order_acquire);
        counter = instance.GetDevice().getSemaphoreCounterValueKHR(*semaphore);
        if (counter < this_tick) {
            return;
        }
    } while (!gpu_tick.compare_exchange_weak(this_tick, counter, std::memory_order_release,
                                             std::memory_order_relaxed));
}

void MasterSemaphoreTimeline::Wait(u64 tick) {
    // No need to wait if the GPU is ahead of the tick
    if (IsFree(tick)) {
        return;
    }
    // Update the GPU tick and try again
    Refresh();
    if (IsFree(tick)) {
        return;
    }

    // If none of the above is hit, fallback to a regular wait
    const vk::SemaphoreWaitInfoKHR wait_info = {
        .semaphoreCount = 1,
        .pSemaphores = &semaphore.get(),
        .pValues = &tick,
    };

    while (instance.GetDevice().waitSemaphoresKHR(&wait_info, WAIT_TIMEOUT) !=
           vk::Result::eSuccess) {
    }
    Refresh();
}

void MasterSemaphoreTimeline::SubmitWork(vk::CommandBuffer cmdbuf, vk::Semaphore wait,
                                         vk::Semaphore signal, u64 signal_value) {
    cmdbuf.end();

    const u32 num_signal_semaphores = signal ? 2U : 1U;
    const std::array signal_values{signal_value, u64(0)};
    const std::array signal_semaphores{Handle(), signal};

    const u32 num_wait_semaphores = wait ? 2U : 1U;
    const std::array wait_values{signal_value - 1, u64(1)};
    const std::array wait_semaphores{Handle(), wait};

    static constexpr std::array<vk::PipelineStageFlags, 2> wait_stage_masks = {
        vk::PipelineStageFlagBits::eAllCommands,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
    };

    const vk::TimelineSemaphoreSubmitInfoKHR timeline_si = {
        .waitSemaphoreValueCount = num_wait_semaphores,
        .pWaitSemaphoreValues = wait_values.data(),
        .signalSemaphoreValueCount = num_signal_semaphores,
        .pSignalSemaphoreValues = signal_values.data(),
    };

    const vk::SubmitInfo submit_info = {
        .pNext = &timeline_si,
        .waitSemaphoreCount = num_wait_semaphores,
        .pWaitSemaphores = wait_semaphores.data(),
        .pWaitDstStageMask = wait_stage_masks.data(),
        .commandBufferCount = 1u,
        .pCommandBuffers = &cmdbuf,
        .signalSemaphoreCount = num_signal_semaphores,
        .pSignalSemaphores = signal_semaphores.data(),
    };

    try {
        instance.GetGraphicsQueue().submit(submit_info);
    } catch (vk::DeviceLostError& err) {
        UNREACHABLE_MSG("Device lost during submit: {}", err.what());
    }
}

constexpr u64 FENCE_RESERVE = 8;

MasterSemaphoreFence::MasterSemaphoreFence(const Instance& instance_) : instance{instance_} {
    const vk::Device device{instance.GetDevice()};
    for (u64 i = 0; i < FENCE_RESERVE; i++) {
        free_queue.push(device.createFenceUnique({}));
    }
    wait_thread = std::jthread([this](std::stop_token token) { WaitThread(token); });
}

MasterSemaphoreFence::~MasterSemaphoreFence() {
    std::ranges::for_each(free_queue,
                          [this](auto fence) { instance.GetDevice().destroyFence(fence); });
}

void MasterSemaphoreFence::Refresh() {}

void MasterSemaphoreFence::Wait(u64 tick) {
    while (true) {
        u64 current_value = gpu_tick.load(std::memory_order_relaxed);
        if (current_value >= tick) {
            return;
        }
        gpu_tick.wait(current_value);
    }
}

void MasterSemaphoreFence::SubmitWork(vk::CommandBuffer cmdbuf, vk::Semaphore wait,
                                      vk::Semaphore signal, u64 signal_value) {
    cmdbuf.end();

    const u32 num_signal_semaphores = signal ? 1U : 0U;
    const u32 num_wait_semaphores = wait ? 1U : 0U;

    static constexpr std::array<vk::PipelineStageFlags, 1> wait_stage_masks = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
    };

    const vk::SubmitInfo submit_info = {
        .waitSemaphoreCount = num_wait_semaphores,
        .pWaitSemaphores = &wait,
        .pWaitDstStageMask = wait_stage_masks.data(),
        .commandBufferCount = 1u,
        .pCommandBuffers = &cmdbuf,
        .signalSemaphoreCount = num_signal_semaphores,
        .pSignalSemaphores = &signal,
    };

    vk::UniqueFence fence{GetFreeFence()};
    try {
        instance.GetGraphicsQueue().submit(submit_info, *fence);
    } catch (vk::DeviceLostError& err) {
        UNREACHABLE_MSG("Device lost during submit: {}", err.what());
    }

    std::scoped_lock lock{wait_mutex};
    wait_queue.push({
        .handle = std::move(fence),
        .signal_value = signal_value,
    });
    wait_cv.notify_one();
}

void MasterSemaphoreFence::WaitThread(std::stop_token token) {
    const vk::Device device{instance.GetDevice()};
    while (!token.stop_requested()) {
        Fence fence;
        {
            std::unique_lock lock{wait_mutex};
            Common::CondvarWait(wait_cv, lock, token, [this] { return !wait_queue.empty(); });
            if (token.stop_requested()) {
                return;
            }
            fence = std::move(wait_queue.front());
            wait_queue.pop();
        }

        const vk::Result result = device.waitForFences(*fence.handle, true, WAIT_TIMEOUT);
        if (result != vk::Result::eSuccess) {
            UNREACHABLE_MSG("Fence wait failed with error {}", vk::to_string(result));
        }
        device.resetFences(*fence.handle);

        gpu_tick.store(fence.signal_value);
        gpu_tick.notify_all();

        std::scoped_lock lock{free_mutex};
        free_queue.push(std::move(fence.handle));
    }
}

vk::UniqueFence MasterSemaphoreFence::GetFreeFence() {
    std::scoped_lock lock{free_mutex};
    if (free_queue.empty()) {
        return instance.GetDevice().createFenceUnique({});
    }

    vk::UniqueFence fence{std::move(free_queue.front())};
    free_queue.pop();
    return fence;
}

} // namespace Vulkan
