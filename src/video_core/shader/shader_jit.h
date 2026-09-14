// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/arch.h"
#if CITRA_ARCH(x86_64) || CITRA_ARCH(arm64)

#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <queue>
#include "common/common_types.h"
#include "video_core/shader/shader.h"

namespace Pica::Shader {

class JitShader;

class JitEngine final : public ShaderEngine {
public:
    JitEngine();
    ~JitEngine() override;

    void SetupBatch(ShaderSetup& setup, u32 entry_point) override;
    void Run(const ShaderSetup& setup, ShaderUnit& state) const override;

private:
    static constexpr size_t MAX_CACHE_SIZE = 1000; // Maximum number of shaders to cache
    std::unordered_map<u64, std::shared_future<std::unique_ptr<JitShader>>> cache;
    std::list<u64> lru_list; // Track LRU order of shaders
    mutable std::mutex cache_mutex;

    // Parallel compilation support
    std::vector<std::thread> thread_pool;
    std::queue<std::function<void()>> compile_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    bool stop_threads = false;
    std::unique_ptr<JitShader> stub_shader;

    void EvictLRU();
    void UpdateLRU(u64 key);
    void ThreadWorker();
    void StartThreadPool(size_t num_threads);
    void StopThreadPool();
};

} // namespace Pica::Shader

#endif // CITRA_ARCH(x86_64) || CITRA_ARCH(arm64)
