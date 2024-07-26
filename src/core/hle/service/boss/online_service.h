// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <variant>
#include <vector>

#include "common/common_types.h"
#include "core/hle/result.h"
#include "core/loader/loader.h"

namespace Kernel {
class MappedBuffer;
}

namespace FileSys {
class ArchiveBackend;
struct Entry;
class Path;
} // namespace FileSys

namespace Service::BOSS {

constexpr u32 BOSS_PAYLOAD_HEADER_LENGTH = 0x28;
constexpr u32 BOSS_MAGIC = Loader::MakeMagic('b', 'o', 's', 's');
constexpr u32 BOSS_PAYLOAD_MAGIC = 0x10001;
constexpr u64 NEWS_PROG_ID = 0x0004013000003502;

constexpr u32 BOSS_CONTENT_HEADER_LENGTH = 0x132;
constexpr u32 BOSS_HEADER_WITH_HASH_LENGTH = 0x13C;
constexpr u32 BOSS_ENTIRE_HEADER_LENGTH = BOSS_CONTENT_HEADER_LENGTH + BOSS_HEADER_WITH_HASH_LENGTH;
constexpr u32 BOSS_EXTDATA_HEADER_LENGTH = 0x18;
constexpr u32 BOSS_S_ENTRY_SIZE = 0xC00;
constexpr u32 BOSS_SAVE_HEADER_SIZE = 4;

constexpr std::size_t TASK_ID_SIZE = 8;
constexpr std::size_t URL_SIZE = 0x200;
constexpr std::size_t HEADERS_SIZE = 0x360;
constexpr std::size_t CERTIDLIST_SIZE = 3;
constexpr std::size_t TASKIDLIST_SIZE = 0x400;

constexpr std::array<u8, 8> boss_system_savedata_id{
    0x00, 0x00, 0x00, 0x00, 0x34, 0x00, 0x01, 0x00,
};

#pragma pack(push, 4)
struct BossHeader {
    u8 header_length;
    std::array<u8, 11> zero1;
    u32_be unknown;
    u32_be download_date;
    std::array<u8, 4> zero2;
    u64_be program_id;
    std::array<u8, 4> zero3;
    u32_be datatype;
    u32_be payload_size;
    u32_be ns_data_id;
    u32_be version;
};
#pragma pack(pop)
static_assert(sizeof(BossHeader) == 0x34, "BossHeader has incorrect size");

struct NsDataEntry {
    std::string filename;
    BossHeader header;
};

enum class NsDataHeaderInfoType : u8 {
    ProgramId = 0,
    Unknown = 1,
    Datatype = 2,
    PayloadSize = 3,
    NsDataId = 4,
    Version = 5,
    Everything = 6,
};

struct NsDataHeaderInfo {
    u64 program_id;
    INSERT_PADDING_BYTES(4);
    u32 datatype;
    u32 payload_size;
    u32 ns_data_id;
    u32 version;
    INSERT_PADDING_BYTES(4);
};
static_assert(sizeof(NsDataHeaderInfo) == 0x20, "NsDataHeaderInfo has incorrect size");

enum class PropertyID : u16 {
    Interval = 0x03,
    Duration = 0x04,
    Url = 0x07,
    Headers = 0x0D,
    CertId = 0x0E,
    CertIdList = 0x0F,
    LoadCert = 0x10,
    LoadRootCert = 0x11,
    TotalTasks = 0x35,
    TaskIdList = 0x36,
};

struct BossSVData {
    INSERT_PADDING_BYTES(0x10);
    u64 program_id;
    std::array<char, TASK_ID_SIZE> task_id;
};

struct BossSSData {
    INSERT_PADDING_BYTES(0x21C);
    std::array<u8, URL_SIZE> url;
};

using BossTaskProperty = std::variant<u8, u16, u32, u64, std::vector<u8>, std::vector<u32>>;
struct BossTaskProperties {
    bool task_result;
    std::map<PropertyID, BossTaskProperty> properties{
        {static_cast<PropertyID>(0x00), u8()},
        {static_cast<PropertyID>(0x01), u8()},
        {static_cast<PropertyID>(0x02), u32()},
        {PropertyID::Interval, u32()},
        {PropertyID::Duration, u32()},
        {static_cast<PropertyID>(0x05), u8()},
        {static_cast<PropertyID>(0x06), u8()},
        {PropertyID::Url, std::vector<u8>(URL_SIZE)},
        {static_cast<PropertyID>(0x08), u32()},
        {static_cast<PropertyID>(0x09), u8()},
        {static_cast<PropertyID>(0x0A), std::vector<u8>(0x100)},
        {static_cast<PropertyID>(0x0B), std::vector<u8>(0x200)},
        {static_cast<PropertyID>(0x0C), u32()},
        {PropertyID::Headers, std::vector<u8>(HEADERS_SIZE)},
        {PropertyID::CertId, u32()},
        {PropertyID::CertIdList, std::vector<u32>(CERTIDLIST_SIZE)},
        {PropertyID::LoadCert, u8()},
        {PropertyID::LoadRootCert, u8()},
        {static_cast<PropertyID>(0x12), u8()},
        {static_cast<PropertyID>(0x13), u32()},
        {static_cast<PropertyID>(0x14), u32()},
        {static_cast<PropertyID>(0x15), std::vector<u8>(0x40)},
        {static_cast<PropertyID>(0x16), u32()},
        {static_cast<PropertyID>(0x18), u8()},
        {static_cast<PropertyID>(0x19), u8()},
        {static_cast<PropertyID>(0x1A), u8()},
        {static_cast<PropertyID>(0x1B), u32()},
        {static_cast<PropertyID>(0x1C), u32()},
        {static_cast<PropertyID>(0x1D), u8()},
        {static_cast<PropertyID>(0x1E), u8()},
        {static_cast<PropertyID>(0x1F), u8()},
        {static_cast<PropertyID>(0x20), u8()},
        {static_cast<PropertyID>(0x21), u8()},
        {static_cast<PropertyID>(0x22), u8()},
        {static_cast<PropertyID>(0x23), u32()},
        {static_cast<PropertyID>(0x24), u8()},
        {static_cast<PropertyID>(0x25), u32()},
        {static_cast<PropertyID>(0x26), u32()},
        {static_cast<PropertyID>(0x27), u32()},
        {static_cast<PropertyID>(0x28), u64()},
        {static_cast<PropertyID>(0x29), u64()},
        {static_cast<PropertyID>(0x2A), u32()},
        {static_cast<PropertyID>(0x2B), u32()},
        {static_cast<PropertyID>(0x2C), u8()},
        {static_cast<PropertyID>(0x2D), u16()},
        {static_cast<PropertyID>(0x2E), u16()},
        {static_cast<PropertyID>(0x2F), std::vector<u8>(0x40)},
        {PropertyID::TotalTasks, u16()},
        {PropertyID::TaskIdList, std::vector<u8>(TASKIDLIST_SIZE)},
        {static_cast<PropertyID>(0x3B), u32()},
        {static_cast<PropertyID>(0x3E), std::vector<u8>(0x200)},
        {static_cast<PropertyID>(0x3F), u8()},
    };

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

class OnlineService final {
public:
    explicit OnlineService(u64 program_id_, u64 extdata_id_);

    Result InitializeSession(u64 init_program_id);
    void RegisterTask(const u32 size, Kernel::MappedBuffer& buffer);
    Result UnregisterTask(const u32 size, Kernel::MappedBuffer& buffer);
    void GetTaskIdList();
    u16 GetNsDataIdList(const u32 filter, const u32 max_entries, Kernel::MappedBuffer& buffer);
    std::optional<NsDataEntry> GetNsDataEntryFromId(const u32 ns_data_id);
    Result GetNsDataHeaderInfo(const u32 ns_data_id, const NsDataHeaderInfoType type,
                               const u32 size, Kernel::MappedBuffer& buffer);
    ResultVal<std::size_t> ReadNsData(const u32 ns_data_id, const u64 offset, const u32 size,
                                      Kernel::MappedBuffer& buffer);
    Result SendProperty(const u16 id, const u32 size, Kernel::MappedBuffer& buffer);
    Result ReceiveProperty(const u16 id, const u32 size, Kernel::MappedBuffer& buffer);

private:
    std::unique_ptr<FileSys::ArchiveBackend> OpenBossExtData();
    std::vector<FileSys::Entry> GetBossExtDataFiles(FileSys::ArchiveBackend* boss_archive);
    FileSys::Path GetBossDataDir();
    std::vector<NsDataEntry> GetNsDataEntries();

    BossTaskProperties current_props;
    std::map<std::string, BossTaskProperties> task_id_list;

    u64 program_id;
    u64 extdata_id;

    // For serialization
    explicit OnlineService() = default;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

} // namespace Service::BOSS
