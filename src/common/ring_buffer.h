// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <type_traits>
#include <vector>
#include "common/common_types.h"

namespace Common {

/// SPSC ring buffer
/// @tparam T            Element type
/// @tparam capacity     Number of slots in ring buffer
/// @tparam granularity  Slot size in terms of number of elements
template <typename T, std::size_t capacity, std::size_t granularity = 1>
class RingBuffer {
    /// A "slot" is made of `granularity` elements of `T`.
    static constexpr std::size_t slot_size = granularity * sizeof(T);
    // T must be safely memcpy-able and have a trivial default constructor.
    static_assert(std::is_trivial_v<T>);
    // Ensure capacity is sensible.
    static_assert(capacity < std::numeric_limits<std::size_t>::max() / 2 / granularity);
    static_assert((capacity & (capacity - 1)) == 0, "capacity must be a power of two");
    // Ensure lock-free.
    static_assert(std::atomic_size_t::is_always_lock_free);

public:
    /// Pushes slots into the ring buffer
    /// @param new_slots   Pointer to the slots to push
    /// @param slot_count  Number of slots to push
    /// @returns The number of slots actually pushed
    std::size_t Push(const void* new_slots, std::size_t slot_count) {
        const std::size_t write_index = m_write_index.load();
        const std::size_t slots_free = capacity + m_read_index.load() - write_index;
        const std::size_t push_count = std::min(slot_count, slots_free);

        const std::size_t pos = write_index % capacity;
        const std::size_t first_copy = std::min(capacity - pos, push_count);
        const std::size_t second_copy = push_count - first_copy;

        const char* in = static_cast<const char*>(new_slots);
        std::memcpy(m_data.data() + pos * granularity, in, first_copy * slot_size);
        in += first_copy * slot_size;
        std::memcpy(m_data.data(), in, second_copy * slot_size);

        m_write_index.store(write_index + push_count);

        return push_count;
    }

    std::size_t Push(std::span<const T> input) {
        return Push(input.data(), input.size() / granularity);
    }

    /// Pops slots from the ring buffer
    /// @param output     Where to store the popped slots
    /// @param max_slots  Maximum number of slots to pop
    /// @returns The number of slots actually popped
    std::size_t Pop(void* output, std::size_t max_slots = ~std::size_t(0)) {
        const std::size_t read_index = m_read_index.load();
        const std::size_t slots_filled = m_write_index.load() - read_index;
        const std::size_t pop_count = std::min(slots_filled, max_slots);

        const std::size_t pos = read_index % capacity;
        const std::size_t first_copy = std::min(capacity - pos, pop_count);
        const std::size_t second_copy = pop_count - first_copy;

        char* out = static_cast<char*>(output);
        std::memcpy(out, m_data.data() + pos * granularity, first_copy * slot_size);
        out += first_copy * slot_size;
        std::memcpy(out, m_data.data(), second_copy * slot_size);

        m_read_index.store(read_index + pop_count);

        return pop_count;
    }

    std::vector<T> Pop(std::size_t max_slots = ~std::size_t(0)) {
        std::vector<T> out(std::min(max_slots, capacity) * granularity);
        const std::size_t count = Pop(out.data(), out.size() / granularity);
        out.resize(count * granularity);
        return out;
    }

    /// @returns Number of slots used
    [[nodiscard]] std::size_t Size() const {
        return m_write_index.load() - m_read_index.load();
    }

    /// @returns Maximum size of ring buffer
    [[nodiscard]] constexpr std::size_t Capacity() const {
        return capacity;
    }

private:
    // It is important to separate the below atomics for performance reasons:
    // Having them on the same cache-line would result in false-sharing between them.
#ifdef __cpp_lib_hardware_interference_size
    static constexpr std::size_t padding_size =
        std::hardware_destructive_interference_size - sizeof(std::atomic_size_t);
#else
    static constexpr std::size_t padding_size = 128 - sizeof(std::atomic_size_t);
#endif

    std::atomic_size_t m_read_index{0};
    char padding1[padding_size];

    std::atomic_size_t m_write_index{0};
    char padding2[padding_size];

    std::array<T, granularity * capacity> m_data;
};

} // namespace Common
