// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <boost/serialization/export.hpp>
#include "core/global.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/service/boss/online_service.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace IPC {
class RequestParser;
}

namespace Service::BOSS {

class Module final {
public:
    explicit Module(Core::System& system_);
    ~Module() = default;

    struct SessionData : public Kernel::SessionRequestHandler::SessionDataBase {
        std::shared_ptr<OnlineService> online_service{};

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int);
        friend class boost::serialization::access;
    };

    class Interface : public ServiceFramework<Interface, SessionData> {
    public:
        Interface(std::shared_ptr<Module> boss, const char* name, u32 max_session);
        ~Interface() = default;

    protected:
        /**
         * BOSS::InitializeSession service function
         *  Inputs:
         *      0 : Header Code[0x00010082]
         *    1-2 : programID, normally zero for using the programID determined from the input PID
         *      3 : 0x20, ARM11-kernel processID translate-header.
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void InitializeSession(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::RegisterStorage service function
         *  Inputs:
         *      0 : Header Code[0x00020100]
         *    1-2 : u64 extdataID
         *      3 : u32 boss_size
         *      4 : u8 extdata_type: 0 = NAND, 1 = SD
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetStorageInfo(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::UnregisterStorage service function
         *  Inputs:
         *      0 : Header Code[0x00030000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void UnregisterStorage(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetStorageInfo service function
         *  Inputs:
         *      0 : Header Code[0x00040000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 unknown value
         */
        void GetStorageInfo(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::RegisterPrivateRootCa service function
         *  Inputs:
         *      0 : Header Code[0x00050042]
         *      1 : u32 Size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void RegisterPrivateRootCa(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::RegisterPrivateClientCert service function
         *  Inputs:
         *      0 : Header Code[0x00060084]
         *      1 : u32 buffer 1 size
         *      2 : u32 buffer 2 size
         *      3 : MappedBufferDesc1(permission = R)
         *      4 : u32 buff_addr1
         *      5 : MappedBufferDesc2(permission = R)
         *      6 : u32 buff_addr2
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff1_size << 4 | 0xA
         *      3 : u32 buff_addr1
         *      4 : buff2_size << 4 | 0xA
         *      5 : u32 buff_addr2
         */
        void RegisterPrivateClientCert(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNewArrivalFlag service function
         *  Inputs:
         *      0 : Header Code[0x00070000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 flag
         */
        void GetNewArrivalFlag(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::RegisterNewArrivalEvent service function
         *  Inputs:
         *      0 : Header Code[0x00080002]
         *      1 : u32 unknown1
         *      2 : u32 unknown2
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void RegisterNewArrivalEvent(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::SetOptoutFlag service function
         *  Inputs:
         *      0 : Header Code[0x00090040]
         *      1 : u8 output_flag
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetOptoutFlag(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetOptoutFlag service function
         *  Inputs:
         *      0 : Header Code[0x000A0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 output_flag
         */
        void GetOptoutFlag(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::RegisterTask service function
         *  Inputs:
         *      0 : Header Code[0x000B00C2]
         *      1 : TaskID buffer size
         *      2 : u8 unknown value, Usually zero, regardless of HTTP GET/POST.
         *      3 : u8 unknown value, Usually zero, regardless of HTTP GET/POST.
         *      4 : MappedBufferDesc1(permission = R)
         *      5 : buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void RegisterTask(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::UnregisterTask service function
         *  Inputs:
         *      0 : Header Code[0x000C0082]
         *      1 : TaskID buffer size
         *      2 : u8 unknown value
         *      3 : MappedBufferDesc1(permission = R)
         *      4 : buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void UnregisterTask(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::ReconfigureTask service function
         *  Inputs:
         *      0 : Header Code[0x000D0082]
         *      1 : TaskID buffer size
         *      2 : u8 unknown value
         *      3 : MappedBufferDesc1(permission = R)
         *      4 : buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void ReconfigureTask(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskIdList service function
         *  Inputs:
         *      0 : Header Code[0x000E0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetTaskIdList(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetStepIdList service function
         *  Inputs:
         *      0 : Header Code[0x000F0042]
         *      1 : u32 buffer size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void GetStepIdList(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNsDataIdList service function
         *  Inputs:
         *      0 : Header Code[0x00100102]
         *      1 : u32 filter
         *      2 : u32 Buffer size in words(max entries)
         *      3 : u16, starting word-index in the internal NsDataId list
         *      4 : u32, start_NsDataId
         *      5 : MappedBufferDesc(permission = W)
         *      6 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u16 Actual number of output entries
         *      3 : u16 Last word-index copied to output in the internal NsDataId list
         *      4 : buff_size << 4 | 0xC
         *      5 : u32 buff_addr
         */
        void GetNsDataIdList(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNsDataIdList1 service function
         *  Inputs:
         *      0 : Header Code[0x00110102]
         *      1 : u32 filter
         *      2 : u32 Buffer size in words(max entries)
         *      3 : u16, starting word-index in the internal NsDataId list
         *      4 : u32, start_NsDataId
         *      5 : MappedBufferDesc(permission = W)
         *      6 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u16 Actual number of output entries
         *      3 : u16 Last word-index copied to output in the internal NsDataId list
         *      4 : buff_size << 4 | 0xC
         *      5 : u32 buff_addr
         */
        void GetNsDataIdList1(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNsDataIdList2 service function
         *  Inputs:
         *      0 : Header Code[0x00120102]
         *      1 : u32 filter
         *      2 : u32 Buffer size in words(max entries)
         *      3 : u16, starting word-index in the internal NsDataId list
         *      4 : u32, start_NsDataId
         *      5 : MappedBufferDesc(permission = W)
         *      6 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u16 Actual number of output entries
         *      3 : u16 Last word-index copied to output in the internal NsDataId list
         *      4 : buff_size << 4 | 0xC
         *      5 : u32 buff_addr
         */
        void GetNsDataIdList2(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNsDataIdList3 service function
         *  Inputs:
         *      0 : Header Code[0x00130102]
         *      1 : u32 filter
         *      2 : u32 Buffer size in words(max entries)
         *      3 : u16, starting word-index in the internal NsDataId list
         *      4 : u32, start_NsDataId
         *      5 : MappedBufferDesc(permission = W)
         *      6 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u16 Actual number of output entries
         *      3 : u16 Last word-index copied to output in the internal NsDataId list
         *      4 : buff_size << 4 | 0xC
         *      5 : u32 buff_addr

         */
        void GetNsDataIdList3(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::SendProperty service function
         *  Inputs:
         *      0 : Header Code[0x00140082]
         *      1 : u16 PropertyID
         *      2 : u32 size
         *      3 : MappedBufferDesc(permission = R)
         *      4 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void SendProperty(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::SendPropertyHandle service function
         *  Inputs:
         *      0 : Header Code[0x00150042]
         *      2 : u16 PropertyID
         *      3 : 0x0
         *      4 : Handle
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SendPropertyHandle(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::ReceiveProperty service function
         *  Inputs:
         *      0 : Header Code[0x00160082]
         *      1 : u16 PropertyID
         *      2 : u32 Size
         *      3 : MappedBufferDesc(permission = W)
         *      4 : u32 buff addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Actual read size
         *      4 : buff_size << 4 | 0xC
         *      5 : u32 buff_addr
         */
        void ReceiveProperty(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::UpdateTaskInterval service function
         *  Inputs:
         *      0 : Header Code[0x00170082]
         *      1 : u32 unknown value
         *      2 : u8 unknown value
         *      3 : MappedBufferDesc1(permission = R)
         *      4 : buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void UpdateTaskInterval(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::UpdateTaskCount service function
         *  Inputs:
         *      0 : Header Code[0x00180082]
         *      1 : u32 buff_size
         *      2 : u32 unknown2
         *      3 : MappedBufferDesc(permission = R)
         *      4 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void UpdateTaskCount(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskInterval service function
         *  Inputs:
         *      0 : Header Code[0x00190042]
         *      1 : u32 size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 unknown value
         *      3 : buff_size << 4 | 0xA
         *      4 : u32 buff_addr
         */
        void GetTaskInterval(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskCount service function
         *  Inputs:
         *      0 : Header Code[0x001A0042]
         *      1 : u32 size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 unknown value
         *      3 : buff_size << 4 | 0xA
         *      4 : u32 buff_addr
         */
        void GetTaskCount(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskServiceStatus service function
         *  Inputs:
         *      0 : Header Code[0x001B0042]
         *      1 : u32 size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 unknown value
         *      3 : buff_size << 4 | 0xA
         *      4 : u32 buff_addr
         */
        void GetTaskServiceStatus(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::StartTask service function
         *  Inputs:
         *      0 : Header Code[0x001C0042]
         *      1 : TaskID buffer size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void StartTask(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::StartTaskImmediate service function
         *  Inputs:
         *      0 : Header Code[0x001D0042]
         *      1 : TaskID buffer size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void StartTaskImmediate(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::CancelTask service function
         *  Inputs:
         *      0 : Header Code[0x001E0042]
         *      1 : TaskID buffer size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void CancelTask(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskFinishHandle service function
         *  Inputs:
         *      0 : Header Code[0x001F0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : 0x0
         *      3 : Task Finish Handle
         */
        void GetTaskFinishHandle(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskState service function
         *  Inputs:
         *      0 : Header Code[0x00200082]
         *      1 : TaskID buffer size
         *      2 : u8 state
         *      3 : MappedBufferDesc(permission = R)
         *      4 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 TaskStatus
         *      3 : u32 Current state value for task PropertyID 0x4
         *      4 : u8 unknown value
         *      5 : buff_size << 4 | 0xA
         *      6 : u32 buff_addr
         */
        void GetTaskState(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskResult service function
         *  Inputs:
         *      0 : Header Code[0x00210042]
         *      1 : u32 size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 unknown value
         *      3 : u32 unknown value
         *      4 : u8 unknown value
         *      5 : buff_size << 4 | 0xA
         *      6 : u32 buff_addr
         */
        void GetTaskResult(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskCommErrorCode service function
         *  Inputs:
         *      0 : Header Code[0x00220042]
         *      1 : u32 size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 unknown value
         *      3 : u32 unknown value
         *      4 : u8 unknown value
         *      5 : buff_size << 4 | 0xA
         *      6 : u32 buff_addr
         */
        void GetTaskCommErrorCode(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskStatus service function
         *  Inputs:
         *      0 : Header Code[0x002300C2]
         *      1 : u32 size
         *      2 : u8 unknown value
         *      3 : u8 unknown value
         *      4 : MappedBufferDesc(permission = R)
         *      5 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 unknown value
         *      3 : buff_size << 4 | 0xA
         *      4 : u32 buff_addr
         */
        void GetTaskStatus(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskError service function
         *  Inputs:
         *      0 : Header Code[0x00240082]
         *      1 : u32 size
         *      2 : u8 unknown value
         *      3 : MappedBufferDesc(permission = R)
         *      4 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 unknown value
         *      3 : buff_size << 4 | 0xA
         *      4 : u32 buff_addr
         */
        void GetTaskError(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskInfo service function
         *  Inputs:
         *      0 : Header Code[0x00250082]
         *      1 : u32 size
         *      2 : u8 unknown value
         *      3 : MappedBufferDesc(permission = R)
         *      4 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void GetTaskInfo(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::DeleteNsData service function
         *  Inputs:
         *      0 : Header Code[0x00260040]
         *      1 : u32 NsDataID
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void DeleteNsData(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNsDataHeaderInfo service function
         *  Inputs:
         *      0 : Header Code[0x002700C2]
         *      1 : u32, NsDataID
         *      2 : u8, type
         *      3 : u32, Size
         *      4 : MappedBufferDesc(permission = W)
         *      5 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xC
         *      3 : u32, buff_addr
         */
        void GetNsDataHeaderInfo(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::ReadNsData service function
         *  Inputs:
         *      0 : Header Code[0x00280102]
         *      1 : u32, NsDataID
         *    2-3 : u64, offset
         *      4 : u32, Size
         *      5 : MappedBufferDesc(permission = W)
         *      6 : u32, buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 Actual read size
         *      3 : u32, unknown value
         *      4 : buff_size << 4 | 0xC
         *      5 : u32, buff_addr
         */
        void ReadNsData(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::SetNsDataAdditionalInfo service function
         *  Inputs:
         *      0 : Header Code[0x00290080]
         *      1 : u32 unknown value
         *      2 : u32 unknown value
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetNsDataAdditionalInfo(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNsDataAdditionalInfo service function
         *  Inputs:
         *      0 : Header Code[0x002A0040]
         *      1 : u32 unknown value
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 unknown value
         */
        void GetNsDataAdditionalInfo(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::SetNsDataNewFlag service function
         *  Inputs:
         *      0 : Header Code[0x002B0080]
         *      1 : u32 unknown value
         *      2 : u8 flag
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetNsDataNewFlag(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNsDataNewFlag service function
         *  Inputs:
         *      0 : Header Code[0x002C0040]
         *      1 : u32 unknown value
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 flag
         */
        void GetNsDataNewFlag(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNsDataLastUpdate service function
         *  Inputs:
         *      0 : Header Code[0x002D0040]
         *      1 : u32 unknown value
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 unknown value
         *      3 : u32 unknown value
         */
        void GetNsDataLastUpdate(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetErrorCode service function
         *  Inputs:
         *      0 : Header Code[0x002E0040]
         *      1 : u8 input
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 unknown value
         */
        void GetErrorCode(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::RegisterStorageEntry service function
         *  Inputs:
         *      0 : Header Code[0x002F0140]
         *      1 : u32 unknown value
         *      2 : u32 unknown value
         *      3 : u32 unknown value
         *      4 : u16 unknown value
         *      5 : u8 unknown value
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void RegisterStorageEntry(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetStorageEntryInfo service function
         *  Inputs:
         *      0 : Header Code[0x00300000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 unknown value
         *      3 : u16 unknown value
         */
        void GetStorageEntryInfo(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::SetStorageOption service function
         *  Inputs:
         *      0 : Header Code[0x00310100]
         *      1 : u8 unknown value
         *      2 : u32 unknown value
         *      3 : u16 unknown value
         *      4 : u16 unknown value
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetStorageOption(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetStorageOption service function
         *  Inputs:
         *      0 : Header Code[0x00320000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 unknown value
         *      3 : u32 unknown value
         *      4 : u16 unknown value
         *      5 : u16 unknown value
         */
        void GetStorageOption(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::StartBgImmediate service function
         *  Inputs:
         *      0 : Header Code[0x00330042]
         *      1 : TaskID buffer size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32, buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32, buff_addr
         */
        void StartBgImmediate(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskProperty0 service function
         *  Inputs:
         *      0 : Header Code[0x00340042]
         *      1 : u32 size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 current state
         *      3 : buff_size << 4 | 0xA
         *      4 : u32 buff_addr
         */
        void GetTaskProperty0(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::RegisterImmediateTask service function
         *  Inputs:
         *      0 : Header Code[0x003500C2]
         *      1 : u32 size
         *      2 : u8 unknown value
         *      3 : u8 unknown value
         *      4 : MappedBufferDesc(permission = R)
         *      5 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void RegisterImmediateTask(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::SetTaskQuery service function
         *  Inputs:
         *      0 : Header Code[0x00360084]
         *      1 : u32 buffer1 size
         *      2 : u32 buffer2 size
         *      3 : MappedBufferDesc1(permission = R)
         *      4 : u32 buff1_addr
         *      5 : MappedBufferDesc2(permission = R)
         *      6 : u32 buff2_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff1_size << 4 | 0xA
         *      3 : u32 buff1_addr
         *      4 : buff2_size << 4 | 0xA
         *      5 : u32 buff2_addr
         */
        void SetTaskQuery(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskQuery service function
         *  Inputs:
         *      0 : Header Code[0x00370084]
         *      1 : u32 buffer1 size
         *      2 : u32 buffer2 size
         *      3 : MappedBufferDesc1(permission = R)
         *      4 : u32 buff1_addr
         *      5 : MappedBufferDesc2(permission = W)
         *      6 : u32 buff2_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff1_size << 4 | 0xA
         *      3 : u32 buff1_addr
         *      4 : buff2_size << 4 | 0xC
         *      5 : u32 buff2_addr
         */
        void GetTaskQuery(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::InitializeSessionPrivileged service function
         *  Inputs:
         *      0 : Header Code[0x04010082]
         *    1-2 : programID, normally zero for using the programID determined from the input PID
         *      3 : 0x20, ARM11-kernel processID translate-header.
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void InitializeSessionPrivileged(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::GetAppNewFlag service function
         *  Inputs:
         *      0 : Header Code[0x04040080]
         *    1-2 : u64 ProgramID
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 flag, 0 = nothing new, 1 = new content
         */
        void GetAppNewFlag(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::GetNsDataIdListPrivileged service function
         *  Inputs:
         *      0 : Header Code[0x040D0182]
         *    1-2 : u64 ProgramID
         *      3 : u32 filter
         *      4 : u32 Buffer size in words(max entries)
         *      5 : u16, starting word-index in the internal NsDataId list
         *      6 : u32, start_NsDataId
         *      7 : MappedBufferDesc(permission = W)
         *      8 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u16 Actual number of output entries
         *      3 : u16 Last word-index copied to output in the internal NsDataId list
         *      4 : buff_size << 4 | 0xC
         *      5 : u32 buff_addr
         */
        void GetNsDataIdListPrivileged(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::GetNsDataIdListPrivileged1 service function
         *  Inputs:
         *      0 : Header Code[0x040E0182]
         *    1-2 : u64 ProgramID
         *      3 : u32 filter
         *      4 : u32 Buffer size in words(max entries)
         *      5 : u16, starting word-index in the internal NsDataId list
         *      6 : u32, start_NsDataId
         *      7 : MappedBufferDesc(permission = W)
         *      8 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u16 Actual number of output entries
         *      3 : u16 Last word-index copied to output in the internal NsDataId list
         *      4 : buff_size << 4 | 0xC
         *      5 : u32 buff_addr
         */
        void GetNsDataIdListPrivileged1(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::SendPropertyPrivileged service function
         *  Inputs:
         *      0 : Header Code[0x04130082]
         *      1 : u16 PropertyID
         *      2 : u32 size
         *      3 : MappedBufferDesc(permission = R)
         *      4 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void SendPropertyPrivileged(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::DeleteNsDataPrivileged service function
         *  Inputs:
         *      0 : Header Code[0x041500C0]
         *    1-2 : u64 ProgramID
         *      3 : u32 NsDataID
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void DeleteNsDataPrivileged(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::GetNsDataHeaderInfoPrivileged service function
         *  Inputs:
         *      0 : Header Code[0x04160142]
         *    1-2 : u64 ProgramID
         *      3 : u32, NsDataID
         *      4 : u8, type
         *      5 : u32, Size
         *      6 : MappedBufferDesc(permission = W)
         *      7 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xC
         *      3 : u32, buff_addr
         */
        void GetNsDataHeaderInfoPrivileged(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::ReadNsDataPrivileged service function
         *  Inputs:
         *      0 : Header Code[0x04170182]
         *    1-2 : u64 ProgramID
         *      3 : u32, NsDataID
         *    4-5 : u64, offset
         *      6 : u32, Size
         *      7 : MappedBufferDesc(permission = W)
         *      8 : u32, buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 Actual read size
         *      3 : u32, unknown value
         *      4 : buff_size << 4 | 0xC
         *      5 : u32, buff_addr
         */
        void ReadNsDataPrivileged(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::SetNsDataNewFlagPrivileged service function
         *  Inputs:
         *      0 : Header Code[0x041A0100]
         *    1-2 : u64 ProgramID
         *      3 : u32 unknown value
         *      4 : u8 flag
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetNsDataNewFlagPrivileged(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::GetNsDataNewFlagPrivileged service function
         *  Inputs:
         *      0 : Header Code[0x041B00C0]
         *    1-2 : u64 ProgramID
         *      3 : u32 unknown value
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 flag
         */
        void GetNsDataNewFlagPrivileged(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> boss;

        std::shared_ptr<OnlineService> GetSessionService(Kernel::HLERequestContext& ctx,
                                                         IPC::RequestParser& rp);
    };

private:
    Core::System& system;
    std::shared_ptr<Kernel::Event> task_finish_event;

    u8 new_arrival_flag;
    u8 ns_data_new_flag;
    u8 ns_data_new_flag_privileged;
    u8 output_flag;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::BOSS

SERVICE_CONSTRUCT(Service::BOSS::Module)
BOOST_CLASS_EXPORT_KEY(Service::BOSS::Module)
BOOST_CLASS_EXPORT_KEY(Service::BOSS::Module::SessionData)
