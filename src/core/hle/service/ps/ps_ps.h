// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::PS {

class PS_PS final : public ServiceFramework<PS_PS> {
public:
    PS_PS();
    ~PS_PS() = default;

private:
    SERVICE_SERIALIZATION_SIMPLE

    /**
     * PS_PS::SignRsaSha256 service function
     *  Inputs:
     *      0 : Header Code[0x00010244]
     *    1-8 : SHA256 hash to sign.
     *      9 : Unused. Intended as the signature size.
     *     10 : Descriptor for static buffer
     *     11 : RSA context buffer (static_buffer_id=0)
     *     12 : Descriptor for mapping a write-only buffer in the target process
     *     13 : Signature buffer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Descriptor for mapping a write-only buffer in the target process
     *      3 : Signature buffer. Size for the translate-header is hard-coded to 0x100.
     */
    void SignRsaSha256(Kernel::HLERequestContext& ctx);

    /**
     * PS_PS::VerifyRsaSha256 service function
     *  Inputs:
     *      0 : Header Code[0x00020244]
     *    1-8 : SHA256 hash to compare with.
     *      9 : Unused. Intended as the signature size.
     *     10 : Descriptor for static buffer
     *     11 : RSA context buffer (static_buffer_id=0)
     *     12 : Descriptor for mapping a read-only buffer in the target process
     *     13 : Signature buffer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Descriptor for mapping a read-only buffer in the target process
     *      3 : Signature buffer. Size for the translate-header is hard-coded to 0x100.
     */
    void VerifyRsaSha256(Kernel::HLERequestContext& ctx);

    /**
     * PS_PS::EncryptDecryptAes service function
     *  Inputs:
     *      0 : Header Code[0x00040204]
     *      1 : Size in bytes
     *      2 : Unused. Destination size in bytes
     *    3-6 : IV / CTR
     *      7 : u8 Algorithm Type (0..5)
     *      8 : u8 Key Type (0..7)
     *      9 : (size<<4) | 10
     *     10 : Source pointer
     *     11 : (size<<4) | 12
     *     12 : Destination pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *    2-5 : Output IV / CTR
     */
    void EncryptDecryptAes(Kernel::HLERequestContext& ctx);

    /**
     * PS_PS::EncryptSignDecryptVerifyAesCcm service function
     *  Inputs:
     *      0 : Header Code[0x00050284]
     *      1 : Input buffer size
     *      2 : Output buffer size
     *      3 : Total CBC-MAC associated data, in bytes.
     *      4 : Total data size, in bytes.
     *      5 : MAC size in bytes.
     *    6-8 : Nonce
     *      9 : Algorithm Type (0..5)
     *     10 : Key Type (0..7)
     *     11 : (inbufsize<<8) | 0x4
     *     12 : Source pointer
     *     13 : (outbufsize<<8) | 0x14
     *     14 : Destination pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void EncryptSignDecryptVerifyAesCcm(Kernel::HLERequestContext& ctx);

    /**
     * PS_PS::GetRomId service function
     *  Inputs:
     *      0 : Header Code[0x00060040]
     *      1 :
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void GetRomId(Kernel::HLERequestContext& ctx);

    /**
     * PS_PS::GetRomId2 service function
     *  Inputs:
     *      0 : Header Code[0x00070040]
     *      1 :
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void GetRomId2(Kernel::HLERequestContext& ctx);

    /**
     * PS_PS::GetRomMakerCode service function
     *  Inputs:
     *      0 : Header Code[0x00080040]
     *      1 :
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void GetRomMakerCode(Kernel::HLERequestContext& ctx);

    /**
     * PS_PS::GetCTRCardAutoStartupBit service function
     *  Inputs:
     *      0 : Header Code[0x00090000]
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : u8, 0 = auto startup bit not set, 1 = auto startup bit set
     */
    void GetCTRCardAutoStartupBit(Kernel::HLERequestContext& ctx);

    /**
     * PS_PS::GetLocalFriendCodeSeed service function
     *  Inputs:
     *      0 : Header Code[0x000A0000]
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : LocalFriendCodeSeed lower word
     *      3 : LocalFriendCodeSeed upper word
     */
    void GetLocalFriendCodeSeed(Kernel::HLERequestContext& ctx);

    /**
     * PS_PS::GetDeviceId service function
     *  Inputs:
     *      0 : Header Code[0x000B0000]
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : u32, DeviceID
     */
    void GetDeviceId(Kernel::HLERequestContext& ctx);

    /**
     * PS_PS::SeedRNG service function
     *  Inputs:
     *      0 : Header Code[0x000C0000]
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SeedRNG(Kernel::HLERequestContext& ctx);

    /**
     * PS_PS::GenerateRandomBytes service function
     *  Inputs:
     *      0 : Header Code[0x000D0042]
     *      1 : Output buffer size
     *      2 : (Size<<8) | 0x4
     *      3 : Output buffer pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void GenerateRandomBytes(Kernel::HLERequestContext& ctx);

    /**
     * PS_PS::InterfaceForPXI_0x04010084 service function
     *  Inputs:
     *      0 : Header Code[0x000E0082]
     *      1 : Input buffer size
     *      2 : Output buffer size
     *      3 : (insize<<8) | 0x4
     *      4 : Input buffer pointer
     *      5 : (outsize<<8) | 0x14
     *      6 : Output buffer pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 :
     */
    void InterfaceForPXI_0x04010084(Kernel::HLERequestContext& ctx);

    /**
     * PS_PS::InterfaceForPXI_0x04020082 service function
     *  Inputs:
     *      0 : Header Code[0x000F0082]
     *      1 : insize
     *      2 : u8 flag
     *      3 : (insize<<8) | 0x4
     *      4 : Input buffer pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 :
     */
    void InterfaceForPXI_0x04020082(Kernel::HLERequestContext& ctx);

    /**
     * PS_PS::InterfaceForPXI_0x04030044 service function
     *  Inputs:
     *      0 : Header Code[0x00100042]
     *      1 :
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 :
     */
    void InterfaceForPXI_0x04030044(Kernel::HLERequestContext& ctx);

    /**
     * PS_PS::InterfaceForPXI_0x04040044 service function
     *  Inputs:
     *      0 : Header Code[0x00110042]
     *      1 :
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 :
     */
    void InterfaceForPXI_0x04040044(Kernel::HLERequestContext& ctx);
};

/// Initializes the PS_PS Service
void InstallInterfaces(Core::System& system);

} // namespace Service::PS

BOOST_CLASS_EXPORT_KEY(Service::PS::PS_PS)
