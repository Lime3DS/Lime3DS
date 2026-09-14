// Copyright 2016-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/arch.h"
#if CITRA_ARCH(x86_64) || CITRA_ARCH(arm64)

#include "common/assert.h"
#include "common/hash.h"
#include "common/microprofile.h"
#include "video_core/shader/shader.h"
#include "video_core/shader/shader_jit.h"
#if CITRA_ARCH(arm64)
#include "video_core/shader/shader_jit_a64_compiler.h"
#endif
#if CITRA_ARCH(x86_64)
#include "video_core/shader/shader_jit_x64_compiler.h"
#endif
#include <future>

namespace Pica::Shader {

JitEngine::JitEngine() {
    stub_shader = std::make_unique<JitShader>();
    // Optionally, compile a minimal stub shader here if needed
    StartThreadPool(std::thread::hardware_concurrency());
}

JitEngine::~JitEngine() {
    StopThreadPool();
}

void JitEngine::StartThreadPool(size_t num_threads) {
    stop_threads = false;
    for (size_t i = 0; i < num_threads; ++i) {
        thread_pool.emplace_back([this]() { ThreadWorker(); });
    }
}

void JitEngine::StopThreadPool() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop_threads = true;
    }
    queue_cv.notify_all();
    for (auto& t : thread_pool) {
        if (t.joinable())
            t.join();
    }
    thread_pool.clear();
}

void JitEngine::ThreadWorker() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this]() { return stop_threads || !compile_queue.empty(); });
            if (stop_threads && compile_queue.empty())
                return;
            job = std::move(compile_queue.front());
            compile_queue.pop();
        }
        job();
    }
}

void JitEngine::SetupBatch(ShaderSetup& setup, u32 entry_point) {
    ASSERT(entry_point < MAX_PROGRAM_CODE_LENGTH);
    setup.entry_point = entry_point;

    setup.DoProgramCodeFixup();
    const u64 code_hash = setup.GetProgramCodeHash();
    const u64 swizzle_hash = setup.GetSwizzleDataHash();
    const u64 cache_key = Common::HashCombine(code_hash, swizzle_hash);

    std::shared_future<std::unique_ptr<JitShader>> shader_future;
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto iter = cache.find(cache_key);
        if (iter != cache.end()) {
            shader_future = iter->second;
            UpdateLRU(cache_key);
        } else {
            // Compile synchronously and store the result
            auto shader = std::make_unique<JitShader>();
            shader->Compile(&setup.GetProgramCode(), &setup.GetSwizzleData());
            auto ready_future = std::make_shared<std::promise<std::unique_ptr<JitShader>>>();
            ready_future->set_value(std::move(shader));
            shader_future = ready_future->get_future().share();
            cache[cache_key] = shader_future;
            lru_list.push_front(cache_key);
        }
    }
    // Wait for the shader to be ready (if compiling in background)
    setup.cached_shader = shader_future.get().get();
}

void JitEngine::EvictLRU() {
    if (lru_list.empty()) {
        return;
    }
    const u64 key = lru_list.back();
    lru_list.pop_back();
    cache.erase(key);
}

void JitEngine::UpdateLRU(u64 key) {
    auto it = std::find(lru_list.begin(), lru_list.end(), key);
    if (it != lru_list.end()) {
        lru_list.erase(it);
    }
    lru_list.push_front(key);
}

MICROPROFILE_DECLARE(GPU_Shader);

void JitEngine::Run(const ShaderSetup& setup, ShaderUnit& state) const {
    // Null check: skip draw if shader is not ready
    if (!setup.cached_shader) {
        return;
    }

    MICROPROFILE_SCOPE(GPU_Shader);

    const JitShader* shader = static_cast<const JitShader*>(setup.cached_shader);
    shader->Run(setup, state, setup.entry_point);
}

} // namespace Pica::Shader

#endif // CITRA_ARCH(x86_64) || CITRA_ARCH(arm64)
