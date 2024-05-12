// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/core.h"
#include "core/file_sys/ncch_container.h"
#include "core/hle/service/fs/fs_user.h"
#include "core/loader/loader.h"
#include "network/artic_base/artic_base_client.h"

namespace Loader {

/// Loads an NCCH file (e.g. from a CCI, or the first NCCH in a CXI)
class Apploader_Artic final : public AppLoader {
public:
    Apploader_Artic(Core::System& system_, const std::string& server_addr, u16 server_port)
        : AppLoader(system_, FileUtil::IOFile()) {
        client = std::make_shared<Network::ArticBase::Client>(server_addr, server_port);
        client->SetCommunicationErrorCallback([&system_]() {
            system_.SetStatus(Core::System::ResultStatus::ErrorArticDisconnected);
        });
        client->SetArticReportTrafficCallback(
            [&system_](u32 bytes) { system_.ReportArticTraffic(bytes); });
        client->SetReportArticEventCallback([&system_](u64 event) {
            Core::PerfStats::PerfArticEventBits ev =
                static_cast<Core::PerfStats::PerfArticEventBits>(event & 0xFFFFFFFF);
            bool set = (event > 32) != 0;
            system_.ReportPerfArticEvent(ev, set);
        });
    }

    ~Apploader_Artic() override;

    /**
     * Returns the type of the file
     * @param file FileUtil::IOFile open file
     * @return FileType found, or FileType::Error if this loader doesn't know it
     */
    static FileType IdentifyType(FileUtil::IOFile& file);

    FileType GetFileType() override {
        return IdentifyType(file);
    }

    [[nodiscard]] std::span<const u32> GetPreferredRegions() const override {
        return preferred_regions;
    }

    ResultStatus Load(std::shared_ptr<Kernel::Process>& process) override;

    std::pair<std::optional<u32>, ResultStatus> LoadCoreVersion() override;

    /**
     * Loads the Exheader and returns the system mode for this application.
     * @returns A pair with the optional system mode, and and the status.
     */
    std::pair<std::optional<Kernel::MemoryMode>, ResultStatus> LoadKernelMemoryMode() override;

    std::pair<std::optional<Kernel::New3dsHwCapabilities>, ResultStatus> LoadNew3dsHwCapabilities()
        override;

    ResultStatus IsExecutable(bool& out_executable) override;

    ResultStatus ReadCode(std::vector<u8>& buffer) override;

    ResultStatus ReadIcon(std::vector<u8>& buffer) override;

    ResultStatus ReadBanner(std::vector<u8>& buffer) override;

    ResultStatus ReadLogo(std::vector<u8>& buffer) override;

    ResultStatus ReadProgramId(u64& out_program_id) override;

    ResultStatus ReadExtdataId(u64& out_extdata_id) override;

    ResultStatus ReadRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) override;

    ResultStatus ReadUpdateRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) override;

    ResultStatus DumpRomFS(const std::string& target_path) override;

    ResultStatus DumpUpdateRomFS(const std::string& target_path) override;

    ResultStatus ReadTitle(std::string& title) override;

    bool SupportsSaveStates() override {
        return false;
    }

    bool SupportsMultipleInstancesForSameFile() override {
        return false;
    }

private:
    /**
     * Loads .code section into memory for booting
     * @param process The newly created process
     * @return ResultStatus result of function
     */
    ResultStatus LoadExec(std::shared_ptr<Kernel::Process>& process);

    /// Reads the region lockout info in the SMDH and send it to CFG service
    /// If an SMDH is not present, the program ID is compared against a list
    /// of known system titles to determine the region.
    void ParseRegionLockoutInfo(u64 program_id);

    bool LoadExheader();

    ResultStatus LoadProductInfo(Service::FS::FS_USER::ProductInfo& out);

    ExHeader_Header program_exheader{};
    bool program_exheader_loaded = false;

    std::optional<u64> cached_title_id = std::nullopt;
    std::optional<Service::FS::FS_USER::ProductInfo> cached_product_info = std::nullopt;
    std::vector<u8> cached_icon;
    std::vector<u8> cached_banner;
    std::vector<u8> cached_logo;

    std::vector<u32> preferred_regions;

    std::string server_address;
    std::shared_ptr<Network::ArticBase::Client> client;
    bool client_connected = false;

    std::shared_ptr<FileSys::RomFSReader> main_romfs_reader = nullptr;
    std::shared_ptr<FileSys::RomFSReader> update_romfs_reader = nullptr;
};

} // namespace Loader
