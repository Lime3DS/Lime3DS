// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include "common/bit_field.h"
#include "common/color.h"
#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/texture/etc1.h"

namespace Pica::Texture {

namespace {

constexpr std::array<std::array<u8, 2>, 8> etc1_modifier_table = {{
    {2, 8},
    {5, 17},
    {9, 29},
    {13, 42},
    {18, 60},
    {24, 80},
    {33, 106},
    {47, 183},
}};

union ETC1Tile {
    u64 raw;

    // Each of these two is a collection of 16 bits (one per lookup value)
    BitField<0, 16, u64> table_subindexes;
    BitField<16, 16, u64> negation_flags;

    unsigned GetTableSubIndex(unsigned index) const {
        return (table_subindexes >> index) & 1;
    }

    bool GetNegationFlag(unsigned index) const {
        return ((negation_flags >> index) & 1) == 1;
    }

    BitField<32, 1, u64> flip;
    BitField<33, 1, u64> differential_mode;

    BitField<34, 3, u64> table_index_2;
    BitField<37, 3, u64> table_index_1;

    union {
        // delta value + base value
        BitField<40, 3, s64> db;
        BitField<43, 5, u64> b;

        BitField<48, 3, s64> dg;
        BitField<51, 5, u64> g;

        BitField<56, 3, s64> dr;
        BitField<59, 5, u64> r;
    } differential;

    union {
        BitField<40, 4, u64> b2;
        BitField<44, 4, u64> b1;

        BitField<48, 4, u64> g2;
        BitField<52, 4, u64> g1;

        BitField<56, 4, u64> r2;
        BitField<60, 4, u64> r1;
    } separate;

    const Common::Vec3<u8> GetRGB(unsigned int x, unsigned int y) const {
        int texel = 4 * x + y;

        if (flip)
            std::swap(x, y);

        // Lookup base value
        Common::Vec3<int> ret;
        if (differential_mode) {
            ret.r() = static_cast<int>(differential.r);
            ret.g() = static_cast<int>(differential.g);
            ret.b() = static_cast<int>(differential.b);
            if (x >= 2) {
                ret.r() += static_cast<int>(differential.dr);
                ret.g() += static_cast<int>(differential.dg);
                ret.b() += static_cast<int>(differential.db);
            }
            ret.r() = Common::Color::Convert5To8(ret.r());
            ret.g() = Common::Color::Convert5To8(ret.g());
            ret.b() = Common::Color::Convert5To8(ret.b());
        } else {
            if (x < 2) {
                ret.r() = Common::Color::Convert4To8(static_cast<u8>(separate.r1));
                ret.g() = Common::Color::Convert4To8(static_cast<u8>(separate.g1));
                ret.b() = Common::Color::Convert4To8(static_cast<u8>(separate.b1));
            } else {
                ret.r() = Common::Color::Convert4To8(static_cast<u8>(separate.r2));
                ret.g() = Common::Color::Convert4To8(static_cast<u8>(separate.g2));
                ret.b() = Common::Color::Convert4To8(static_cast<u8>(separate.b2));
            }
        }

        // Add modifier
        unsigned table_index =
            static_cast<int>((x < 2) ? table_index_1.Value() : table_index_2.Value());

        int modifier = etc1_modifier_table[table_index][GetTableSubIndex(texel)];
        if (GetNegationFlag(texel))
            modifier *= -1;

        ret.r() = std::clamp(ret.r() + modifier, 0, 255);
        ret.g() = std::clamp(ret.g() + modifier, 0, 255);
        ret.b() = std::clamp(ret.b() + modifier, 0, 255);

        return ret.Cast<u8>();
    }
};

} // anonymous namespace

Common::Vec3<u8> SampleETC1Subtile(u64 value, unsigned int x, unsigned int y) {
    ETC1Tile tile{value};
    return tile.GetRGB(x, y);
}

} // namespace Pica::Texture
