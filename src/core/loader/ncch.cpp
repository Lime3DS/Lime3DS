// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>
#include <fmt/format.h>
#include "common/literals.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/core.h"
#include "core/file_sys/ncch_container.h"
#include "core/file_sys/title_metadata.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/fs/fs_user.h"
#include "core/loader/ncch.h"
#include "core/loader/smdh.h"
#include "core/memory.h"
#include "core/system_titles.h"
#include "network/network.h"

namespace Loader {

using namespace Common::Literals;
static const u64 UPDATE_MASK = 0x0000000e00000000;

FileType AppLoader_NCCH::IdentifyType(FileUtil::IOFile& file) {
    u32 magic;
    file.Seek(0x100, SEEK_SET);
    if (1 != file.ReadArray<u32>(&magic, 1))
        return FileType::Error;

    if (MakeMagic('N', 'C', 'S', 'D') == magic)
        return FileType::CCI;

    if (MakeMagic('N', 'C', 'C', 'H') == magic)
        return FileType::CXI;

    return FileType::Error;
}

std::pair<std::optional<u32>, ResultStatus> AppLoader_NCCH::LoadCoreVersion() {
    if (!is_loaded) {
        ResultStatus res = base_ncch.Load();
        if (res != ResultStatus::Success) {
            return std::make_pair(std::nullopt, res);
        }
    }

    // Provide the core version from the exheader.
    auto& ncch_caps = overlay_ncch->exheader_header.arm11_system_local_caps;
    return std::make_pair(ncch_caps.core_version, ResultStatus::Success);
}

std::pair<std::optional<Kernel::MemoryMode>, ResultStatus> AppLoader_NCCH::LoadKernelMemoryMode() {
    if (!is_loaded) {
        ResultStatus res = base_ncch.Load();
        if (res != ResultStatus::Success) {
            return std::make_pair(std::nullopt, res);
        }
    }
    if (memory_mode_override.has_value()) {
        return std::make_pair(memory_mode_override, ResultStatus::Success);
    }

    // Provide the memory mode from the exheader.
    auto& ncch_caps = overlay_ncch->exheader_header.arm11_system_local_caps;
    auto mode = static_cast<Kernel::MemoryMode>(ncch_caps.system_mode.Value());
    return std::make_pair(mode, ResultStatus::Success);
}

std::pair<std::optional<Kernel::New3dsHwCapabilities>, ResultStatus>
AppLoader_NCCH::LoadNew3dsHwCapabilities() {
    if (!is_loaded) {
        ResultStatus res = base_ncch.Load();
        if (res != ResultStatus::Success) {
            return std::make_pair(std::nullopt, res);
        }
    }

    // Provide the capabilities from the exheader.
    auto& ncch_caps = overlay_ncch->exheader_header.arm11_system_local_caps;
    auto caps = Kernel::New3dsHwCapabilities{
        ncch_caps.enable_l2_cache != 0,
        ncch_caps.enable_804MHz_cpu != 0,
        static_cast<Kernel::New3dsMemoryMode>(ncch_caps.n3ds_mode),
    };
    return std::make_pair(std::move(caps), ResultStatus::Success);
}

ResultStatus AppLoader_NCCH::LoadExec(std::shared_ptr<Kernel::Process>& process) {
    using Kernel::CodeSet;

    if (!is_loaded)
        return ResultStatus::ErrorNotLoaded;

    std::vector<u8> code;
    u64_le program_id;
    if (ResultStatus::Success == ReadCode(code) &&
        ResultStatus::Success == ReadProgramId(program_id)) {
        if (IsGbaVirtualConsole(code)) {
            LOG_ERROR(Loader, "Encountered unsupported GBA Virtual Console code section.");
            return ResultStatus::ErrorGbaTitle;
        }

        std::string process_name = Common::StringFromFixedZeroTerminatedBuffer(
            (const char*)overlay_ncch->exheader_header.codeset_info.name, 8);

        std::shared_ptr<CodeSet> codeset = system.Kernel().CreateCodeSet(process_name, program_id);

        codeset->CodeSegment().offset = 0;
        codeset->CodeSegment().addr = overlay_ncch->exheader_header.codeset_info.text.address;
        codeset->CodeSegment().size =
            overlay_ncch->exheader_header.codeset_info.text.num_max_pages * Memory::CITRA_PAGE_SIZE;

        codeset->RODataSegment().offset =
            codeset->CodeSegment().offset + codeset->CodeSegment().size;
        codeset->RODataSegment().addr = overlay_ncch->exheader_header.codeset_info.ro.address;
        codeset->RODataSegment().size =
            overlay_ncch->exheader_header.codeset_info.ro.num_max_pages * Memory::CITRA_PAGE_SIZE;

        // TODO(yuriks): Not sure if the bss size is added to the page-aligned .data size or just
        //               to the regular size. Playing it safe for now.
        u32 bss_page_size = (overlay_ncch->exheader_header.codeset_info.bss_size + 0xFFF) & ~0xFFF;
        code.resize(code.size() + bss_page_size, 0);

        codeset->DataSegment().offset =
            codeset->RODataSegment().offset + codeset->RODataSegment().size;
        codeset->DataSegment().addr = overlay_ncch->exheader_header.codeset_info.data.address;
        codeset->DataSegment().size =
            overlay_ncch->exheader_header.codeset_info.data.num_max_pages *
                Memory::CITRA_PAGE_SIZE +
            bss_page_size;

        // Apply patches now that the entire codeset (including .bss) has been allocated
        const ResultStatus patch_result = overlay_ncch->ApplyCodePatch(code);
        if (patch_result != ResultStatus::Success && patch_result != ResultStatus::ErrorNotUsed)
            return patch_result;

        codeset->entrypoint = codeset->CodeSegment().addr;
        codeset->memory = std::move(code);

        process = system.Kernel().CreateProcess(std::move(codeset));

        // Attach a resource limit to the process based on the resource limit category
        const auto category = static_cast<Kernel::ResourceLimitCategory>(
            overlay_ncch->exheader_header.arm11_system_local_caps.resource_limit_category);
        process->resource_limit = system.Kernel().ResourceLimit().GetForCategory(category);

        // When running N3DS-unaware titles pm will lie about the amount of memory available.
        // This means RESLIMIT_COMMIT = APPMEMALLOC doesn't correspond to the actual size of
        // APPLICATION. See:
        // https://github.com/LumaTeam/Luma3DS/blob/e2778a45/sysmodules/pm/source/launch.c#L237
        auto& ncch_caps = overlay_ncch->exheader_header.arm11_system_local_caps;
        const auto o3ds_mode = *LoadKernelMemoryMode().first;
        const auto n3ds_mode = static_cast<Kernel::New3dsMemoryMode>(ncch_caps.n3ds_mode);
        const bool is_new_3ds = Settings::values.is_new_3ds.GetValue();
        if (is_new_3ds && n3ds_mode == Kernel::New3dsMemoryMode::Legacy &&
            category == Kernel::ResourceLimitCategory::Application) {
            u64 new_limit = 0;
            switch (o3ds_mode) {
            case Kernel::MemoryMode::Prod:
                new_limit = 64_MiB;
                break;
            case Kernel::MemoryMode::Dev1:
                new_limit = 96_MiB;
                break;
            case Kernel::MemoryMode::Dev2:
                new_limit = 80_MiB;
                break;
            default:
                break;
            }
            process->resource_limit->SetLimitValue(Kernel::ResourceLimitType::Commit,
                                                   static_cast<s32>(new_limit));
        }

        // Set the default CPU core for this process
        process->ideal_processor =
            overlay_ncch->exheader_header.arm11_system_local_caps.ideal_processor;

        // Copy data while converting endianness
        using KernelCaps = std::array<u32, ExHeader_ARM11_KernelCaps::NUM_DESCRIPTORS>;
        KernelCaps kernel_caps;
        std::copy_n(overlay_ncch->exheader_header.arm11_kernel_caps.descriptors, kernel_caps.size(),
                    begin(kernel_caps));
        process->ParseKernelCaps(kernel_caps.data(), kernel_caps.size());

        s32 priority = overlay_ncch->exheader_header.arm11_system_local_caps.priority;
        u32 stack_size = overlay_ncch->exheader_header.codeset_info.stack_size;

        // On real HW this is done with FS:Reg, but we can be lazy
        auto fs_user = system.ServiceManager().GetService<Service::FS::FS_USER>("fs:USER");
        fs_user->RegisterProgramInfo(process->process_id, process->codeset->program_id, filepath);

        Service::FS::FS_USER::ProductInfo product_info{};
        std::memcpy(product_info.product_code.data(), overlay_ncch->ncch_header.product_code,
                    product_info.product_code.size());
        std::memcpy(&product_info.remaster_version,
                    overlay_ncch->exheader_header.codeset_info.flags.remaster_version,
                    sizeof(product_info.remaster_version));
        product_info.maker_code = overlay_ncch->ncch_header.maker_code;
        fs_user->RegisterProductInfo(process->process_id, product_info);

        process->Run(priority, stack_size);
        return ResultStatus::Success;
    }
    return ResultStatus::Error;
}

void AppLoader_NCCH::ParseRegionLockoutInfo(u64 program_id) {
    if (Settings::values.region_value.GetValue() != Settings::REGION_VALUE_AUTO_SELECT) {
        return;
    }

    preferred_regions.clear();

    std::vector<u8> smdh_buffer;
    if (ReadIcon(smdh_buffer) == ResultStatus::Success && smdh_buffer.size() >= sizeof(SMDH)) {
        SMDH smdh;
        std::memcpy(&smdh, smdh_buffer.data(), sizeof(SMDH));
        u32 region_lockout = smdh.region_lockout;
        constexpr u32 REGION_COUNT = 7;
        for (u32 region = 0; region < REGION_COUNT; ++region) {
            if (region_lockout & 1) {
                preferred_regions.push_back(region);
            }
            region_lockout >>= 1;
        }
    } else {
        const auto region = Core::GetSystemTitleRegion(program_id);
        if (region.has_value()) {
            preferred_regions.push_back(region.value());
        }
    }
}

bool AppLoader_NCCH::IsGbaVirtualConsole(std::span<const u8> code) {
    if (code.size() < 0x10) [[unlikely]] {
        return false;
    }

    u32 gbaVcHeader[2];
    std::memcpy(gbaVcHeader, code.data() + code.size() - 0x10, sizeof(gbaVcHeader));
    return gbaVcHeader[0] == MakeMagic('.', 'C', 'A', 'A') && gbaVcHeader[1] == 1;
}

ResultStatus AppLoader_NCCH::Load(std::shared_ptr<Kernel::Process>& process) {
    u64_le ncch_program_id;

    if (is_loaded)
        return ResultStatus::ErrorAlreadyLoaded;

    ResultStatus result = base_ncch.Load();
    if (result != ResultStatus::Success)
        return result;

    ReadProgramId(ncch_program_id);
    std::string program_id{fmt::format("{:016X}", ncch_program_id)};

    LOG_INFO(Loader, "Program ID: {}", program_id);

    update_ncch.OpenFile(Service::AM::GetTitleContentPath(Service::FS::MediaType::SDMC,
                                                          ncch_program_id | UPDATE_MASK));
    result = update_ncch.Load();
    if (result == ResultStatus::Success) {
        overlay_ncch = &update_ncch;
    }

    if (auto room_member = Network::GetRoomMember().lock()) {
        Network::GameInfo game_info;
        ReadTitle(game_info.name);
        game_info.id = ncch_program_id;
        room_member->SendGameInfo(game_info);
    }

    is_loaded = true; // Set state to loaded

    result = LoadExec(process); // Load the executable into memory for booting
    if (ResultStatus::Success != result)
        return result;

    system.ArchiveManager().RegisterSelfNCCH(*this);

    ParseRegionLockoutInfo(ncch_program_id);

    return ResultStatus::Success;
}

ResultStatus AppLoader_NCCH::IsExecutable(bool& out_executable) {
    Loader::ResultStatus result = overlay_ncch->Load();
    if (result != Loader::ResultStatus::Success)
        return result;

    out_executable = overlay_ncch->ncch_header.is_executable != 0;
    return ResultStatus::Success;
}

ResultStatus AppLoader_NCCH::ReadCode(std::vector<u8>& buffer) {
    return overlay_ncch->LoadSectionExeFS(".code", buffer);
}

ResultStatus AppLoader_NCCH::ReadIcon(std::vector<u8>& buffer) {
    return overlay_ncch->LoadSectionExeFS("icon", buffer);
}

ResultStatus AppLoader_NCCH::ReadBanner(std::vector<u8>& buffer) {
    return overlay_ncch->LoadSectionExeFS("banner", buffer);
}

ResultStatus AppLoader_NCCH::ReadLogo(std::vector<u8>& buffer) {
    return overlay_ncch->LoadSectionExeFS("logo", buffer);
}

ResultStatus AppLoader_NCCH::ReadProgramId(u64& out_program_id) {
    ResultStatus result = base_ncch.ReadProgramId(out_program_id);
    if (result != ResultStatus::Success)
        return result;

    return ResultStatus::Success;
}

ResultStatus AppLoader_NCCH::ReadExtdataId(u64& out_extdata_id) {
    ResultStatus result = base_ncch.ReadExtdataId(out_extdata_id);
    if (result != ResultStatus::Success)
        return result;

    return ResultStatus::Success;
}

ResultStatus AppLoader_NCCH::ReadRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
    return base_ncch.ReadRomFS(romfs_file);
}

ResultStatus AppLoader_NCCH::ReadUpdateRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
    ResultStatus result = update_ncch.ReadRomFS(romfs_file);

    if (result != ResultStatus::Success)
        return base_ncch.ReadRomFS(romfs_file);

    return ResultStatus::Success;
}

ResultStatus AppLoader_NCCH::DumpRomFS(const std::string& target_path) {
    return base_ncch.DumpRomFS(target_path);
}

ResultStatus AppLoader_NCCH::DumpUpdateRomFS(const std::string& target_path) {
    u64 program_id;
    ReadProgramId(program_id);
    update_ncch.OpenFile(
        Service::AM::GetTitleContentPath(Service::FS::MediaType::SDMC, program_id | UPDATE_MASK));
    return update_ncch.DumpRomFS(target_path);
}

ResultStatus AppLoader_NCCH::ReadTitle(std::string& title) {
    std::vector<u8> data;
    Loader::SMDH smdh;
    ReadIcon(data);

    if (!Loader::IsValidSMDH(data)) {
        return ResultStatus::ErrorInvalidFormat;
    }

    std::memcpy(&smdh, data.data(), sizeof(Loader::SMDH));

    const auto& short_title = smdh.GetShortTitle(SMDH::TitleLanguage::English);
    auto title_end = std::find(short_title.begin(), short_title.end(), u'\0');
    title = Common::UTF16ToUTF8(std::u16string{short_title.begin(), title_end});

    return ResultStatus::Success;
}

ResultStatus AppLoader_NCCH::ReadTitleLong(std::string& title) {
    std::vector<u8> data;
    Loader::SMDH smdh;
    ReadIcon(data);

    if (!Loader::IsValidSMDH(data)) {
        return ResultStatus::ErrorInvalidFormat;
    }

    std::memcpy(&smdh, data.data(), sizeof(Loader::SMDH));

    const auto& long_title = smdh.GetLongTitle(SMDH::TitleLanguage::English);
    auto title_end = std::find(long_title.begin(), long_title.end(), u'\0');
    title = Common::UTF16ToUTF8(std::u16string{long_title.begin(), title_end});

    return ResultStatus::Success;
}

} // namespace Loader
