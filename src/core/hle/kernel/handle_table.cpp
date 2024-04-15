// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include <boost/serialization/array.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include "common/archives.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"

SERIALIZE_EXPORT_IMPL(Kernel::HandleTable)

namespace Kernel {
namespace {
constexpr u16 GetSlot(Handle handle) {
    return handle >> 15;
}

constexpr u16 GetGeneration(Handle handle) {
    return handle & 0x7FFF;
}
} // Anonymous namespace

HandleTable::HandleTable(KernelSystem& kernel) : kernel(kernel) {
    next_generation = 1;
    Clear();
}

HandleTable::~HandleTable() = default;

Result HandleTable::Create(Handle* out_handle, std::shared_ptr<Object> obj) {
    DEBUG_ASSERT(obj != nullptr);

    u16 slot = next_free_slot;
    R_UNLESS(slot < generations.size(), ResultOutOfHandles);
    next_free_slot = generations[slot];

    u16 generation = next_generation++;

    // Overflow count so it fits in the 15 bits dedicated to the generation in the handle.
    // CTR-OS doesn't use generation 0, so skip straight to 1.
    if (next_generation >= (1 << 15)) {
        next_generation = 1;
    }

    generations[slot] = generation;
    objects[slot] = std::move(obj);

    *out_handle = generation | (slot << 15);
    return ResultSuccess;
}

Result HandleTable::Duplicate(Handle* out_handle, Handle handle) {
    std::shared_ptr<Object> object = GetGeneric(handle);
    R_UNLESS(object, ResultInvalidHandle);
    return Create(out_handle, std::move(object));
}

Result HandleTable::Close(Handle handle) {
    R_UNLESS(IsValid(handle), ResultInvalidHandle);

    const u16 slot = GetSlot(handle);
    objects[slot] = nullptr;

    generations[slot] = next_free_slot;
    next_free_slot = slot;
    return ResultSuccess;
}

bool HandleTable::IsValid(Handle handle) const {
    const u16 slot = GetSlot(handle);
    const u16 generation = GetGeneration(handle);
    return slot < MAX_COUNT && objects[slot] != nullptr && generations[slot] == generation;
}

std::shared_ptr<Object> HandleTable::GetGeneric(Handle handle) const {
    if (handle == CurrentThread) {
        return SharedFrom(kernel.GetCurrentThreadManager().GetCurrentThread());
    } else if (handle == CurrentProcess) {
        return kernel.GetCurrentProcess();
    }

    if (!IsValid(handle)) {
        return nullptr;
    }
    return objects[GetSlot(handle)];
}

void HandleTable::Clear() {
    for (u16 i = 0; i < MAX_COUNT; ++i) {
        generations[i] = i + 1;
        objects[i] = nullptr;
    }
    next_free_slot = 0;
}

template <class Archive>
void HandleTable::serialize(Archive& ar, const unsigned int) {
    ar & objects;
    ar & generations;
    ar & next_generation;
    ar & next_free_slot;
}
SERIALIZE_IMPL(HandleTable)

} // namespace Kernel
