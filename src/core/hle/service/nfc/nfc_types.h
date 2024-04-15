// Copyright 2022 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "common/bit_field.h"
#include "common/common_types.h"
#include "core/hle/applets/mii_selector.h"

namespace Service::NFC {
static constexpr std::size_t amiibo_name_length = 0xA;
static constexpr std::size_t application_id_version_offset = 0x1c;
static constexpr std::size_t counter_limit = 0xffff;

enum class ServiceType : u32 {
    User,
    Debug,
    System,
};

enum class CommunicationState : u8 {
    Idle = 0,
    SearchingForAdapter = 1,
    Initialized = 2,
    Active = 3,
};

enum class ConnectionState : u8 {
    Success = 0,
    NoAdapter = 1,
    Lost = 2,
};

enum class DeviceState : u32 {
    NotInitialized = 0,
    Initialized = 1,
    SearchingForTag = 2,
    TagFound = 3,
    TagRemoved = 4,
    TagMounted = 5,
    TagPartiallyMounted = 6, // Validate this one seems to have other name
};

enum class ModelType : u32 {
    Amiibo,
};

enum class MountTarget : u32 {
    None,
    Rom,
    Ram,
    All,
};

enum class AmiiboType : u8 {
    Figure,
    Card,
    Yarn,
};

enum class AmiiboSeries : u8 {
    SuperSmashBros,
    SuperMario,
    ChibiRobo,
    YoshiWoollyWorld,
    Splatoon,
    AnimalCrossing,
    EightBitMario,
    Skylanders,
    Unknown8,
    TheLegendOfZelda,
    ShovelKnight,
    Unknown11,
    Kiby,
    Pokemon,
    MarioSportsSuperstars,
    MonsterHunter,
    BoxBoy,
    Pikmin,
    FireEmblem,
    Metroid,
    Others,
    MegaMan,
    Diablo,
};

enum class TagType : u32 {
    None,
    Type1, // ISO14443A RW 96-2k bytes 106kbit/s
    Type2, // ISO14443A RW/RO 540 bytes 106kbit/s
    Type3, // Sony Felica RW/RO 2k bytes 212kbit/s
    Type4, // ISO14443A RW/RO 4k-32k bytes 424kbit/s
    Type5, // ISO15693 RW/RO 540 bytes 106kbit/s
};

enum class PackedTagType : u8 {
    None,
    Type1, // ISO14443A RW 96-2k bytes 106kbit/s
    Type2, // ISO14443A RW/RO 540 bytes 106kbit/s
    Type3, // Sony Felica RW/RO 2k bytes 212kbit/s
    Type4, // ISO14443A RW/RO 4k-32k bytes 424kbit/s
    Type5, // ISO15693 RW/RO 540 bytes 106kbit/s
};

// Verify this enum. It might be completely wrong default protocol is 0x0
enum class TagProtocol : u32 {
    None,
    TypeA = 1U << 0, // ISO14443A
    TypeB = 1U << 1, // ISO14443B
    TypeF = 1U << 2, // Sony Felica
    Unknown1 = 1U << 3,
    Unknown2 = 1U << 5,
    All = 0xFFFFFFFFU,
};

// Verify this enum. It might be completely wrong default protocol is 0x0
enum class PackedTagProtocol : u8 {
    None,
    TypeA = 1U << 0, // ISO14443A
    TypeB = 1U << 1, // ISO14443B
    TypeF = 1U << 2, // Sony Felica
    Unknown1 = 1U << 3,
    Unknown2 = 1U << 5,
    All = 0xFF,
};

enum class AppAreaVersion : u8 {
    Nintendo3DS = 0,
    NintendoWiiU = 1,
    Nintendo3DSv2 = 2,
    NintendoSwitch = 3,
    NotSet = 0xFF,
};

using UniqueSerialNumber = std::array<u8, 7>;
using LockBytes = std::array<u8, 2>;
using HashData = std::array<u8, 0x20>;
using ApplicationArea = std::array<u8, 0xD8>;
using AmiiboName = std::array<u16_be, amiibo_name_length>;
using DataBlock = std::array<u8, 0x10>;
using KeyData = std::array<u8, 0x6>;

struct TagUuid {
    UniqueSerialNumber uid;
    u8 nintendo_id;
    LockBytes lock_bytes;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & uid;
        ar & nintendo_id;
        ar & lock_bytes;
    }
    friend class boost::serialization::access;
};
static_assert(sizeof(TagUuid) == 10, "TagUuid is an invalid size");

struct WriteDate {
    u16 year;
    u8 month;
    u8 day;
};
static_assert(sizeof(WriteDate) == 0x4, "WriteDate is an invalid size");

struct AmiiboDate {
    u16 raw_date{};

    u16 GetValue() const {
        return Common::swap16(raw_date);
    }

    u16 GetYear() const {
        return static_cast<u16>(((GetValue() & 0xFE00) >> 9) + 2000);
    }
    u8 GetMonth() const {
        return static_cast<u8>((GetValue() & 0x01E0) >> 5);
    }
    u8 GetDay() const {
        return static_cast<u8>(GetValue() & 0x001F);
    }

    WriteDate GetWriteDate() const {
        if (!IsValidDate()) {
            return {
                .year = 2000,
                .month = 1,
                .day = 1,
            };
        }
        return {
            .year = GetYear(),
            .month = GetMonth(),
            .day = GetDay(),
        };
    }

    void SetYear(u16 year) {
        const u16 year_converted = static_cast<u16>((year - 2000) << 9);
        raw_date = Common::swap16((GetValue() & ~0xFE00) | year_converted);
    }
    void SetMonth(u8 month) {
        const u16 month_converted = static_cast<u16>(month << 5);
        raw_date = Common::swap16((GetValue() & ~0x01E0) | month_converted);
    }
    void SetDay(u8 day) {
        const u16 day_converted = static_cast<u16>(day);
        raw_date = Common::swap16((GetValue() & ~0x001F) | day_converted);
    }

    bool IsValidDate() const {
        const bool is_day_valid = GetDay() > 0 && GetDay() < 32;
        const bool is_month_valid = GetMonth() > 0 && GetMonth() < 13;
        const bool is_year_valid = GetYear() >= 2000;
        return is_year_valid && is_month_valid && is_day_valid;
    }
};
static_assert(sizeof(AmiiboDate) == 2, "AmiiboDate is an invalid size");

struct Settings {
    union {
        u8 raw{};

        BitField<0, 4, u8> font_region;
        BitField<4, 1, u8> amiibo_initialized;
        BitField<5, 1, u8> appdata_initialized;
    };
};
static_assert(sizeof(Settings) == 1, "AmiiboDate is an invalid size");

struct AmiiboSettings {
    Settings settings;
    u8 country_code_id;
    u16_be crc_counter; // Incremented each time crc is changed
    AmiiboDate init_date;
    AmiiboDate write_date;
    u32_be crc;
    AmiiboName amiibo_name; // UTF-16 text
};
static_assert(sizeof(AmiiboSettings) == 0x20, "AmiiboSettings is an invalid size");

struct AmiiboModelInfo {
    u16 character_id;
    u8 character_variant;
    AmiiboType amiibo_type;
    u16_be model_number;
    AmiiboSeries series;
    PackedTagType tag_type;
    INSERT_PADDING_BYTES(0x4); // Unknown
};
static_assert(sizeof(AmiiboModelInfo) == 0xC, "AmiiboModelInfo is an invalid size");

struct NTAG215Password {
    u32 PWD;  // Password to allow write access
    u16 PACK; // Password acknowledge reply
    u16 RFUI; // Reserved for future use

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & PWD;
        ar & PACK;
        ar & RFUI;
    }
    friend class boost::serialization::access;
};
static_assert(sizeof(NTAG215Password) == 0x8, "NTAG215Password is an invalid size");

#pragma pack(1)
struct EncryptedAmiiboFile {
    u8 constant_value;                 // Must be A5
    u16_be write_counter;              // Number of times the amiibo has been written?
    u8 amiibo_version;                 // Amiibo file version
    AmiiboSettings settings;           // Encrypted amiibo settings
    HashData hmac_tag;                 // Hash
    AmiiboModelInfo model_info;        // Encrypted amiibo model info
    HashData keygen_salt;              // Salt
    HashData hmac_data;                // Hash
    Mii::ChecksummedMiiData owner_mii; // Encrypted Mii data
    u64_be application_id;             // Encrypted Game id
    u16_be application_write_counter;  // Encrypted Counter
    u32_be application_area_id;        // Encrypted Game id
    u8 application_id_byte;
    u8 unknown;
    u64 mii_extension;
    std::array<u32, 0x5> unknown2;
    u32_be register_info_crc;
    ApplicationArea application_area; // Encrypted Game data
};
static_assert(sizeof(EncryptedAmiiboFile) == 0x1F8, "AmiiboFile is an invalid size");

struct NTAG215File {
    LockBytes lock_bytes;      // Tag UUID
    u16 static_lock;           // Set defined pages as read only
    u32 compability_container; // Defines available memory
    HashData hmac_data;        // Hash
    u8 constant_value;         // Must be A5
    u16_be write_counter;      // Number of times the amiibo has been written?
    u8 amiibo_version;         // Amiibo file version
    AmiiboSettings settings;
    Mii::ChecksummedMiiData owner_mii; // Mii data
    u64_be application_id;             // Game id
    u16_be application_write_counter;  // Counter
    u32_be application_area_id;
    u8 application_id_byte;
    u8 unknown;
    u64 mii_extension;
    std::array<u32, 0x5> unknown2;
    u32_be register_info_crc;
    ApplicationArea application_area; // Game data
    HashData hmac_tag;                // Hash
    UniqueSerialNumber uid;           // Unique serial number
    u8 nintendo_id;                   // Tag UUID
    AmiiboModelInfo model_info;
    HashData keygen_salt;     // Salt
    u32 dynamic_lock;         // Dynamic lock
    u32 CFG0;                 // Defines memory protected by password
    u32 CFG1;                 // Defines number of verification attempts
    NTAG215Password password; // Password data
};
static_assert(sizeof(NTAG215File) == 0x21C, "NTAG215File is an invalid size");
static_assert(std::is_trivially_copyable_v<NTAG215File>, "NTAG215File must be trivially copyable.");
#pragma pack()

struct EncryptedNTAG215File {
    TagUuid uuid;                    // Unique serial number
    u16 static_lock;                 // Set defined pages as read only
    u32 compability_container;       // Defines available memory
    EncryptedAmiiboFile user_memory; // Writable data
    u32 dynamic_lock;                // Dynamic lock
    u32 CFG0;                        // Defines memory protected by password
    u32 CFG1;                        // Defines number of verification attempts
    NTAG215Password password;        // Password data
};
static_assert(sizeof(EncryptedNTAG215File) == 0x21C, "EncryptedNTAG215File is an invalid size");
static_assert(std::is_trivially_copyable_v<EncryptedNTAG215File>,
              "EncryptedNTAG215File must be trivially copyable.");

struct SerializableAmiiboFile {
    union {
        std::array<u8, 0x21C> raw;
        NTAG215File file;
    };
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & raw;
    }
    friend class boost::serialization::access;
};
static_assert(sizeof(SerializableAmiiboFile) == 0x21C, "SerializableAmiiboFile is an invalid size");
static_assert(std::is_trivially_copyable_v<SerializableAmiiboFile>,
              "SerializableAmiiboFile must be trivially copyable.");

struct SerializableEncryptedAmiiboFile {
    union {
        std::array<u8, 0x21C> raw;
        EncryptedNTAG215File file;
    };
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & raw;
    }
    friend class boost::serialization::access;
};
static_assert(sizeof(SerializableEncryptedAmiiboFile) == 0x21C,
              "SerializableEncryptedAmiiboFile is an invalid size");
static_assert(std::is_trivially_copyable_v<SerializableEncryptedAmiiboFile>,
              "SerializableEncryptedAmiiboFile must be trivially copyable.");

struct TagInfo {
    u16 uuid_length;
    PackedTagProtocol protocol;
    PackedTagType tag_type;
    UniqueSerialNumber uuid;
    std::array<u8, 0x21> extra_data;
};
static_assert(sizeof(TagInfo) == 0x2C, "TagInfo is an invalid size");

struct TagInfo2 {
    u16 uuid_length;
    INSERT_PADDING_BYTES(0x1);
    PackedTagType tag_type;
    UniqueSerialNumber uuid;
    std::array<u8, 0x21> extra_data;
    TagProtocol protocol;
    std::array<u8, 0x30> extra_data2;
};
static_assert(sizeof(TagInfo2) == 0x60, "TagInfo2 is an invalid size");

struct CommonInfo {
    WriteDate last_write_date;
    u16 application_write_counter;
    u16 character_id;
    u8 character_variant;
    AmiiboSeries series;
    u16 model_number;
    AmiiboType amiibo_type;
    u8 version;
    u16 application_area_size;
    INSERT_PADDING_BYTES(0x30);
};
static_assert(sizeof(CommonInfo) == 0x40, "CommonInfo is an invalid size");

struct ModelInfo {
    u16 character_id;
    u8 character_variant;
    AmiiboSeries series;
    u16 model_number;
    AmiiboType amiibo_type;
    INSERT_PADDING_BYTES(0x2F);
};
static_assert(sizeof(ModelInfo) == 0x36, "ModelInfo is an invalid size");

struct RegisterInfo {
    Mii::ChecksummedMiiData mii_data;
    AmiiboName amiibo_name;
    INSERT_PADDING_BYTES(0x2); // Zero string terminator
    u8 flags;
    u8 font_region;
    WriteDate creation_date;
    INSERT_PADDING_BYTES(0x2C);
};
static_assert(sizeof(RegisterInfo) == 0xA8, "RegisterInfo is an invalid size");

struct RegisterInfoPrivate {
    Mii::ChecksummedMiiData mii_data;
    AmiiboName amiibo_name;
    INSERT_PADDING_BYTES(0x2); // Zero string terminator
    u8 flags;
    u8 font_region;
    WriteDate creation_date;
    INSERT_PADDING_BYTES(0x28);
};
static_assert(sizeof(RegisterInfoPrivate) == 0xA4, "RegisterInfoPrivate is an invalid size");
static_assert(std::is_trivial_v<RegisterInfoPrivate>, "RegisterInfoPrivate must be trivial.");
static_assert(std::is_trivially_copyable_v<RegisterInfoPrivate>,
              "RegisterInfoPrivate must be trivially copyable.");

struct AdminInfo {
    u64_be application_id;
    u32_be application_area_id;
    u16 crc_counter;
    u8 flags;
    PackedTagType tag_type;
    AppAreaVersion app_area_version;
    INSERT_PADDING_BYTES(0x7);
    INSERT_PADDING_BYTES(0x28);
};
static_assert(sizeof(AdminInfo) == 0x40, "AdminInfo is an invalid size");

} // namespace Service::NFC
