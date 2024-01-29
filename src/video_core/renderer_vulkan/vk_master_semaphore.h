// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <condition_variable>
#include <queue>
#include "common/common_types.h"
#include "common/polyfill_thread.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

class Instance;
class Scheduler;

class MasterSemaphore {
public:
    virtual ~MasterSemaphore() = default;

    [[nodiscard]] u64 CurrentTick() const noexcept {
        return current_tick.load(std::memory_order_acquire);
    }

    [[nodiscard]] u64 KnownGpuTick() const noexcept {
        return gpu_tick.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool IsFree(u64 tick) const noexcept {
        return KnownGpuTick() >= tick;
    }

    [[nodiscard]] u64 NextTick() noexcept {
        return current_tick.fetch_add(1, std::memory_order_release);
    }

    /// Refresh the known GPU tick
    virtual void Refresh() = 0;

    /// Waits for a tick to be hit on the GPU
    virtual void Wait(u64 tick) = 0;

    /// Submits the provided command buffer for execution
    virtual void SubmitWork(vk::CommandBuffer cmdbuf, vk::Semaphore wait, vk::Semaphore signal,
                            u64 signal_value) = 0;

protected:
    std::atomic<u64> gpu_tick{0};     ///< Current known GPU tick.
    std::atomic<u64> current_tick{1}; ///< Current logical tick.
};

class MasterSemaphoreTimeline : public MasterSemaphore {
public:
    explicit MasterSemaphoreTimeline(const Instance& instance);
    ~MasterSemaphoreTimeline() override;

    [[nodiscard]] vk::Semaphore Handle() const noexcept {
        return semaphore.get();
    }

    void Refresh() override;

    void Wait(u64 tick) override;

    void SubmitWork(vk::CommandBuffer cmdbuf, vk::Semaphore wait, vk::Semaphore signal,
                    u64 signal_value) override;

private:
    const Instance& instance;
    vk::UniqueSemaphore semaphore; ///< Timeline semaphore.
};

class MasterSemaphoreFence : public MasterSemaphore {
    using Waitable = std::pair<vk::Fence, u64>;

public:
    explicit MasterSemaphoreFence(const Instance& instance);
    ~MasterSemaphoreFence() override;

    void Refresh() override;

    void Wait(u64 tick) override;

    void SubmitWork(vk::CommandBuffer cmdbuf, vk::Semaphore wait, vk::Semaphore signal,
                    u64 signal_value) override;

private:
    void WaitThread(std::stop_token token);

    vk::UniqueFence GetFreeFence();

private:
    const Instance& instance;

    struct Fence {
        vk::UniqueFence handle;
        u64 signal_value;
    };

    std::queue<vk::UniqueFence> free_queue;
    std::queue<Fence> wait_queue;
    std::mutex free_mutex;
    std::mutex wait_mutex;
    std::condition_variable_any wait_cv;
    std::jthread wait_thread;
};

} // namespace Vulkan
