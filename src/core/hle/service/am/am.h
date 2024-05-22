// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <boost/serialization/array.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include "common/common_types.h"
#include "common/construct.h"
#include "common/swap.h"
#include "core/file_sys/cia_container.h"
#include "core/file_sys/file_backend.h"
#include "core/global.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace FileUtil {
class IOFile;
}

namespace Service::FS {
enum class MediaType : u32;
}

namespace Kernel {
class Mutex;
}

namespace Service::AM {

namespace ErrCodes {
enum {
    CIACurrentlyInstalling = 4,
    InvalidTID = 31,
    EmptyCIA = 32,
    TryingToUninstallSystemApp = 44,
    InvalidTIDInList = 60,
    InvalidCIAHeader = 104,
};
} // namespace ErrCodes

enum class CIAInstallState : u32 {
    InstallStarted,
    HeaderLoaded,
    CertLoaded,
    TicketLoaded,
    TMDLoaded,
    ContentWritten,
};

enum class InstallStatus : u32 {
    Success,
    ErrorFailedToOpenFile,
    ErrorFileNotFound,
    ErrorAborted,
    ErrorInvalid,
    ErrorEncrypted,
};

// Title ID valid length
constexpr std::size_t TITLE_ID_VALID_LENGTH = 16;

constexpr u64 TWL_TITLE_ID_FLAG = 0x0000800000000000ULL;

// Progress callback for InstallCIA, receives bytes written and total bytes
using ProgressCallback = void(std::size_t, std::size_t);

// A file handled returned for CIAs to be written into and subsequently installed.
class CIAFile final : public FileSys::FileBackend {
public:
    explicit CIAFile(Core::System& system_, Service::FS::MediaType media_type);
    ~CIAFile();

    ResultVal<std::size_t> Read(u64 offset, std::size_t length, u8* buffer) const override;
    Result WriteTicket();
    Result WriteTitleMetadata();
    ResultVal<std::size_t> WriteContentData(u64 offset, std::size_t length, const u8* buffer);
    ResultVal<std::size_t> Write(u64 offset, std::size_t length, bool flush, bool update_timestamp,
                                 const u8* buffer) override;
    u64 GetSize() const override;
    bool SetSize(u64 size) const override;
    bool Close() override;
    void Flush() const override;

private:
    Core::System& system;

    // Whether it's installing an update, and what step of installation it is at
    bool is_update = false;
    CIAInstallState install_state = CIAInstallState::InstallStarted;

    // How much has been written total, CIAContainer for the installing CIA, buffer of all data
    // prior to content data, how much of each content index has been written, and where the CIA
    // is being installed to
    u64 written = 0;
    FileSys::CIAContainer container;
    std::vector<u8> data;
    std::vector<u64> content_written;
    std::vector<FileUtil::IOFile> content_files;
    Service::FS::MediaType media_type;

    class DecryptionState;
    std::unique_ptr<DecryptionState> decryption_state;
};

// A file handled returned for Tickets to be written into and subsequently installed.
class TicketFile final : public FileSys::FileBackend {
public:
    explicit TicketFile();
    ~TicketFile();

    ResultVal<std::size_t> Read(u64 offset, std::size_t length, u8* buffer) const override;
    ResultVal<std::size_t> Write(u64 offset, std::size_t length, bool flush, bool update_timestamp,
                                 const u8* buffer) override;
    u64 GetSize() const override;
    bool SetSize(u64 size) const override;
    bool Close() override;
    void Flush() const override;

private:
    u64 written = 0;
    std::vector<u8> data;
};

/**
 * Installs a CIA file from a specified file path.
 * @param path file path of the CIA file to install
 * @param update_callback callback function called during filesystem write
 * @returns bool whether the install was successful
 */
InstallStatus InstallCIA(const std::string& path,
                         std::function<ProgressCallback>&& update_callback = nullptr);

/**
 * Downloads and installs title form the Nintendo Update Service.
 * @param title_id the title_id to download
 * @returns  whether the install was successful or error code
 */
InstallStatus InstallFromNus(u64 title_id, int version = -1);

/**
 * Get the update title ID for a title
 * @param titleId the title ID
 * @returns The update title ID
 */
u64 GetTitleUpdateId(u64 title_id);

/**
 * Get the mediatype for an installed title
 * @param titleId the installed title ID
 * @returns MediaType which the installed title will reside on
 */
Service::FS::MediaType GetTitleMediaType(u64 titleId);

/**
 * Get the .tmd path for a title
 * @param media_type the media the title exists on
 * @param tid the title ID to get
 * @param update set true if the incoming TMD should be used instead of the current TMD
 * @returns string path to the .tmd file if it exists, otherwise a path to create one is given.
 */
std::string GetTitleMetadataPath(Service::FS::MediaType media_type, u64 tid, bool update = false);

/**
 * Get the .app path for a title's installed content index.
 * @param media_type the media the title exists on
 * @param tid the title ID to get
 * @param index the content index to get
 * @param update set true if the incoming TMD should be used instead of the current TMD
 * @returns string path to the .app file
 */
std::string GetTitleContentPath(FS::MediaType media_type, u64 tid, std::size_t index = 0,
                                bool update = false);

/**
 * Get the folder for a title's installed content.
 * @param media_type the media the title exists on
 * @param tid the title ID to get
 * @returns string path to the title folder
 */
std::string GetTitlePath(Service::FS::MediaType media_type, u64 tid);

/**
 * Get the title/ folder for a storage medium.
 * @param media_type the storage medium to get the path for
 * @returns string path to the folder
 */
std::string GetMediaTitlePath(Service::FS::MediaType media_type);

/**
 * Uninstalls the specified title.
 * @param media_type the storage medium the title is installed to
 * @param title_id the title ID to uninstall
 * @return result of the uninstall operation
 */
Result UninstallProgram(const FS::MediaType media_type, const u64 title_id);

class Module final {
public:
    explicit Module(Core::System& system);
    ~Module();

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> am, const char* name, u32 max_session);
        ~Interface();

    protected:
        /**
         * AM::GetNumPrograms service function
         * Gets the number of installed titles in the requested media type
         *  Inputs:
         *      0 : Command header (0x00010040)
         *      1 : Media type to load the titles from
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : The number of titles in the requested media type
         */
        void GetNumPrograms(Kernel::HLERequestContext& ctx);

        /**
         * AM::FindDLCContentInfos service function
         * Explicitly checks that TID high value is 0004008C or an error is returned.
         *  Inputs:
         *      1 : MediaType
         *    2-3 : u64, Title ID
         *      4 : Content count
         *      6 : Content IDs pointer
         *      8 : Content Infos pointer
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         */
        void FindDLCContentInfos(Kernel::HLERequestContext& ctx);

        /**
         * AM::ListDLCContentInfos service function
         * Explicitly checks that TID high value is 0004008C or an error is returned.
         *  Inputs:
         *      1 : Content count
         *      2 : MediaType
         *    3-4 : u64, Title ID
         *      5 : Start Index
         *      7 : Content Infos pointer
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : Number of content infos returned
         */
        void ListDLCContentInfos(Kernel::HLERequestContext& ctx);

        /**
         * AM::DeleteContents service function
         *  Inputs:
         *      1 : MediaType
         *    2-3 : u64, Title ID
         *      4 : Content count
         *      6 : Content IDs pointer
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         */
        void DeleteContents(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetProgramList service function
         * Loads information about the desired number of titles from the desired media type into an
         * array
         *  Inputs:
         *      1 : Title count
         *      2 : Media type to load the titles from
         *      4 : Title IDs output pointer
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : The number of titles loaded from the requested media type
         */
        void GetProgramList(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetProgramInfos service function
         *  Inputs:
         *      1 : u8 Mediatype
         *      2 : Total titles
         *      4 : TitleIDList pointer
         *      6 : TitleList pointer
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         */
        void GetProgramInfos(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetProgramInfosIgnorePlatform service function
         *  Inputs:
         *      1 : u8 Mediatype
         *      2 : Total titles
         *      4 : TitleIDList pointer
         *      6 : TitleList pointer
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         */
        void GetProgramInfosIgnorePlatform(Kernel::HLERequestContext& ctx);

        /**
         * AM::DeleteUserProgram service function
         * Deletes a user program
         *  Inputs:
         *      1 : Media Type
         *      2-3 : Title ID
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         */
        void DeleteUserProgram(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetProductCode service function
         * Gets the product code of a title
         *  Inputs:
         *      1 : Media Type
         *      2-3 : Title ID
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2-5 : Product Code
         */
        void GetProductCode(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetDLCTitleInfos service function
         * Wrapper for AM::GetProgramInfos, explicitly checks that TID high value is 0004008C.
         *  Inputs:
         *      1 : u8 Mediatype
         *      2 : Total titles
         *      4 : TitleIDList pointer
         *      6 : TitleList pointer
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         */
        void GetDLCTitleInfos(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetPatchTitleInfos service function
         * Wrapper for AM::GetProgramInfos, explicitly checks that TID high value is 0004000E.
         *  Inputs:
         *      1 : u8 Mediatype
         *      2 : Total titles
         *      4 : TitleIDList input pointer
         *      6 : TitleList output pointer
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : TitleIDList input pointer
         *      4 : TitleList output pointer
         */
        void GetPatchTitleInfos(Kernel::HLERequestContext& ctx);

        /**
         * AM::ListDataTitleTicketInfos service function
         *  Inputs:
         *      1 : Ticket count
         *    2-3 : u64, Title ID
         *      4 : Start Index?
         *      5 : (TicketCount * 24) << 8 | 0x4
         *      6 : Ticket Infos pointer
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : Number of ticket infos returned
         */
        void ListDataTitleTicketInfos(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetDLCContentInfoCount service function
         * Explicitly checks that TID high value is 0004008C or an error is returned.
         *  Inputs:
         *      0 : Command header (0x100100C0)
         *      1 : MediaType
         *    2-3 : u64, Title ID
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : Number of content infos plus one
         */
        void GetDLCContentInfoCount(Kernel::HLERequestContext& ctx);

        /**
         * AM::DeleteTicket service function
         *  Inputs:
         *    1-2 : u64, Title ID
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         */
        void DeleteTicket(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetNumTickets service function
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : Number of tickets
         */
        void GetNumTickets(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetTicketList service function
         *  Inputs:
         *      1 : Number of TicketList
         *      2 : Number to skip
         *      4 : TicketList pointer
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : Total TicketList
         */
        void GetTicketList(Kernel::HLERequestContext& ctx);

        /**
         * AM::NeedsCleanup service function
         *  Inputs:
         *      1 : Media Type
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : bool, Needs Cleanup
         */
        void NeedsCleanup(Kernel::HLERequestContext& ctx);

        /**
         * AM::DoCleanup service function
         *  Inputs:
         *      1 : Media Type
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         */
        void DoCleanup(Kernel::HLERequestContext& ctx);

        /**
         * AM::QueryAvailableTitleDatabase service function
         *  Inputs:
         *      1 : Media Type
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : Boolean, database availability
         */
        void QueryAvailableTitleDatabase(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetPersonalizedTicketInfoList service function
         *  Inputs:
         *      1 : Count
         *      2-3 : Buffer
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : Out count
         */
        void GetPersonalizedTicketInfoList(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetNumImportTitleContextsFiltered service function
         *  Inputs:
         *      1 : Count
         *      2 : Filter
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : Num import titles
         */
        void GetNumImportTitleContextsFiltered(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetImportTitleContextListFiltered service function
         *  Inputs:
         *      1 : Count
         *      2 : Media type
         *      3 : filter
         *      4-5 : Buffer
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : Out count
         *      3-4 : Out buffer
         */
        void GetImportTitleContextListFiltered(Kernel::HLERequestContext& ctx);

        /**
         * AM::CheckContentRights service function
         *  Inputs:
         *      1-2 : Title ID
         *      3 : Content Index
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : Boolean, whether we have rights to this content
         */
        void CheckContentRights(Kernel::HLERequestContext& ctx);

        /**
         * AM::CheckContentRightsIgnorePlatform service function
         *  Inputs:
         *      1-2 : Title ID
         *      3 : Content Index
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : Boolean, whether we have rights to this content
         */
        void CheckContentRightsIgnorePlatform(Kernel::HLERequestContext& ctx);

        /**
         * AM::BeginImportProgram service function
         * Begin importing from a CTR Installable Archive
         *  Inputs:
         *      0 : Command header (0x04020040)
         *      1 : Media type to install title to
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2-3 : CIAFile handle for application to write to
         */
        void BeginImportProgram(Kernel::HLERequestContext& ctx);

        /**
         * AM::BeginImportProgramTemporarily service function
         * Begin importing from a CTR Installable Archive into the temporary title database
         *  Inputs:
         *      0 : Command header (0x04030000)
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2-3 : CIAFile handle for application to write to
         */
        void BeginImportProgramTemporarily(Kernel::HLERequestContext& ctx);

        /**
         * AM::EndImportProgram service function
         * Finish importing from a CTR Installable Archive
         *  Inputs:
         *      0 : Command header (0x04050002)
         *      1-2 : CIAFile handle application wrote to
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         */
        void EndImportProgram(Kernel::HLERequestContext& ctx);

        /**
         * AM::EndImportProgramWithoutCommit service function
         * Finish importing from a CTR Installable Archive
         *  Inputs:
         *      0 : Command header (0x04060002)
         *      1-2 : CIAFile handle application wrote to
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         */
        void EndImportProgramWithoutCommit(Kernel::HLERequestContext& ctx);

        /**
         * AM::CommitImportPrograms service function
         * Commits changes from the temporary title database to the real title database (title.db).
         * This is a no-op for us, we don't use title.db
         *  Inputs:
         *      0 : Command header (0x040700C2)
         *      1 : Media type
         *      2 : Title count
         *      3 : Database type
         *    4-5 : Title list buffer
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         */
        void CommitImportPrograms(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetProgramInfoFromCia service function
         * Get TitleInfo from a CIA file handle
         *  Inputs:
         *      0 : Command header (0x04080042)
         *      1 : Media type of the title
         *      2-3 : File handle CIA data can be read from
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2-8: TitleInfo structure
         */
        void GetProgramInfoFromCia(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetSystemMenuDataFromCia service function
         * Loads a CIA file's SMDH data into a specified buffer
         *  Inputs:
         *      0 : Command header (0x04090004)
         *      1-2 : File handle CIA data can be read from
         *      3-4 : Output buffer
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         */
        void GetSystemMenuDataFromCia(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetDependencyListFromCia service function
         * Loads a CIA's dependency list into a specified buffer
         *  Inputs:
         *      0 : Command header (0x040A0002)
         *      1-2 : File handle CIA data can be read from
         *      64-65 : Output buffer
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         */
        void GetDependencyListFromCia(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetTransferSizeFromCia service function
         * Returns the total expected transfer size up to the CIA meta offset from a CIA
         *  Inputs:
         *      0 : Command header (0x040B0002)
         *      1-2 : File handle CIA data can be read from
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2-3 : Transfer size
         */
        void GetTransferSizeFromCia(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetCoreVersionFromCia service function
         * Returns the core version from a CIA
         *  Inputs:
         *      0 : Command header (0x040C0002)
         *      1-2 : File handle CIA data can be read from
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : Core version
         */
        void GetCoreVersionFromCia(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetRequiredSizeFromCia service function
         * Returns the required amount of free space required to install a given CIA file
         *  Inputs:
         *      0 : Command header (0x040D0042)
         *      1 : Media type to install title to
         *      2-3 : File handle CIA data can be read from
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2-3 : Required free space for CIA
         */
        void GetRequiredSizeFromCia(Kernel::HLERequestContext& ctx);

        /**
         * AM::DeleteProgram service function
         * Deletes a program
         *  Inputs:
         *      0 : Command header (0x041000C0)
         *      1 : Media type
         *      2-3 : Title ID
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         */
        void DeleteProgram(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetSystemUpdaterMutex service function
         *  Inputs:
         *      0 : Command header (0x04120000)
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : Copy handle descriptor
         *      3 : System updater mutex
         */
        void GetSystemUpdaterMutex(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetMetaSizeFromCia service function
         * Returns the size of a given CIA's meta section
         *  Inputs:
         *      0 : Command header (0x04130002)
         *      1-2 : File handle CIA data can be read from
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2 : Meta section size
         */
        void GetMetaSizeFromCia(Kernel::HLERequestContext& ctx);

        /**
         * AM::GetMetaDataFromCia service function
         * Loads meta section data from a CIA file into a given buffer
         *  Inputs:
         *      0 : Command header (0x04140044)
         *      1-2 : File handle CIA data can be read from
         *      3-4 : Output buffer
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         */
        void GetMetaDataFromCia(Kernel::HLERequestContext& ctx);

        /**
         * AM::BeginImportTicket service function
         *  Inputs:
         *      1 : Media type to install title to
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         *      2-3 : TicketHandle handle for application to write to
         */
        void BeginImportTicket(Kernel::HLERequestContext& ctx);

        /**
         * AM::EndImportTicket service function
         *  Inputs:
         *      1-2 : TicketHandle handle application wrote to
         *  Outputs:
         *      1 : Result, 0 on success, otherwise error code
         */
        void EndImportTicket(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> am;
    };

private:
    /**
     * Scans the for titles in a storage medium for listing.
     * @param media_type the storage medium to scan
     */
    void ScanForTitles(Service::FS::MediaType media_type);

    /**
     * Scans all storage mediums for titles for listing.
     */
    void ScanForAllTitles();

    Core::System& system;
    bool cia_installing = false;
    std::array<std::vector<u64_le>, 3> am_title_list;
    std::shared_ptr<Kernel::Mutex> system_updater_mutex;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::AM

BOOST_CLASS_EXPORT_KEY(Service::AM::Module)
SERVICE_CONSTRUCT(Service::AM::Module)
