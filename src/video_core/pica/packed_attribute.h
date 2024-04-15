// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/serialization/binary_object.hpp>

#include "common/vector_math.h"
#include "video_core/pica_types.h"

namespace Pica {

/**
 * Uniforms and fixed attributes are written in a packed format such that four float24 values are
 * encoded in three 32-bit numbers. Uniforms can also encode four float32 values in four 32-bit
 * numbers. We write to internal memory once a full vector is written.
 */
struct PackedAttribute {
    std::array<u32, 4> buffer{};
    u32 index{};

    /// Places a word to the queue and returns true if the queue becomes full.
    constexpr bool Push(u32 word, bool is_float32 = false) {
        buffer[index++] = word;
        return (index >= 4 && is_float32) || (index >= 3 && !is_float32);
    }

    /// Resets the queue discarding previous entries.
    constexpr void Reset() {
        index = 0;
    }

    /// Returns the queue contents with either float24 or float32 interpretation.
    constexpr Common::Vec4<f24> Get(bool is_float32 = false) {
        Reset();
        if (is_float32) {
            return AsFloat32();
        } else {
            return AsFloat24();
        }
    }

private:
    /// Decodes the queue contents with float24 transfer mode.
    constexpr Common::Vec4<f24> AsFloat24() const {
        const u32 x = buffer[2] & 0xFFFFFF;
        const u32 y = ((buffer[1] & 0xFFFF) << 8) | ((buffer[2] >> 24) & 0xFF);
        const u32 z = ((buffer[0] & 0xFF) << 16) | ((buffer[1] >> 16) & 0xFFFF);
        const u32 w = buffer[0] >> 8;
        return Common::Vec4<f24>{f24::FromRaw(x), f24::FromRaw(y), f24::FromRaw(z),
                                 f24::FromRaw(w)};
    }

    /// Decodes the queue contents with float32 transfer mode.
    constexpr Common::Vec4<f24> AsFloat32() const {
        Common::Vec4<f24> uniform;
        for (u32 i = 0; i < 4; i++) {
            const f32 buffer_value = std::bit_cast<f32>(buffer[i]);
            uniform[3 - i] = f24::FromFloat32(buffer_value);
        }
        return uniform;
    }

private:
    template <class Archive>
    void serialize(Archive& ar, const u32) {
        ar & buffer;
        ar & index;
    }
    friend class boost::serialization::access;
};

} // namespace Pica
