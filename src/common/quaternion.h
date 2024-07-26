// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/vector_math.h"

namespace Common {

template <typename T>
class Quaternion {
public:
    Vec3<T> xyz;
    T w{};

    [[nodiscard]] Quaternion<decltype(-T{})> Inverse() const {
        return {-xyz, w};
    }

    [[nodiscard]] Quaternion<decltype(T{} + T{})> operator+(const Quaternion& other) const {
        return {xyz + other.xyz, w + other.w};
    }

    [[nodiscard]] Quaternion<decltype(T{} - T{})> operator-(const Quaternion& other) const {
        return {xyz - other.xyz, w - other.w};
    }

    [[nodiscard]] Quaternion<decltype(T{} * T{} - T{} * T{})> operator*(
        const Quaternion& other) const {
        return {xyz * other.w + other.xyz * w + Cross(xyz, other.xyz),
                w * other.w - Dot(xyz, other.xyz)};
    }

    [[nodiscard]] Quaternion<T> Normalized() const {
        T length = std::sqrt(xyz.Length2() + w * w);
        return {xyz / length, w / length};
    }
};

template <typename T>
[[nodiscard]] auto QuaternionRotate(const Quaternion<T>& q, const Vec3<T>& v) {
    return v + 2 * Cross(q.xyz, Cross(q.xyz, v) + v * q.w);
}

[[nodiscard]] inline Quaternion<float> MakeQuaternion(const Vec3<float>& axis, float angle) {
    return {axis * std::sin(angle / 2), std::cos(angle / 2)};
}

} // namespace Common
