// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <cstring>
#include "common/cityhash.h"
#include "common/common_types.h"

namespace Common {

/**
 * Computes a 64-bit hash over the specified block of data
 * @param data Block of data to compute hash over
 * @param len Length of data (in bytes) to compute hash over
 * @returns 64-bit hash value that was computed over the data block
 */
static inline u64 ComputeHash64(const void* data, std::size_t len) noexcept {
    return CityHash64(static_cast<const char*>(data), len);
}

/**
 * Computes a 64-bit hash of a struct. In addition to being trivially copyable, it is also critical
 * that either the struct includes no padding, or that any padding is initialized to a known value
 * by memsetting the struct to 0 before filling it in.
 */
template <typename T>
static inline u64 ComputeStructHash64(const T& data) noexcept {
    static_assert(std::is_trivially_copyable_v<T>,
                  "Type passed to ComputeStructHash64 must be trivially copyable");
    return ComputeHash64(&data, sizeof(data));
}

/**
 * Combines the seed parameter with the provided hash, producing a new unique hash
 * Implementation from: http://boost.sourceforge.net/doc/html/boost/hash_combine.html
 */
[[nodiscard]] inline u64 HashCombine(const u64 seed, const u64 hash) {
    return seed ^ (hash + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

template <typename T>
struct IdentityHash {
    std::size_t operator()(const T& value) const {
        return value;
    }
};

/// A helper template that ensures the padding in a struct is initialized by memsetting to 0.
template <typename T>
struct HashableStruct {
    // In addition to being trivially copyable, T must also have a trivial default constructor,
    // because any member initialization would be overridden by memset
    static_assert(std::is_trivial_v<T>, "Type passed to HashableStruct must be trivial");
    /*
     * We use a union because "implicitly-defined copy/move constructor for a union X copies the
     * object representation of X." and "implicitly-defined copy assignment operator for a union X
     * copies the object representation (3.9) of X." = Bytewise copy instead of memberwise copy.
     * This is important because the padding bytes are included in the hash and comparison between
     * objects.
     */
    union {
        T state;
    };

    HashableStruct() noexcept {
        // Memset structure to zero padding bits, so that they will be deterministic when hashing
        std::memset(&state, 0, sizeof(T));
    }

    bool operator==(const HashableStruct<T>& o) const noexcept {
        return std::memcmp(&state, &o.state, sizeof(T)) == 0;
    };

    bool operator!=(const HashableStruct<T>& o) const noexcept {
        return !(*this == o);
    };

    std::size_t Hash() const noexcept {
        return Common::ComputeStructHash64(state);
    }
};

} // namespace Common
