// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/serialization/access.hpp>

#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/vector_math.h"

#define LCD_REG_INDEX(field_name) (offsetof(Pica::RegsLcd, field_name) / sizeof(u32))

namespace Pica {

union ColorFill {
    u32 raw;

    BitField<0, 8, u32> color_r;
    BitField<8, 8, u32> color_g;
    BitField<16, 8, u32> color_b;
    BitField<24, 1, u32> is_enabled;

    Common::Vec3<u8> AsVector() const noexcept {
        return Common::MakeVec<u8>(color_r, color_g, color_b);
    }
};

struct RegsLcd {
    INSERT_PADDING_WORDS(0x81);
    ColorFill color_fill_top;
    INSERT_PADDING_WORDS(0xE);
    u32 backlight_top;
    INSERT_PADDING_WORDS(0x1F0);
    ColorFill color_fill_bottom;
    INSERT_PADDING_WORDS(0xE);
    u32 backlight_bottom;
    INSERT_PADDING_WORDS(0x16F);

    static constexpr std::size_t NumIds() {
        return sizeof(RegsLcd) / sizeof(u32);
    }

    const u32& operator[](int index) const {
        const u32* content = reinterpret_cast<const u32*>(this);
        return content[index];
    }

    u32& operator[](int index) {
        u32* content = reinterpret_cast<u32*>(this);
        return content[index];
    }

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & color_fill_top.raw;
        ar & backlight_top;
        ar & color_fill_bottom.raw;
        ar & backlight_bottom;
    }
    friend class boost::serialization::access;
};
static_assert(std::is_standard_layout_v<RegsLcd>, "Structure does not use standard layout");

#define ASSERT_REG_POSITION(field_name, position)                                                  \
    static_assert(offsetof(RegsLcd, field_name) == position * 4,                                   \
                  "Field " #field_name " has invalid position")
ASSERT_REG_POSITION(color_fill_top, 0x81);
ASSERT_REG_POSITION(backlight_top, 0x90);
ASSERT_REG_POSITION(color_fill_bottom, 0x281);
ASSERT_REG_POSITION(backlight_bottom, 0x290);

#undef ASSERT_REG_POSITION

} // namespace Pica
