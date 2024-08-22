// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <tuple>
#include <boost/serialization/array.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#include <fmt/ranges.h>
#include "common/archives.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/core.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/result.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/cfg/cfg_defaults.h"
#include "core/hle/service/cfg/cfg_i.h"
#include "core/hle/service/cfg/cfg_nor.h"
#include "core/hle/service/cfg/cfg_s.h"
#include "core/hle/service/cfg/cfg_u.h"
#include "core/loader/loader.h"

SERVICE_CONSTRUCT_IMPL(Service::CFG::Module)
SERIALIZE_EXPORT_IMPL(Service::CFG::Module)

namespace Service::CFG {

template <class Archive>
void Module::serialize(Archive& ar, const unsigned int) {
    ar & cfg_config_file_buffer;
    ar & cfg_system_save_data_archive;
    ar & preferred_region_code;
    ar & preferred_region_chosen;
}
SERIALIZE_IMPL(Module)

/// The maximum number of block entries that can exist in the config file
constexpr u32 CONFIG_FILE_MAX_BLOCK_ENTRIES = 1479;

namespace {

/// Block header in the config savedata file
struct SaveConfigBlockEntry {
    u32 block_id;       ///< The id of the current block
    u32 offset_or_data; ///< This is the absolute offset to the block data if the size is greater
    /// than 4 bytes, otherwise it contains the data itself
    u16 size;                ///< The size of the block
    AccessFlag access_flags; ///< The access control flags of the block
};

/**
 * The header of the config savedata file,
 * contains information about the blocks in the file
 */
struct SaveFileConfig {
    u16 total_entries;       ///< The total number of set entries in the config file
    u16 data_entries_offset; ///< The offset where the data for the blocks start, this is hardcoded
                             /// to 0x455C as per hardware
    SaveConfigBlockEntry block_entries[CONFIG_FILE_MAX_BLOCK_ENTRIES]; ///< The block headers, the
                                                                       /// maximum possible value is
    /// 1479 as per hardware
    u32 unknown; ///< This field is unknown, possibly padding, 0 has been observed in hardware
};
static_assert(sizeof(SaveFileConfig) == 0x455C,
              "SaveFileConfig header must be exactly 0x455C bytes");

} // namespace

static constexpr u16 C(const char code[2]) {
    return code[0] | (code[1] << 8);
}

static const std::array<u16, 187> country_codes = {{
    0,       C("JP"), 0,       0,       0,       0,       0,       0,       // 0-7
    C("AI"), C("AG"), C("AR"), C("AW"), C("BS"), C("BB"), C("BZ"), C("BO"), // 8-15
    C("BR"), C("VG"), C("CA"), C("KY"), C("CL"), C("CO"), C("CR"), C("DM"), // 16-23
    C("DO"), C("EC"), C("SV"), C("GF"), C("GD"), C("GP"), C("GT"), C("GY"), // 24-31
    C("HT"), C("HN"), C("JM"), C("MQ"), C("MX"), C("MS"), C("AN"), C("NI"), // 32-39
    C("PA"), C("PY"), C("PE"), C("KN"), C("LC"), C("VC"), C("SR"), C("TT"), // 40-47
    C("TC"), C("US"), C("UY"), C("VI"), C("VE"), 0,       0,       0,       // 48-55
    0,       0,       0,       0,       0,       0,       0,       0,       // 56-63
    C("AL"), C("AU"), C("AT"), C("BE"), C("BA"), C("BW"), C("BG"), C("HR"), // 64-71
    C("CY"), C("CZ"), C("DK"), C("EE"), C("FI"), C("FR"), C("DE"), C("GR"), // 72-79
    C("HU"), C("IS"), C("IE"), C("IT"), C("LV"), C("LS"), C("LI"), C("LT"), // 80-87
    C("LU"), C("MK"), C("MT"), C("ME"), C("MZ"), C("NA"), C("NL"), C("NZ"), // 88-95
    C("NO"), C("PL"), C("PT"), C("RO"), C("RU"), C("RS"), C("SK"), C("SI"), // 96-103
    C("ZA"), C("ES"), C("SZ"), C("SE"), C("CH"), C("TR"), C("GB"), C("ZM"), // 104-111
    C("ZW"), C("AZ"), C("MR"), C("ML"), C("NE"), C("TD"), C("SD"), C("ER"), // 112-119
    C("DJ"), C("SO"), C("AD"), C("GI"), C("GG"), C("IM"), C("JE"), C("MC"), // 120-127
    C("TW"), 0,       0,       0,       0,       0,       0,       0,       // 128-135
    C("KR"), 0,       0,       0,       0,       0,       0,       0,       // 136-143
    C("HK"), C("MO"), 0,       0,       0,       0,       0,       0,       // 144-151
    C("ID"), C("SG"), C("TH"), C("PH"), C("MY"), 0,       0,       0,       // 152-159
    C("CN"), 0,       0,       0,       0,       0,       0,       0,       // 160-167
    C("AE"), C("IN"), C("EG"), C("OM"), C("QA"), C("KW"), C("SA"), C("SY"), // 168-175
    C("BH"), C("JO"), 0,       0,       0,       0,       0,       0,       // 176-183
    C("SM"), C("VA"), C("BM"),                                              // 184-186
}};

// Based on PKHeX's lists of subregions at
// https://github.com/kwsch/PKHeX/tree/master/PKHeX.Core/Resources/text/locale3DS/subregions
static const std::array<u8, 187> default_subregion = {{
    0, 2, 0,  0, 0, 0, 0, 0, // 0-7
    1, 2, 2,  1, 1, 1, 2, 2, // 8-15
    2, 1, 2,  1, 2, 2, 2, 1, // 16-23
    2, 2, 2,  1, 1, 1, 2, 2, // 24-31
    2, 2, 2,  1, 2, 1, 1, 2, // 32-39
    2, 2, 2,  2, 1, 1, 2, 2, // 40-47
    1, 2, 2,  1, 2, 0, 0, 0, // 48-55
    0, 0, 0,  0, 0, 0, 0, 0, // 56-63
    2, 2, 2,  2, 2, 1, 2, 6, // 64-71
    1, 2, 18, 1, 8, 2, 2, 2, // 72-79
    2, 1, 2,  2, 1, 2, 1, 2, // 80-87
    1, 1, 1,  1, 1, 1, 2, 2, // 88-95
    7, 2, 2,  2, 9, 1, 2, 1, // 96-103
    2, 2, 2,  2, 2, 2, 2, 1, // 104-111
    1, 1, 1,  1, 1, 1, 1, 1, // 112-119
    1, 1, 1,  1, 1, 1, 1, 1, // 120-127
    2, 0, 0,  0, 0, 0, 0, 0, // 128-135
    2, 0, 0,  0, 0, 0, 0, 0, // 136-143
    1, 0, 0,  0, 0, 0, 0, 0, // 144-151
    0, 1, 0,  0, 2, 0, 0, 0, // 152-159
    2, 0, 0,  0, 0, 0, 0, 0, // 160-167
    2, 2, 0,  0, 0, 0, 2, 0, // 168-175
    0, 0, 0,  0, 0, 0, 0, 0, // 176-183
    1, 1, 1,                 // 184-186
}};
std::array<u8, 2> unknown;

constexpr std::array<u8, 8> cfg_system_savedata_id{
    0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x01, 0x00,
};

Module::Interface::Interface(std::shared_ptr<Module> cfg, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), cfg(std::move(cfg)) {}

Module::Interface::~Interface() = default;

void Module::Interface::GetCountryCodeString(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u16 country_code_id = rp.Pop<u16>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    if (country_code_id >= country_codes.size() || 0 == country_codes[country_code_id]) {
        LOG_ERROR(Service_CFG, "requested country code id={} is invalid", country_code_id);
        rb.Push(Result(ErrorDescription::NotFound, ErrorModule::Config, ErrorSummary::WrongArgument,
                       ErrorLevel::Permanent));
        rb.Skip(1, false);
        return;
    }

    rb.Push(ResultSuccess);
    // the real CFG service copies only three bytes (including the null-terminator) here
    rb.Push<u32>(country_codes[country_code_id]);
}

std::shared_ptr<Module> Module::Interface::Interface::GetModule() const {
    return cfg;
}

void Module::Interface::GetCountryCodeID(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u16 country_code = rp.Pop<u16>();
    u16 country_code_id = 0;

    // The following algorithm will fail if the first country code isn't 0.
    DEBUG_ASSERT(country_codes[0] == 0);

    for (u16 id = 0; id < country_codes.size(); ++id) {
        if (country_codes[id] == country_code) {
            country_code_id = id;
            break;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    if (0 == country_code_id) {
        LOG_ERROR(Service_CFG, "requested country code name={}{} is invalid",
                  static_cast<char>(country_code & 0xff), static_cast<char>(country_code >> 8));
        rb.Push(Result(ErrorDescription::NotFound, ErrorModule::Config, ErrorSummary::WrongArgument,
                       ErrorLevel::Permanent));
        rb.Push<u16>(0x00FF);
        return;
    }

    rb.Push(ResultSuccess);
    rb.Push<u16>(country_code_id);
}

u32 Module::GetRegionValue() {
    if (Settings::values.region_value.GetValue() == Settings::REGION_VALUE_AUTO_SELECT) {
        UpdatePreferredRegionCode();
        return preferred_region_code;
    }

    return Settings::values.region_value.GetValue();
}

void Module::Interface::GetRegion(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push<u8>(static_cast<u8>(cfg->GetRegionValue()));
}

void Module::Interface::SecureInfoGetByte101(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_DEBUG(Service_CFG, "(STUBBED) called");

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    // According to 3dbrew this is normally 0.
    rb.Push<u8>(0);
}

void Module::Interface::SetUUIDClockSequence(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    cfg->mcu_data.clock_sequence = rp.Pop<u16>();
    cfg->SaveMCUConfig();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void Module::Interface::GetUUIDClockSequence(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push<u16>(static_cast<u16>(cfg->mcu_data.clock_sequence));
}

void Module::Interface::GetTransferableId(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 app_id_salt = rp.Pop<u32>() & 0x000FFFFF;

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);

    std::array<u8, 12> buffer;
    const Result result =
        cfg->GetConfigBlock(ConsoleUniqueID2BlockID, 8, AccessFlag::Global, buffer.data());
    rb.Push(result);
    if (result.IsSuccess()) {
        std::memcpy(&buffer[8], &app_id_salt, sizeof(u32));
        std::array<u8, CryptoPP::SHA256::DIGESTSIZE> hash;
        CryptoPP::SHA256().CalculateDigest(hash.data(), buffer.data(), sizeof(buffer));
        u32 low, high;
        std::memcpy(&low, &hash[hash.size() - 8], sizeof(u32));
        std::memcpy(&high, &hash[hash.size() - 4], sizeof(u32));
        rb.Push(low);
        rb.Push(high);
    } else {
        rb.Push<u32>(0);
        rb.Push<u32>(0);
    }

    LOG_DEBUG(Service_CFG, "called app_id_salt=0x{:X}", app_id_salt);
}

void Module::Interface::IsCoppacsSupported(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    rb.Push(ResultSuccess);

    u8 canada_or_usa = 1;
    if (canada_or_usa == cfg->GetRegionValue()) {
        rb.Push(true);
    } else {
        rb.Push(false);
    }
}

void Module::Interface::GetSystemModel(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    u32 data{};

    // TODO(Subv): Find out the correct error codes
    rb.Push(cfg->GetConfigBlock(ConsoleModelBlockID, 4, AccessFlag::SystemRead,
                                reinterpret_cast<u8*>(&data)));
    ConsoleModelInfo model;
    std::memcpy(&model, &data, 4);
    if ((model.model == NINTENDO_3DS || model.model == NINTENDO_3DS_XL ||
         model.model == NINTENDO_2DS) &&
        Settings::values.is_new_3ds) {
        model.model = NEW_NINTENDO_3DS_XL;
    } else if ((model.model == NEW_NINTENDO_3DS || model.model == NEW_NINTENDO_3DS_XL ||
                model.model == NEW_NINTENDO_2DS_XL) &&
               !Settings::values.is_new_3ds) {
        model.model = NINTENDO_3DS_XL;
    }
    std::memcpy(&data, &model, 4);
    rb.Push<u8>(data & 0xFF);
}

void Module::Interface::GetModelNintendo2DS(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    u32 data{};

    // TODO(Subv): Find out the correct error codes
    rb.Push(cfg->GetConfigBlock(ConsoleModelBlockID, 4, AccessFlag::SystemRead,
                                reinterpret_cast<u8*>(&data)));
    u8 model = data & 0xFF;
    rb.Push(model != Service::CFG::NINTENDO_2DS);
}

void Module::Interface::GetConfig(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 size = rp.Pop<u32>();
    u32 block_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    std::vector<u8> data(size);
    rb.Push(cfg->GetConfigBlock(block_id, size, AccessFlag::UserRead, data.data()));
    buffer.Write(data.data(), 0, data.size());
    rb.PushMappedBuffer(buffer);
}

void Module::Interface::GetSystemConfig(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 size = rp.Pop<u32>();
    u32 block_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    std::vector<u8> data(size);
    rb.Push(cfg->GetConfigBlock(block_id, size, AccessFlag::SystemRead, data.data()));
    buffer.Write(data.data(), 0, data.size());
    rb.PushMappedBuffer(buffer);
}

void Module::Interface::SetSystemConfig(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 block_id = rp.Pop<u32>();
    u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    std::vector<u8> data(size);
    buffer.Read(data.data(), 0, data.size());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(cfg->SetConfigBlock(block_id, size, AccessFlag::SystemWrite, data.data()));
    rb.PushMappedBuffer(buffer);
}

void Module::Interface::UpdateConfigNANDSavegame(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(cfg->UpdateConfigNANDSavegame());
}

void Module::Interface::FormatConfig(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(cfg->FormatConfig());
}

ResultVal<void*> Module::GetConfigBlockPointer(u32 block_id, u32 size, AccessFlag accesss_flag) {
    // Read the header
    auto config = reinterpret_cast<SaveFileConfig*>(cfg_config_file_buffer.data());
    auto itr =
        std::find_if(std::begin(config->block_entries), std::end(config->block_entries),
                     [&](const SaveConfigBlockEntry& entry) { return entry.block_id == block_id; });

    if (itr == std::end(config->block_entries)) {
        if (HasDefaultConfigBlock(static_cast<ConfigBlockID>(block_id))) {
            LOG_WARNING(Service_CFG,
                        "Config block 0x{:X} with flags {} and size {} was not found, creating "
                        "from defaults.",
                        block_id, accesss_flag, size);
            const auto& default_block = GetDefaultConfigBlock(static_cast<ConfigBlockID>(block_id));
            auto result = CreateConfigBlock(block_id, static_cast<u16>(default_block.data.size()),
                                            default_block.access_flags, default_block.data.data());
            if (!result.IsSuccess()) {
                LOG_ERROR(Service_CFG,
                          "Failed to create config block 0x{:X} from defaults: 0x{:08X}", block_id,
                          result.raw);
                return result;
            }
            result = UpdateConfigNANDSavegame();
            if (!result.IsSuccess()) {
                LOG_ERROR(Service_CFG, "Failed to save updated config savegame: 0x{:08X}",
                          result.raw);
                return result;
            }
            itr = std::find_if(
                std::begin(config->block_entries), std::end(config->block_entries),
                [&](const SaveConfigBlockEntry& entry) { return entry.block_id == block_id; });
        } else {
            LOG_ERROR(Service_CFG,
                      "Config block 0x{:X} with flags {} and size {} was not found, and no default "
                      "exists.",
                      block_id, accesss_flag, size);
            return Result(ErrorDescription::NotFound, ErrorModule::Config,
                          ErrorSummary::WrongArgument, ErrorLevel::Permanent);
        }
    }

    if (False(itr->access_flags & accesss_flag)) {
        LOG_ERROR(Service_CFG, "Invalid access flag {:X} for config block 0x{:X} with size {}",
                  accesss_flag, block_id, size);
        return Result(ErrorDescription::NotAuthorized, ErrorModule::Config,
                      ErrorSummary::WrongArgument, ErrorLevel::Permanent);
    }

    if (itr->size != size) {
        LOG_ERROR(Service_CFG, "Invalid size {} for config block 0x{:X} with flags {}", size,
                  block_id, accesss_flag);
        return Result(ErrorDescription::InvalidSize, ErrorModule::Config,
                      ErrorSummary::WrongArgument, ErrorLevel::Permanent);
    }

    void* pointer;

    // The data is located in the block header itself if the size is less than 4 bytes
    if (itr->size <= 4)
        pointer = &itr->offset_or_data;
    else
        pointer = &cfg_config_file_buffer[itr->offset_or_data];

    return pointer;
}

Result Module::GetConfigBlock(u32 block_id, u32 size, AccessFlag accesss_flag, void* output) {
    bool get_from_artic =
        block_id == ConsoleUniqueID2BlockID &&
        (static_cast<u16>(accesss_flag) & static_cast<u16>(AccessFlag::UserRead)) != 0;

    if (get_from_artic && artic_client.get()) {
        auto req = artic_client->NewRequest("CFGU_GetConfigInfoBlk2");

        req.AddParameterS32(block_id);
        req.AddParameterU32(size);

        auto resp = artic_client->Send(req);

        if (!resp.has_value() || !resp->Succeeded())
            return Result(-1);

        auto res = Result(static_cast<u32>(resp->GetMethodResult()));
        if (res.IsError())
            return res;

        auto buff = resp->GetResponseBuffer(0);
        if (!buff.has_value())
            return Result(-1);
        size_t actually_read = buff->second;
        if (actually_read > size)
            return Result(-1);

        memcpy(output, buff->first, actually_read);
        return ResultSuccess;

    } else {
        void* pointer = nullptr;
        CASCADE_RESULT(pointer, GetConfigBlockPointer(block_id, size, accesss_flag));
        std::memcpy(output, pointer, size);

        return ResultSuccess;
    }
}

Result Module::SetConfigBlock(u32 block_id, u32 size, AccessFlag accesss_flag, const void* input) {
    void* pointer = nullptr;
    CASCADE_RESULT(pointer, GetConfigBlockPointer(block_id, size, accesss_flag));
    std::memcpy(pointer, input, size);
    return ResultSuccess;
}

Result Module::CreateConfigBlock(u32 block_id, u16 size, AccessFlag access_flags,
                                 const void* data) {
    SaveFileConfig* config = reinterpret_cast<SaveFileConfig*>(cfg_config_file_buffer.data());
    if (config->total_entries >= CONFIG_FILE_MAX_BLOCK_ENTRIES)
        return ResultUnknown; // TODO(Subv): Find the right error code

    // Insert the block header with offset 0 for now
    config->block_entries[config->total_entries] = {block_id, 0, size, access_flags};
    if (size > 4) {
        u32 offset = config->data_entries_offset;
        // Perform a search to locate the next offset for the new data
        // use the offset and size of the previous block to determine the new position
        for (int i = config->total_entries - 1; i >= 0; --i) {
            // Ignore the blocks that don't have a separate data offset
            if (config->block_entries[i].size > 4) {
                offset = config->block_entries[i].offset_or_data + config->block_entries[i].size;
                break;
            }
        }

        config->block_entries[config->total_entries].offset_or_data = offset;

        // Write the data at the new offset
        std::memcpy(&cfg_config_file_buffer[offset], data, size);
    } else {
        // The offset_or_data field in the header contains the data itself if it's 4 bytes or less
        std::memcpy(&config->block_entries[config->total_entries].offset_or_data, data, size);
    }

    ++config->total_entries;
    return ResultSuccess;
}

Result Module::DeleteConfigNANDSaveFile() {
    FileSys::Path path("/config");
    return cfg_system_save_data_archive->DeleteFile(path);
}

Result Module::UpdateConfigNANDSavegame() {
    FileSys::Mode mode = {};
    mode.write_flag.Assign(1);
    mode.create_flag.Assign(1);

    FileSys::Path path("/config");

    auto config_result = cfg_system_save_data_archive->OpenFile(path, mode);
    ASSERT_MSG(config_result.Succeeded(), "could not open file");

    auto config = std::move(config_result).Unwrap();
    config->Write(0, CONFIG_SAVEFILE_SIZE, true, false, cfg_config_file_buffer.data());

    return ResultSuccess;
}

Result Module::FormatConfig() {
    Result res = DeleteConfigNANDSaveFile();
    // The delete command fails if the file doesn't exist, so we have to check that too
    if (!res.IsSuccess() && res != FileSys::ResultFileNotFound) {
        return res;
    }
    // Delete the old data
    cfg_config_file_buffer.fill(0);
    // Create the header
    SaveFileConfig* config = reinterpret_cast<SaveFileConfig*>(cfg_config_file_buffer.data());
    // This value is hardcoded, taken from 3dbrew, verified by hardware, it's always the same value
    config->data_entries_offset = 0x455C;

    // Fill the config with default block data.
    auto default_blocks = GetDefaultConfigBlocks();
    for (auto& entry : default_blocks) {
        res = CreateConfigBlock(entry.first, static_cast<u16>(entry.second.data.size()),
                                entry.second.access_flags, entry.second.data.data());
        if (!res.IsSuccess()) {
            return res;
        }
    }

    // Generate a new console unique ID.
    const auto [random_number, console_id] = GenerateConsoleUniqueId();
    SetConsoleUniqueId(random_number, console_id);

    // Save the buffer to the file
    res = UpdateConfigNANDSavegame();
    if (!res.IsSuccess()) {
        return res;
    }

    return ResultSuccess;
}

Result Module::LoadConfigNANDSaveFile() {
    const std::string& nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
    FileSys::ArchiveFactory_SystemSaveData systemsavedata_factory(nand_directory);

    // Open the SystemSaveData archive 0x00010017
    FileSys::Path archive_path(cfg_system_savedata_id);
    auto archive_result = systemsavedata_factory.Open(archive_path, 0);

    // If the archive didn't exist, create the files inside
    if (archive_result.Code() == FileSys::ResultNotFound) {
        // Format the archive to create the directories
        systemsavedata_factory.Format(archive_path, FileSys::ArchiveFormatInfo(), 0, 0, 0);

        // Open it again to get a valid archive now that the folder exists
        cfg_system_save_data_archive = systemsavedata_factory.Open(archive_path, 0).Unwrap();
    } else {
        ASSERT_MSG(archive_result.Succeeded(), "Could not open the CFG SystemSaveData archive!");

        cfg_system_save_data_archive = std::move(archive_result).Unwrap();
    }

    FileSys::Path config_path("/config");
    FileSys::Mode open_mode = {};
    open_mode.read_flag.Assign(1);

    auto config_result = cfg_system_save_data_archive->OpenFile(config_path, open_mode);

    // Read the file if it already exists
    if (config_result.Succeeded()) {
        auto config = std::move(config_result).Unwrap();
        config->Read(0, CONFIG_SAVEFILE_SIZE, cfg_config_file_buffer.data());
        return ResultSuccess;
    }

    return FormatConfig();
}

void Module::LoadMCUConfig() {
    FileUtil::IOFile mcu_data_file(
        fmt::format("{}/mcu.dat", FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir)), "rb");

    if (mcu_data_file.IsOpen() && mcu_data_file.GetSize() >= sizeof(MCUData) &&
        mcu_data_file.ReadBytes(&mcu_data, sizeof(MCUData)) == sizeof(MCUData)) {
        if (mcu_data.IsValid()) {
            return;
        }
    }
    mcu_data_file.Close();
    mcu_data = MCUData();
    SaveMCUConfig();
}

void Module::SaveMCUConfig() {
    FileUtil::IOFile mcu_data_file(
        fmt::format("{}/mcu.dat", FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir)), "wb");

    if (mcu_data_file.IsOpen()) {
        mcu_data_file.WriteBytes(&mcu_data, sizeof(MCUData));
    }
}

Module::Module(Core::System& system_) : system(system_) {
    LoadConfigNANDSaveFile();
    LoadMCUConfig();
    // Check the config savegame EULA Version and update it to 0x7F7F if necessary
    // so users will never get a prompt to accept EULA
    auto version = GetEULAVersion();
    auto& default_version = GetDefaultEULAVersion();
    if (version.major != default_version.major || version.minor != default_version.minor) {
        LOG_INFO(Service_CFG, "Updating accepted EULA version to {}.{}", default_version.major,
                 default_version.minor);
        SetEULAVersion(default_version);
        UpdateConfigNANDSavegame();
    }
}

Module::~Module() = default;

/// Checks if the language is available in the chosen region, and returns a proper one
static std::tuple<u32 /*region*/, SystemLanguage> AdjustLanguageInfoBlock(
    std::span<const u32> region_code, SystemLanguage language) {
    static const std::array<std::vector<SystemLanguage>, 7> region_languages{{
        // JPN
        {LANGUAGE_JP},
        // USA
        {LANGUAGE_EN, LANGUAGE_FR, LANGUAGE_ES, LANGUAGE_PT},
        // EUR
        {LANGUAGE_EN, LANGUAGE_FR, LANGUAGE_DE, LANGUAGE_IT, LANGUAGE_ES, LANGUAGE_NL, LANGUAGE_PT,
         LANGUAGE_RU},
        // AUS
        {LANGUAGE_EN, LANGUAGE_FR, LANGUAGE_DE, LANGUAGE_IT, LANGUAGE_ES, LANGUAGE_NL, LANGUAGE_PT,
         LANGUAGE_RU},
        // CHN
        {LANGUAGE_ZH},
        // KOR
        {LANGUAGE_KO},
        // TWN
        {LANGUAGE_TW},
    }};
    // Check if any available region supports the languages
    for (u32 region : region_code) {
        const auto& available = region_languages[region];
        if (std::find(available.begin(), available.end(), language) != available.end()) {
            // found a proper region, so return this region - language pair
            return {region, language};
        }
    }
    // The language is not available in any available region, so default to the first region and
    // language
    const u32 default_region = region_code[0];
    return {default_region, region_languages[default_region][0]};
}

void Module::UpdatePreferredRegionCode() {
    if (preferred_region_chosen || !system.IsPoweredOn()) {
        return;
    }
    preferred_region_chosen = true;

    const auto preferred_regions = system.GetAppLoader().GetPreferredRegions();
    if (preferred_regions.empty()) {
        return;
    }

    const auto current_language = GetRawSystemLanguage();
    const auto [region, adjusted_language] =
        AdjustLanguageInfoBlock(preferred_regions, current_language);

    preferred_region_code = region;
    LOG_INFO(Service_CFG, "Preferred region code set to {}", preferred_region_code);

    if (current_language != adjusted_language) {
        LOG_WARNING(Service_CFG, "System language {} does not fit the region. Adjusted to {}",
                    current_language, adjusted_language);
        SetSystemLanguage(adjusted_language);
    }
}

void Module::SetUsername(const std::u16string& name) {
    ASSERT(name.size() <= 10);
    UsernameBlock block{};
    name.copy(block.username.data(), name.size());
    SetConfigBlock(UsernameBlockID, sizeof(block), AccessFlag::SystemWrite, &block);
}

std::u16string Module::GetUsername() {
    UsernameBlock block;
    GetConfigBlock(UsernameBlockID, sizeof(block), AccessFlag::SystemRead, &block);

    // the username string in the block isn't null-terminated,
    // so we need to find the end manually.
    std::u16string username(block.username.data(), std::size(block.username));
    const std::size_t pos = username.find(u'\0');
    if (pos != std::u16string::npos)
        username.erase(pos);
    return username;
}

void Module::SetBirthday(u8 month, u8 day) {
    BirthdayBlock block = {month, day};
    SetConfigBlock(BirthdayBlockID, sizeof(block), AccessFlag::SystemWrite, &block);
}

std::tuple<u8, u8> Module::GetBirthday() {
    BirthdayBlock block;
    GetConfigBlock(BirthdayBlockID, sizeof(block), AccessFlag::SystemRead, &block);
    return std::make_tuple(block.month, block.day);
}

void Module::SetSystemLanguage(SystemLanguage language) {
    u8 block = language;
    SetConfigBlock(LanguageBlockID, sizeof(block), AccessFlag::SystemWrite, &block);
}

SystemLanguage Module::GetSystemLanguage() {
    if (Settings::values.region_value.GetValue() == Settings::REGION_VALUE_AUTO_SELECT) {
        UpdatePreferredRegionCode();
    }
    return GetRawSystemLanguage();
}

SystemLanguage Module::GetRawSystemLanguage() {
    u8 block{};
    GetConfigBlock(LanguageBlockID, sizeof(block), AccessFlag::SystemRead, &block);
    return static_cast<SystemLanguage>(block);
}

void Module::SetSoundOutputMode(SoundOutputMode mode) {
    u8 block = mode;
    SetConfigBlock(SoundOutputModeBlockID, sizeof(block), AccessFlag::SystemWrite, &block);
}

SoundOutputMode Module::GetSoundOutputMode() {
    u8 block{};
    GetConfigBlock(SoundOutputModeBlockID, sizeof(block), AccessFlag::SystemRead, &block);
    return static_cast<SoundOutputMode>(block);
}

void Module::SetCountryCode(u8 country_code) {
    ConsoleCountryInfo block = {{0, 0}, default_subregion[country_code], country_code};
    SetConfigBlock(CountryInfoBlockID, sizeof(block), AccessFlag::SystemWrite, &block);
}

u8 Module::GetCountryCode() {
    ConsoleCountryInfo block{};
    GetConfigBlock(CountryInfoBlockID, sizeof(block), AccessFlag::SystemRead, &block);
    return block.country_code;
}

void Module::SetCountryInfo(u8 country_code, u8 state_code) {
    ConsoleCountryInfo block = {{0, 0}, state_code, country_code};
    SetConfigBlock(CountryInfoBlockID, sizeof(block), AccessFlag::SystemWrite, &block);
}

u8 Module::GetStateCode() {
    ConsoleCountryInfo block{};
    GetConfigBlock(CountryInfoBlockID, sizeof(block), AccessFlag::SystemRead, &block);
    return block.state_code;
}

std::pair<u32, u64> Module::GenerateConsoleUniqueId() const {
    CryptoPP::AutoSeededRandomPool rng;
    const u32 random_number = rng.GenerateWord32(0, 0xFFFF);

    u64_le local_friend_code_seed;
    rng.GenerateBlock(reinterpret_cast<CryptoPP::byte*>(&local_friend_code_seed),
                      sizeof(local_friend_code_seed));

    const u64 console_id =
        (local_friend_code_seed & 0x3FFFFFFFF) | (static_cast<u64>(random_number) << 48);

    return std::make_pair(random_number, console_id);
}

Result Module::SetConsoleUniqueId(u32 random_number, u64 console_id) {
    u64_le console_id_le = console_id;
    Result res = SetConfigBlock(ConsoleUniqueID1BlockID, sizeof(console_id_le), AccessFlag::Global,
                                &console_id_le);
    if (!res.IsSuccess())
        return res;

    res = SetConfigBlock(ConsoleUniqueID2BlockID, sizeof(console_id_le), AccessFlag::Global,
                         &console_id_le);
    if (!res.IsSuccess())
        return res;

    u32_le random_number_le = random_number;
    res = SetConfigBlock(ConsoleUniqueID3BlockID, sizeof(random_number_le), AccessFlag::Global,
                         &random_number_le);
    if (!res.IsSuccess())
        return res;

    return ResultSuccess;
}

u64 Module::GetConsoleUniqueId() {
    u64_le console_id_le{};
    GetConfigBlock(ConsoleUniqueID2BlockID, sizeof(console_id_le), AccessFlag::Global,
                   &console_id_le);
    return console_id_le;
}

EULAVersion Module::GetEULAVersion() {
    u32_le data{};
    GetConfigBlock(EULAVersionBlockID, sizeof(data), AccessFlag::Global, &data);
    EULAVersion version;
    version.minor = data & 0xFF;
    version.major = (data >> 8) & 0xFF;
    return version;
}

void Module::SetEULAVersion(const EULAVersion& version) {
    SetConfigBlock(EULAVersionBlockID, sizeof(version), AccessFlag::Global, &version);
}

void Module::SetSystemSetupNeeded(bool setup_needed) {
    u32 block = setup_needed ? 0 : 1;
    SetConfigBlock(SystemSetupRequiredBlockID, sizeof(block), AccessFlag::System, &block);
}

bool Module::IsSystemSetupNeeded() {
    u32 block{};
    GetConfigBlock(SystemSetupRequiredBlockID, sizeof(block), AccessFlag::System, &block);
    return (block & 0xFFFF) == 0;
}

std::shared_ptr<Module> GetModule(Core::System& system) {
    if (system.IsPoweredOn()) {
        auto cfg = system.ServiceManager().GetService<Module::Interface>("cfg:u");
        if (cfg) {
            return cfg->GetModule();
        }
    }

    // If the system is not running or the module is missing,
    // create an ad-hoc module instance to read data from.
    return std::make_shared<Module>(system);
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto cfg = std::make_shared<Module>(system);
    std::make_shared<CFG_I>(cfg)->InstallAsService(service_manager);
    std::make_shared<CFG_S>(cfg)->InstallAsService(service_manager);
    std::make_shared<CFG_U>(cfg)->InstallAsService(service_manager);
    std::make_shared<CFG_NOR>()->InstallAsService(service_manager);
}

std::string GetConsoleIdHash(Core::System& system) {
    u64_le console_id = GetModule(system)->GetConsoleUniqueId();
    std::array<u8, sizeof(console_id)> buffer;
    std::memcpy(buffer.data(), &console_id, sizeof(console_id));

    std::array<u8, CryptoPP::SHA256::DIGESTSIZE> hash;
    CryptoPP::SHA256().CalculateDigest(hash.data(), buffer.data(), sizeof(buffer));
    return fmt::format("{:02x}", fmt::join(hash.begin(), hash.end(), ""));
}

} // namespace Service::CFG
