// Copyright 2019 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstring>
#include <type_traits>
#include <catch2/catch_test_macros.hpp>
#include "common/bit_field.h"

TEST_CASE("BitField", "[common]") {
    enum class TestEnum : u32 {
        A = 0b10111101,
        B = 0b10101110,
        C = 0b00001111,
    };

    union LEBitField {
        u32_le raw;
        BitField<0, 6, u32> a;
        BitField<6, 4, s32> b;
        BitField<10, 8, TestEnum> c;
        BitField<18, 14, u32> d;
    } le_bitfield;

    union BEBitField {
        u32_be raw;
        BitFieldBE<0, 6, u32> a;
        BitFieldBE<6, 4, s32> b;
        BitFieldBE<10, 8, TestEnum> c;
        BitFieldBE<18, 14, u32> d;
    } be_bitfield;

    static_assert(sizeof(LEBitField) == sizeof(u32));
    static_assert(sizeof(BEBitField) == sizeof(u32));
    static_assert(std::is_trivially_copyable_v<LEBitField>);
    static_assert(std::is_trivially_copyable_v<BEBitField>);

    std::array<u8, 4> raw{{
        0b01101100,
        0b11110110,
        0b10111010,
        0b11101100,
    }};

    std::memcpy(&le_bitfield, &raw, sizeof(raw));
    std::memcpy(&be_bitfield, &raw, sizeof(raw));

    // bit fields: 11101100101110'10111101'1001'101100
    REQUIRE(le_bitfield.raw == 0b11101100'10111010'11110110'01101100);
    REQUIRE(le_bitfield.a == 0b101100);
    REQUIRE(le_bitfield.b == -7); // 1001 as two's complement
    REQUIRE(le_bitfield.c == TestEnum::A);
    REQUIRE(le_bitfield.d == 0b11101100101110);

    le_bitfield.a.Assign(0b000111);
    le_bitfield.b.Assign(-1);
    le_bitfield.c.Assign(TestEnum::C);
    le_bitfield.d.Assign(0b01010101010101);
    std::memcpy(&raw, &le_bitfield, sizeof(raw));
    // bit fields: 01010101010101'00001111'1111'000111
    REQUIRE(le_bitfield.raw == 0b01010101'01010100'00111111'11000111);
    REQUIRE(raw == std::array<u8, 4>{{
                       0b11000111,
                       0b00111111,
                       0b01010100,
                       0b01010101,
                   }});

    // bit fields: 01101100111101'10101110'1011'101100
    REQUIRE(be_bitfield.raw == 0b01101100'11110110'10111010'11101100U);
    REQUIRE(be_bitfield.a == 0b101100);
    REQUIRE(be_bitfield.b == -5); // 1011 as two's complement
    REQUIRE(be_bitfield.c == TestEnum::B);
    REQUIRE(be_bitfield.d == 0b01101100111101);

    be_bitfield.a.Assign(0b000111);
    be_bitfield.b.Assign(-1);
    be_bitfield.c.Assign(TestEnum::C);
    be_bitfield.d.Assign(0b01010101010101);
    std::memcpy(&raw, &be_bitfield, sizeof(raw));
    // bit fields: 01010101010101'00001111'1111'000111
    REQUIRE(be_bitfield.raw == 0b01010101'01010100'00111111'11000111U);
    REQUIRE(raw == std::array<u8, 4>{{
                       0b01010101,
                       0b01010100,
                       0b00111111,
                       0b11000111,
                   }});
}
