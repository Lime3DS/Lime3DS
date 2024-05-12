// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <unordered_map>
#include <boost/serialization/base_object.hpp>
#include "common/common_types.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/secure_value_backend.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::FS {

class ArchiveManager;

struct ProgramInfo {
    u64 program_id;
    MediaType media_type;
    INSERT_PADDING_BYTES(4);
};
static_assert(sizeof(ProgramInfo) == 0x10, "ProgramInfo struct has incorrect size");

struct ClientSlot : public Kernel::SessionRequestHandler::SessionDataBase {
    // We retrieves program ID for client process on FS::Initialize(WithSDKVersion)
    // Real 3DS matches program ID and process ID based on data registered by loader via fs:REG, so
    // theoretically the program ID for FS client and for process codeset can mismatch if the loader
    // behaviour is modified. Since we don't emulate fs:REG mechanism, we assume the program ID is
    // the same as codeset ID and fetch from there directly.
    u64 program_id = 0;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler::SessionDataBase>(
            *this);
        ar & program_id;
    }
    friend class boost::serialization::access;
};

class FS_USER final : public ServiceFramework<FS_USER, ClientSlot> {
public:
    explicit FS_USER(Core::System& system);

    // On real HW this is part of FSReg (FSReg:Register). But since that module is only used by
    // loader and pm, which we HLEed, we can just directly use it here
    void RegisterProgramInfo(u32 process_id, u64 program_id, const std::string& filepath);

    std::string GetCurrentGamecardPath() const;

    struct ProductInfo {
        std::array<u8, 0x10> product_code;
        u16_le maker_code;
        u16_le remaster_version;
    };
    static_assert(sizeof(ProductInfo) == 0x14);

    void RegisterProductInfo(u32 process_id, const ProductInfo& product_info);

    std::optional<ProductInfo> GetProductInfo(u32 process_id);

    /// Gets the registered program info of a process.
    ResultVal<ProgramInfo> GetProgramLaunchInfo(u32 process_id) const {
        auto info = program_info_map.find(process_id);
        if (info != program_info_map.end()) {
            return info->second;
        } else {
            return Result(FileSys::ErrCodes::ArchiveNotMounted, ErrorModule::FS,
                          ErrorSummary::NotFound, ErrorLevel::Status);
        }
    }

    void RegisterSecureValueBackend(const std::shared_ptr<FileSys::SecureValueBackend>& backend) {
        secure_value_backend = backend;
    }

private:
    void Initialize(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::OpenFile service function
     *  Inputs:
     *      1 : Transaction
     *      2 : Archive handle lower word
     *      3 : Archive handle upper word
     *      4 : Low path type
     *      5 : Low path size
     *      6 : Open flags
     *      7 : Attributes
     *      8 : (LowPathSize << 14) | 2
     *      9 : Low path data pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      3 : File handle
     */
    void OpenFile(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::OpenFileDirectly service function
     *  Inputs:
     *      1 : Transaction
     *      2 : Archive ID
     *      3 : Archive low path type
     *      4 : Archive low path size
     *      5 : File low path type
     *      6 : File low path size
     *      7 : Flags
     *      8 : Attributes
     *      9 : (ArchiveLowPathSize << 14) | 0x802
     *      10 : Archive low path
     *      11 : (FileLowPathSize << 14) | 2
     *      12 : File low path
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      3 : File handle
     */
    void OpenFileDirectly(Kernel::HLERequestContext& ctx);

    /*
     * FS_User::DeleteFile service function
     *  Inputs:
     *      1 : Transaction
     *      2 : Archive handle lower word
     *      3 : Archive handle upper word
     *      4 : File path string type
     *      5 : File path string size
     *      7 : File path string data
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void DeleteFile(Kernel::HLERequestContext& ctx);

    /*
     * FS_User::RenameFile service function
     *  Inputs:
     *      1 : Transaction
     *      2 : Source archive handle lower word
     *      3 : Source archive handle upper word
     *      4 : Source file path type
     *      5 : Source file path size
     *      6 : Dest archive handle lower word
     *      7 : Dest archive handle upper word
     *      8 : Dest file path type
     *      9 : Dest file path size
     *      11: Source file path string data
     *      13: Dest file path string
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void RenameFile(Kernel::HLERequestContext& ctx);

    /*
     * FS_User::DeleteDirectory service function
     *  Inputs:
     *      1 : Transaction
     *      2 : Archive handle lower word
     *      3 : Archive handle upper word
     *      4 : Directory path string type
     *      5 : Directory path string size
     *      7 : Directory path string data
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void DeleteDirectory(Kernel::HLERequestContext& ctx);

    /*
     * FS_User::DeleteDirectoryRecursively service function
     *  Inputs:
     *      0 : Command header 0x08070142
     *      1 : Transaction
     *      2 : Archive handle lower word
     *      3 : Archive handle upper word
     *      4 : Directory path string type
     *      5 : Directory path string size
     *      7 : Directory path string data
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void DeleteDirectoryRecursively(Kernel::HLERequestContext& ctx);

    /*
     * FS_User::CreateFile service function
     *  Inputs:
     *      0 : Command header 0x08080202
     *      1 : Transaction
     *      2 : Archive handle lower word
     *      3 : Archive handle upper word
     *      4 : File path string type
     *      5 : File path string size
     *      6 : File attributes
     *      7-8 : File size
     *      10: File path string data
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void CreateFile(Kernel::HLERequestContext& ctx);

    /*
     * FS_User::CreateDirectory service function
     *  Inputs:
     *      1 : Transaction
     *      2 : Archive handle lower word
     *      3 : Archive handle upper word
     *      4 : Directory path string type
     *      5 : Directory path string size
     *      6 : Directory attributes
     *      8 : Directory path string data
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void CreateDirectory(Kernel::HLERequestContext& ctx);

    /*
     * FS_User::RenameDirectory service function
     *  Inputs:
     *      1 : Transaction
     *      2 : Source archive handle lower word
     *      3 : Source archive handle upper word
     *      4 : Source dir path type
     *      5 : Source dir path size
     *      6 : Dest archive handle lower word
     *      7 : Dest archive handle upper word
     *      8 : Dest dir path type
     *      9 : Dest dir path size
     *      11: Source dir path string data
     *      13: Dest dir path string
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void RenameDirectory(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::OpenDirectory service function
     *  Inputs:
     *      1 : Archive handle low word
     *      2 : Archive handle high word
     *      3 : Low path type
     *      4 : Low path size
     *      7 : (LowPathSize << 14) | 2
     *      8 : Low path data pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      3 : Directory handle
     */
    void OpenDirectory(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::OpenArchive service function
     *  Inputs:
     *      1 : Archive ID
     *      2 : Archive low path type
     *      3 : Archive low path size
     *      4 : (LowPathSize << 14) | 2
     *      5 : Archive low path
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Archive handle lower word (unused)
     *      3 : Archive handle upper word (same as file handle)
     */
    void OpenArchive(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::ControlArchive service function
     *  Inputs:
     *      1-2 : Archive Handle
     *      3 : Action
     *      4 : Input Size
     *      5 : Output Size
     *      6 : (Input Size << 4) | 0xA
     *      7 : Input Pointer
     *      8 : (Output Size << 4) | 0xC
     *      9 : Output Pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ControlArchive(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::CloseArchive service function
     *  Inputs:
     *      0 : 0x080E0080
     *      1 : Archive handle low word
     *      2 : Archive handle high word
     *  Outputs:
     *      0 : 0x080E0040
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void CloseArchive(Kernel::HLERequestContext& ctx);

    /*
     * FS_User::IsSdmcDetected service function
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Whether the Sdmc could be detected
     */
    void IsSdmcDetected(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::IsSdmcWriteable service function
     *  Outputs:
     *      0 : Command header 0x08180080
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Whether the Sdmc is currently writeable
     */
    void IsSdmcWriteable(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::FormatSaveData service function,
     * formats the SaveData specified by the input path.
     *  Inputs:
     *      0  : 0x084C0242
     *      1  : Archive ID
     *      2  : Archive path type
     *      3  : Archive path size
     *      4  : Size in Blocks (1 block = 512 bytes)
     *      5  : Number of directories
     *      6  : Number of files
     *      7  : Directory bucket count
     *      8  : File bucket count
     *      9  : Duplicate data
     *      10 : (PathSize << 14) | 2
     *      11 : Archive low path
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void FormatSaveData(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::FormatThisUserSaveData service function
     *  Inputs:
     *      0  : 0x080F0180
     *      1  : Size in Blocks (1 block = 512 bytes)
     *      2  : Number of directories
     *      3  : Number of files
     *      4  : Directory bucket count
     *      5  : File bucket count
     *      6  : Duplicate data
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void FormatThisUserSaveData(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::GetFreeBytes service function
     *  Inputs:
     *      0: 0x08120080
     *      1: Archive handle low word
     *      2: Archive handle high word
     *  Outputs:
     *      1: Result of function, 0 on success, otherwise error code
     *      2: Free byte count low word
     *      3: Free byte count high word
     */
    void GetFreeBytes(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::GetSdmcArchiveResource service function.
     *  Inputs:
     *      0 : 0x08140000
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Sector byte-size
     *      3 : Cluster byte-size
     *      4 : Partition capacity in clusters
     *      5 : Available free space in clusters
     */
    void GetSdmcArchiveResource(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::GetNandArchiveResource service function.
     *  Inputs:
     *      0 : 0x08150000
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Sector byte-size
     *      3 : Cluster byte-size
     *      4 : Partition capacity in clusters
     *      5 : Available free space in clusters
     */
    void GetNandArchiveResource(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::CreateExtSaveData service function
     *  Inputs:
     *      0 : 0x08510242
     *      1 : Media type (NAND / SDMC)
     *      2 : Low word of the saveid to create
     *      3 : High word of the saveid to create
     *      4 : Unknown
     *      5 : Number of directories
     *      6 : Number of files
     *      7-8 : Size limit
     *      9 : Size of the SMDH icon
     *      10: (SMDH Size << 4) | 0x0000000A
     *      11: Pointer to the SMDH icon for the new ExtSaveData
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void CreateExtSaveData(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::DeleteExtSaveData service function
     *  Inputs:
     *      0 : 0x08520100
     *      1 : Media type (NAND / SDMC)
     *      2 : Low word of the saveid to create
     *      3 : High word of the saveid to create
     *      4 : Unknown
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void DeleteExtSaveData(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::CardSlotIsInserted service function.
     *  Inputs:
     *      0 : 0x08210000
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Whether there is a game card inserted into the slot or not.
     */
    void CardSlotIsInserted(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::DeleteSystemSaveData service function.
     *  Inputs:
     *      0 : 0x08570080
     *      1 : High word of the SystemSaveData id to delete
     *      2 : Low word of the SystemSaveData id to delete
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void DeleteSystemSaveData(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::CreateSystemSaveData service function.
     *  Inputs:
     *      0 : 0x08560240
     *      1 : u8 MediaType of the system save data
     *      2 : SystemSaveData id to create
     *      3 : Total size
     *      4 : Block size
     *      5 : Number of directories
     *      6 : Number of files
     *      7 : Directory bucket count
     *      8 : File bucket count
     *      9 : u8 Whether to duplicate data or not
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void CreateSystemSaveData(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::CreateLegacySystemSaveData service function.
     *  This function appears to be obsolete and seems to have been replaced by
     *  command 0x08560240 (CreateSystemSaveData).
     *
     *  Inputs:
     *      0 : 0x08100200
     *      1 : SystemSaveData id to create
     *      2 : Total size
     *      3 : Block size
     *      4 : Number of directories
     *      5 : Number of files
     *      6 : Directory bucket count
     *      7 : File bucket count
     *      8 : u8 Duplicate data
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void CreateLegacySystemSaveData(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::InitializeWithSdkVersion service function.
     *  Inputs:
     *      0 : 0x08610042
     *      1 : Used SDK Version
     *      2 : ProcessId Header
     *      3 : placeholder for ProcessId
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void InitializeWithSdkVersion(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::SetPriority service function.
     *  Inputs:
     *      0 : 0x08620040
     *      1 : priority
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetPriority(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::GetPriority service function.
     *  Inputs:
     *      0 : 0x08630000
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : priority
     */
    void GetPriority(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::GetArchiveResource service function.
     *  Inputs:
     *      0 : 0x08490040
     *      1 : Media type
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Sector byte-size
     *      3 : Cluster byte-size
     *      4 : Partition capacity in clusters
     *      5 : Available free space in clusters
     */
    void GetArchiveResource(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::GetFormatInfo service function.
     *  Inputs:
     *      0 : 0x084500C2
     *      1 : Archive ID
     *      2 : Archive path type
     *      3 : Archive path size
     *      4 : (PathSize << 14) | 2
     *      5 : Archive low path
     *  Outputs:
     *      0 : 0x08450140
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Total size
     *      3 : Number of directories
     *      4 : Number of files
     *      5 : Duplicate data
     */
    void GetFormatInfo(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::GetProductInfo service function.
     *  Inputs:
     *      0 : 0x082E0040
     *      1 : Process ID
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2-6 : Product info
     */
    void GetProductInfo(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::GetProgramLaunchInfo service function.
     *  Inputs:
     *      0 : 0x082F0040
     *      1 : Process ID
     *  Outputs:
     *      0 : 0x082F0140
     *      1 : Result of function, 0 on success, otherwise error code
     *      2-3 : Program ID
     *      4 : Media type
     *      5 : Unknown
     */
    void GetProgramLaunchInfo(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::Obsoleted_3_0_CreateExtSaveData service function.
     *  Inputs:
     *      0 : 0x08300182
     *      1 : Media type
     *      2 : Low word of the saveid to create
     *      3 : High word of the saveid to create
     *      4 : Size of the SMDH icon
     *      5 : Number of directories
     *      6 : Number of files
     *      7 : (SMDH Size << 4) | 0x0000000A
     *      8 : Pointer to the SMDH icon for the new ExtSaveData
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ObsoletedCreateExtSaveData(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::Obsoleted_3_0_DeleteExtSaveData service function.
     *  Inputs:
     *      0 : 0x08350080
     *      1 : Media type
     *      2 : Save ID Low (high is always 0x00000000)
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ObsoletedDeleteExtSaveData(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::GetSpecialContentIndex service function.
     *  Inputs:
     *      0 : 0x083A0100
     *      1 : Media type
     *    2-3 : Program ID
     *      4 : Special content type
     *  Outputs:
     *      0 : 0x083A0080
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Special content index
     */
    void GetSpecialContentIndex(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::GetNumSeeds service function.
     *  Inputs:
     *      0 : 0x087D0000
     *  Outputs:
     *      0 : 0x087D0080
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Number of seeds in the SEEDDB
     */
    void GetNumSeeds(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::AddSeed service function.
     *  Inputs:
     *      0 : 0x087A0180
     *    1-2 : u64, Title ID
     *    3-6 : Seed
     *  Outputs:
     *      0 : 0x087A0040
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void AddSeed(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::SetSaveDataSecureValue service function.
     *  Inputs:
     *      0 : 0x08650140
     *      1-2 : Secure Value
     *      3 : Secure Value Slot
     *      4 : Title Id
     *      5 : Title Variation
     *  Outputs:
     *      0 : 0x08650140
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ObsoletedSetSaveDataSecureValue(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::GetSaveDataSecureValue service function.
     *  Inputs:
     *      0 : 0x086600C0
     *      1 : Secure Value Slot
     *      2 : Title Id
     *      3 : Title Variation
     *  Outputs:
     *      0 : 0x086600C0
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : If Secure Value doesn't exist, 0, if it exists, 1
     *      3-4 : Secure Value
     */
    void ObsoletedGetSaveDataSecureValue(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::ControlSecureSave service function
     *  Inputs:
     *      1 : Action
     *      2 : Input Size
     *      3 : Output Size
     *      4 : (Input Size << 4) | 0xA
     *      5 : Input Pointer
     *      6 : (Output Size << 4) | 0xC
     *      7 : Output Pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ControlSecureSave(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::SetThisSaveDataSecureValue service function.
     *  Inputs:
     *      1 : Secure Value Slot
     *      2-3 : Secure Value
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetThisSaveDataSecureValue(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::GetSaveDataSecureValue service function.
     *  Inputs:
     *      1 : Secure Value Slot
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : If Secure Value doesn't exist, 0, if it exists, 1
     *      3 : Unknown
     *      4-5 : Secure Value
     */
    void GetThisSaveDataSecureValue(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::SetSaveDataSecureValue service function.
     *  Inputs:
     *      0 : 0x08750180
     *      1-2 : Archive
     *      3 : Secure Value Slot
     *      4 : value
     *      5 : flush
     *  Outputs:
     *      0 : header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetSaveDataSecureValue(Kernel::HLERequestContext& ctx);

    /**
     * FS_User::GetSaveDataSecureValue service function.
     *  Inputs:
     *      0 : 0x087600C0
     *      1-2 : Archive
     *      2 : Secure Value slot
     *  Outputs:
     *      0 : Header
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : If Secure Value doesn't exist, 0, if it exists, 1
     *      3 : unknown
     *      4-5 : Secure Value
     */
    void GetSaveDataSecureValue(Kernel::HLERequestContext& ctx);

    static ResultVal<u16> GetSpecialContentIndexFromGameCard(u64 title_id, SpecialContentType type);
    static ResultVal<u16> GetSpecialContentIndexFromTMD(MediaType media_type, u64 title_id,
                                                        SpecialContentType type);

    std::unordered_map<u32, ProgramInfo> program_info_map;
    std::string current_gamecard_path;

    std::unordered_map<u32, ProductInfo> product_info_map;

    u32 priority = -1; ///< For SetPriority and GetPriority service functions

    Core::System& system;
    ArchiveManager& archives;

    std::shared_ptr<FileSys::SecureValueBackend> secure_value_backend;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::FS

SERVICE_CONSTRUCT(Service::FS::FS_USER)
BOOST_CLASS_EXPORT_KEY(Service::FS::FS_USER)
BOOST_CLASS_EXPORT_KEY(Service::FS::ClientSlot)
