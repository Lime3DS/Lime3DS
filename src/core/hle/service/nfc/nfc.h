// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <boost/serialization/binary_object.hpp>
#include "common/common_types.h"
#include "core/hle/service/nfc/nfc_device.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class Event;
} // namespace Kernel

namespace Service::NFC {

enum class CommunicationMode : u8 {
    NotInitialized = 0,
    Ntag = 1,
    Amiibo = 2,
    TrainTag = 3,
};

class Module final {
public:
    explicit Module(Core::System& system);
    ~Module();

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> nfc, const char* name, u32 max_session);
        ~Interface();

        std::shared_ptr<Module> GetModule() const;

        bool IsSearchingForAmiibos();

        bool IsTagActive();

        bool LoadAmiibo(const std::string& fullpath);

        void RemoveAmiibo();

    protected:
        /**
         * NFC::Initialize service function
         *  Inputs:
         *      0 : Header code [0x00010040]
         *      1 : (u8) CommunicationMode. Can be either value 0x1, 0x2 or 0x3
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Initialize(Kernel::HLERequestContext& ctx);

        /**
         * NFC::Finalize service function
         *  Inputs:
         *      0 : Header code [0x00020040]
         *      1 : (u8) CommunicationMode.
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Finalize(Kernel::HLERequestContext& ctx);

        /**
         * NFC::Connect service function
         *  Inputs:
         *      0 : Header code [0x00030000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Connect(Kernel::HLERequestContext& ctx);

        /**
         * NFC::Disconnect service function
         *  Inputs:
         *      0 : Header code [0x00040000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Disconnect(Kernel::HLERequestContext& ctx);

        /**
         * NFC::StartDetection service function
         *  Inputs:
         *      0 : Header code [0x00050040]
         *      1 : (u16) unknown. This is normally 0x0
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void StartDetection(Kernel::HLERequestContext& ctx);

        /**
         * NFC::StopDetection service function
         *  Inputs:
         *      0 : Header code [0x00060000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void StopDetection(Kernel::HLERequestContext& ctx);

        /**
         * NFC::Mount service function
         *  Inputs:
         *      0 : Header code [0x00070000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Mount(Kernel::HLERequestContext& ctx);

        /**
         * NFC::Unmount service function
         *  Inputs:
         *      0 : Header code [0x00080000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Unmount(Kernel::HLERequestContext& ctx);

        /**
         * NFC::Flush service function
         *  Inputs:
         *      0 : Header code [0x00090002]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Flush(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetActivateEvent service function
         *  Inputs:
         *      0 : Header code [0x000B0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Copy handle descriptor
         *      3 : Event Handle
         */
        void GetActivateEvent(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetDeactivateEvent service function
         *  Inputs:
         *      0 : Header code [0x000C0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Copy handle descriptor
         *      3 : Event Handle
         */
        void GetDeactivateEvent(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetStatus service function
         *  Inputs:
         *      0 : Header code [0x000D0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : (u8) Tag state
         */
        void GetStatus(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetTargetConnectionStatus service function
         *  Inputs:
         *      0 : Header code [0x000F0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : (u8) Communication state
         */
        void GetTargetConnectionStatus(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetTagInfo2 service function
         *  Inputs:
         *      0 : Header code [0x00100000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *   2-26 : 0x60-byte struct
         */
        void GetTagInfo2(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetTagInfo service function
         *  Inputs:
         *      0 : Header code [0x00110000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *   2-12 : 0x2C-byte struct
         */
        void GetTagInfo(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetConnectResult service function
         *  Inputs:
         *      0 : Header code [0x00120000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Output NFC-adapter result-code
         */
        void GetConnectResult(Kernel::HLERequestContext& ctx);

        /**
         * NFC::OpenApplicationArea service function
         *  Inputs:
         *      0 : Header code [0x00130040]
         *      1 : (u32) App ID
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void OpenApplicationArea(Kernel::HLERequestContext& ctx);

        /**
         * NFC::CreateApplicationArea service function
         *  Inputs:
         *      0 : Header code [0x00140384]
         *      1 : (u32) App ID
         *      2 : Size
         *   3-14 : 0x30-byte zeroed-out struct
         *     15 : 0x20, PID translate-header for kernel
         *     16 : PID written by kernel
         *     17 : (Size << 14) | 2
         *     18 : Pointer to input buffer
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void CreateApplicationArea(Kernel::HLERequestContext& ctx);

        /**
         * NFC::ReadApplicationArea service function
         *  Inputs:
         *      0 : Header code [0x00150040]
         *      1 : Size (unused? Hard-coded to be 0xD8)
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void ReadApplicationArea(Kernel::HLERequestContext& ctx);

        /**
         * NFC::WriteApplicationArea service function
         *  Inputs:
         *      0 : Header code [0x00160242]
         *      1 : Size
         *    2-9 : AmiiboWriteRequest struct (see above)
         *     10 : (Size << 14) | 2
         *     11 : Pointer to input appdata buffer
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void WriteApplicationArea(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetNfpRegisterInfo service function
         *  Inputs:
         *      0 : Header code [0x00170000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *   2-43 : AmiiboSettings struct (see above)
         */
        void GetNfpRegisterInfo(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetNfpCommonInfo service function
         *  Inputs:
         *      0 : Header code [0x00180000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *   2-17 : 0x40-byte config struct
         */
        void GetNfpCommonInfo(Kernel::HLERequestContext& ctx);

        /**
         * NFC::InitializeCreateInfo service function
         *  Inputs:
         *      0 : Header code [0x00180000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *   2-16 : 0x3C-byte config struct
         */
        void InitializeCreateInfo(Kernel::HLERequestContext& ctx);

        /**
         * NFC::LoadAmiiboPartially service function
         *  Inputs:
         *      0 : Header code [0x001A0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void MountRom(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetIdentificationBlock service function
         *  Inputs:
         *      0 : Header code [0x001B0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *   2-31 : 0x36-byte struct
         */
        void GetIdentificationBlock(Kernel::HLERequestContext& ctx);

        /**
         * NFC::Format service function
         *  Inputs:
         *      0 : Header code [0x040100C2]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Format(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetAdminInfo service function
         *  Inputs:
         *      0 : Header code [0x04020000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetAdminInfo(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetEmptyRegisterInfo service function
         *  Inputs:
         *      0 : Header code [0x04030000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetEmptyRegisterInfo(Kernel::HLERequestContext& ctx);

        /**
         * NFC::SetRegisterInfo service function
         *  Inputs:
         *      0 : Header code [0x04040A40]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetRegisterInfo(Kernel::HLERequestContext& ctx);

        /**
         * NFC::DeleteRegisterInfo service function
         *  Inputs:
         *      0 : Header code [0x04050000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void DeleteRegisterInfo(Kernel::HLERequestContext& ctx);

        /**
         * NFC::DeleteApplicationArea service function
         *  Inputs:
         *      0 : Header code [0x04060000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void DeleteApplicationArea(Kernel::HLERequestContext& ctx);

        /**
         * NFC::ExistsApplicationArea service function
         *  Inputs:
         *      0 : Header code [0x04070000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void ExistsApplicationArea(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> nfc;
    };

private:
    CommunicationMode nfc_mode = CommunicationMode::NotInitialized;

    std::shared_ptr<NfcDevice> device = nullptr;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::NFC

SERVICE_CONSTRUCT(Service::NFC::Module)
BOOST_CLASS_EXPORT_KEY(Service::NFC::Module)
