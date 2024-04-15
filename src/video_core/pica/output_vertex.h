// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_funcs.h"
#include "common/vector_math.h"
#include "video_core/pica_types.h"

namespace Pica {

struct RasterizerRegs;

using AttributeBuffer = std::array<Common::Vec4<f24>, 16>;

struct OutputVertex {
    OutputVertex() = default;
    explicit OutputVertex(const RasterizerRegs& regs, const AttributeBuffer& output);

    Common::Vec4<f24> pos;
    Common::Vec4<f24> quat;
    Common::Vec4<f24> color;
    Common::Vec2<f24> tc0;
    Common::Vec2<f24> tc1;
    f24 tc0_w;
    INSERT_PADDING_WORDS(1);
    Common::Vec3<f24> view;
    INSERT_PADDING_WORDS(1);
    Common::Vec2<f24> tc2;

private:
    template <class Archive>
    void serialize(Archive& ar, const u32) {
        ar & pos;
        ar & quat;
        ar & color;
        ar & tc0;
        ar & tc1;
        ar & tc0_w;
        ar & view;
        ar & tc2;
    }
    friend class boost::serialization::access;
};
static_assert(std::is_trivial_v<OutputVertex>, "Structure is not POD");
static_assert(sizeof(OutputVertex) == 24 * sizeof(f32), "OutputVertex has invalid size");

} // namespace Pica
