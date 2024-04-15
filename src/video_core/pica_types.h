// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cmath>
#include <cstring>
#include <boost/serialization/access.hpp>
#include "common/common_types.h"

namespace Pica {

/**
 * Template class for converting arbitrary Pica float types to IEEE 754 32-bit single-precision
 * floating point.
 *
 * When decoding, format is as follows:
 *  - The first `M` bits are the mantissa
 *  - The next `E` bits are the exponent
 *  - The last bit is the sign bit
 *
 * @todo Verify on HW if this conversion is sufficiently accurate.
 */
template <unsigned M, unsigned E>
struct Float {
public:
    static constexpr Float<M, E> FromFloat32(float val) {
        Float<M, E> ret;
        ret.value = val;
        return ret;
    }

    static constexpr Float<M, E> FromRaw(u32 hex) {
        Float<M, E> res;

        const s32 width = M + E + 1;
        const s32 bias = 128 - (1 << (E - 1));
        s32 exponent = (hex >> M) & ((1 << E) - 1);
        const u32 mantissa = hex & ((1 << M) - 1);
        const u32 sign = (hex >> (E + M)) << 31;

        if (hex & ((1 << (width - 1)) - 1)) {
            if (exponent == (1 << E) - 1)
                exponent = 255;
            else
                exponent += bias;
            hex = sign | (mantissa << (23 - M)) | (exponent << 23);
        } else {
            hex = sign;
        }

        std::memcpy(&res.value, &hex, sizeof(float));

        return res;
    }

    static constexpr Float<M, E> Zero() {
        return FromFloat32(0.f);
    }

    static constexpr Float<M, E> One() {
        return FromFloat32(1.f);
    }

    // Not recommended for anything but logging
    constexpr float ToFloat32() const {
        return value;
    }

    constexpr Float<M, E> operator*(const Float<M, E>& flt) const {
        float result = value * flt.ToFloat32();
        // PICA gives 0 instead of NaN when multiplying by inf
        if (std::isnan(result))
            if (!std::isnan(value) && !std::isnan(flt.ToFloat32()))
                result = 0.f;
        return Float<M, E>::FromFloat32(result);
    }

    constexpr Float<M, E> operator/(const Float<M, E>& flt) const {
        return Float<M, E>::FromFloat32(ToFloat32() / flt.ToFloat32());
    }

    constexpr Float<M, E> operator+(const Float<M, E>& flt) const {
        return Float<M, E>::FromFloat32(ToFloat32() + flt.ToFloat32());
    }

    constexpr Float<M, E> operator-(const Float<M, E>& flt) const {
        return Float<M, E>::FromFloat32(ToFloat32() - flt.ToFloat32());
    }

    constexpr Float<M, E>& operator*=(const Float<M, E>& flt) {
        value = operator*(flt).value;
        return *this;
    }

    constexpr Float<M, E>& operator/=(const Float<M, E>& flt) {
        value /= flt.ToFloat32();
        return *this;
    }

    constexpr Float<M, E>& operator+=(const Float<M, E>& flt) {
        value += flt.ToFloat32();
        return *this;
    }

    constexpr Float<M, E>& operator-=(const Float<M, E>& flt) {
        value -= flt.ToFloat32();
        return *this;
    }

    constexpr Float<M, E> operator-() const {
        return Float<M, E>::FromFloat32(-ToFloat32());
    }

    constexpr bool operator<(const Float<M, E>& flt) const {
        return ToFloat32() < flt.ToFloat32();
    }

    constexpr bool operator>(const Float<M, E>& flt) const {
        return ToFloat32() > flt.ToFloat32();
    }

    constexpr bool operator>=(const Float<M, E>& flt) const {
        return ToFloat32() >= flt.ToFloat32();
    }

    constexpr bool operator<=(const Float<M, E>& flt) const {
        return ToFloat32() <= flt.ToFloat32();
    }

    constexpr bool operator==(const Float<M, E>& flt) const {
        return ToFloat32() == flt.ToFloat32();
    }

    constexpr bool operator!=(const Float<M, E>& flt) const {
        return ToFloat32() != flt.ToFloat32();
    }

private:
    static constexpr u32 MASK = (1 << (M + E + 1)) - 1;
    static constexpr u32 MANTISSA_MASK = (1 << M) - 1;
    static constexpr u32 EXPONENT_MASK = (1 << E) - 1;

    // Stored as a regular float, merely for convenience
    // TODO: Perform proper arithmetic on this!
    float value;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version) {
        ar & value;
    }
};

using f24 = Pica::Float<16, 7>;
using f20 = Pica::Float<12, 7>;
using f16 = Pica::Float<10, 5>;

} // namespace Pica
