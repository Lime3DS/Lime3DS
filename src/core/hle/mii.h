// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/serialization/export.hpp>
#include "common/bit_field.h"
#include "common/common_types.h"

namespace Mii {

using Nickname = std::array<char16_t, 10>;

#pragma pack(push, 1)
// Reference: https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/mii.h
struct MiiData {
    u8 version; ///< Always 3?

    /// Mii options
    union {
        u8 raw;

        BitField<0, 1, u8> allow_copying;   ///< True if copying is allowed
        BitField<1, 1, u8> is_private_name; ///< Private name?
        BitField<2, 2, u8> region_lock;     ///< Region lock (0=no lock, 1=JPN, 2=USA, 3=EUR)
        BitField<4, 2, u8> char_set;        ///< Character set (0=JPN+USA+EUR, 1=CHN, 2=KOR, 3=TWN)
    } mii_options;

    /// Mii position in Mii selector or Mii maker
    union {
        u8 raw;

        BitField<0, 4, u8> page_index; ///< Page index of Mii
        BitField<4, 4, u8> slot_index; ///< Slot offset of Mii on its Page
    } mii_pos;

    /// Console Identity
    union {
        u8 raw;

        BitField<0, 4, u8> unknown0; ///< Mabye padding (always seems to be 0)?
        BitField<4, 3, u8>
            origin_console; ///< Console that the Mii was created on (1=WII, 2=DSI, 3=3DS)
    } console_identity;

    u64_be system_id;      ///< Identifies the system that the Mii was created on (Determines pants)
    u32_be mii_id;         ///< ID of Mii
    std::array<u8, 6> mac; ///< Creator's system's full MAC address
    u16 pad;               ///< Padding

    /// Mii details
    union {
        u16_be raw;

        BitField<0, 1, u16> gender;          ///< Gender of Mii (0=Male, 1=Female)
        BitField<1, 4, u16> bday_month;      ///< Month of Mii's birthday
        BitField<5, 5, u16> bday_day;        ///< Day of Mii's birthday
        BitField<10, 4, u16> favorite_color; ///< Color of Mii's shirt
        BitField<14, 1, u16> favorite;       ///< Whether the Mii is one of your 10 favorite Mii's
    } mii_details;

    Nickname mii_name; ///< Name of Mii (Encoded using UTF16)
    u8 height;         ///< How tall the Mii is
    u8 width;          ///< How wide the Mii is

    /// Face style
    union {
        u8 raw;

        BitField<0, 1, u8> disable_sharing; ///< Whether or not Sharing of the Mii is allowed
        BitField<1, 4, u8> type;            ///< Face type
        BitField<5, 3, u8> skin_color;      ///< Color of skin
    } face_style;

    /// Face details
    union {
        u8 raw;

        BitField<0, 4, u8> wrinkles;
        BitField<4, 4, u8> makeup;
    } face_details;

    u8 hair_style;

    /// Hair details
    union {
        u8 raw;

        BitField<0, 3, u8> color;
        BitField<3, 1, u8> flip;
    } hair_details;

    /// Eye details
    union {
        u32_be raw;

        BitField<0, 6, u32> type;
        BitField<6, 3, u32> color;
        BitField<9, 4, u32> scale;
        BitField<13, 3, u32> aspect;
        BitField<16, 5, u32> rotate;
        BitField<21, 4, u32> x;
        BitField<25, 5, u32> y;
    } eye_details;

    /// Eyebrow details
    union {
        u32_be raw;

        BitField<0, 5, u32> style;
        BitField<5, 3, u32> color;
        BitField<8, 4, u32> scale;
        BitField<12, 3, u32> aspect;
        BitField<16, 5, u32> rotate;
        BitField<21, 4, u32> x;
        BitField<25, 5, u32> y;
    } eyebrow_details;

    /// Nose details
    union {
        u16_be raw;

        BitField<0, 5, u16> type;
        BitField<5, 4, u16> scale;
        BitField<9, 5, u16> y;
    } nose_details;

    /// Mouth details
    union {
        u16_be raw;

        BitField<0, 6, u16> type;
        BitField<6, 3, u16> color;
        BitField<9, 4, u16> scale;
        BitField<13, 3, u16> aspect;
    } mouth_details;

    /// Mustache details
    union {
        u16_be raw;

        BitField<0, 5, u16> mouth_y;
        BitField<5, 3, u16> mustache_type;
    } mustache_details;

    /// Beard details
    union {
        u16_be raw;

        BitField<0, 3, u16> type;
        BitField<3, 3, u16> color;
        BitField<6, 4, u16> scale;
        BitField<10, 5, u16> y;
    } beard_details;

    /// Glasses details
    union {
        u16_be raw;

        BitField<0, 4, u16> type;
        BitField<4, 3, u16> color;
        BitField<7, 4, u16> scale;
        BitField<11, 5, u16> y;
    } glasses_details;

    /// Mole details
    union {
        u16_be raw;

        BitField<0, 1, u16> type;
        BitField<1, 4, u16> scale;
        BitField<5, 5, u16> x;
        BitField<10, 5, u16> y;
    } mole_details;

    Nickname author_name; ///< Name of Mii's author (Encoded using UTF16)
private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

static_assert(sizeof(MiiData) == 0x5C, "MiiData structure has incorrect size");
static_assert(std::is_trivial_v<MiiData>, "MiiData must be trivial.");
static_assert(std::is_trivially_copyable_v<MiiData>, "MiiData must be trivially copyable.");

struct ChecksummedMiiData {
private:
    MiiData mii_data;
    u16 padding;
    u16_be crc16;

public:
    ChecksummedMiiData& operator=(const MiiData& data) {
        mii_data = data;
        padding = 0;
        FixChecksum();
        return *this;
    }

    ChecksummedMiiData& operator=(MiiData&& data) {
        mii_data = std::move(data);
        padding = 0;
        FixChecksum();
        return *this;
    }

    void SetMiiData(MiiData data) {
        mii_data = data;
        FixChecksum();
    }

    operator MiiData() const {
        return mii_data;
    }

    bool IsChecksumValid() {
        return crc16 == CalculateChecksum();
    }

    u16 CalculateChecksum();

    void FixChecksum() {
        crc16 = CalculateChecksum();
    }

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};
#pragma pack(pop)
static_assert(sizeof(ChecksummedMiiData) == 0x60,
              "ChecksummedMiiData structure has incorrect size");
static_assert(std::is_trivial_v<ChecksummedMiiData>, "ChecksummedMiiData must be trivial.");
static_assert(std::is_trivially_copyable_v<ChecksummedMiiData>,
              "ChecksummedMiiData must be trivially copyable.");
} // namespace Mii

BOOST_CLASS_EXPORT_KEY(Mii::MiiData)
BOOST_CLASS_EXPORT_KEY(Mii::ChecksummedMiiData)
