// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::LDR {

struct ClientSlot : public Kernel::SessionRequestHandler::SessionDataBase {
    VAddr loaded_crs = 0; ///< the virtual address of the static module

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler::SessionDataBase>(
            *this);
        ar & loaded_crs;
    }
    friend class boost::serialization::access;
};

class RO final : public ServiceFramework<RO, ClientSlot> {
public:
    explicit RO(Core::System& system);

private:
    /**
     * RO::Initialize service function
     *  Inputs:
     *      0 : 0x000100C2
     *      1 : CRS buffer pointer
     *      2 : CRS Size
     *      3 : Process memory address where the CRS will be mapped
     *      4 : handle translation descriptor (zero)
     *      5 : KProcess handle
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Initialize(Kernel::HLERequestContext& ctx);

    /**
     * RO::LoadCRR service function
     *  Inputs:
     *      0 : 0x00020082
     *      1 : CRR buffer pointer
     *      2 : CRR Size
     *      3 : handle translation descriptor (zero)
     *      4 : KProcess handle
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void LoadCRR(Kernel::HLERequestContext& ctx);

    /**
     * RO::UnloadCRR service function
     *  Inputs:
     *      0 : 0x00030042
     *      1 : CRR buffer pointer
     *      2 : handle translation descriptor (zero)
     *      3 : KProcess handle
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void UnloadCRR(Kernel::HLERequestContext& ctx);

    /**
     * RO::LoadCRO service function
     *  Inputs:
     *      0 : 0x000402C2 (old) / 0x000902C2 (new)
     *      1 : CRO buffer pointer
     *      2 : memory address where the CRO will be mapped
     *      3 : CRO Size
     *      4 : .data segment buffer pointer
     *      5 : must be zero
     *      6 : .data segment buffer size
     *      7 : .bss segment buffer pointer
     *      8 : .bss segment buffer size
     *      9 : (bool) register CRO as auto-link module
     *     10 : fix level
     *     11 : CRR address (zero if use loaded CRR)
     *     12 : handle translation descriptor (zero)
     *     13 : KProcess handle
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : CRO fixed size
     *  Note:
     *      This service function has two versions. The function defined here is a
     *      unified one of two, with an additional parameter link_on_load_bug_fix.
     *      There is a dispatcher template below.
     */
    void LoadCRO(Kernel::HLERequestContext& ctx, bool link_on_load_bug_fix);

    template <bool link_on_load_bug_fix>
    void LoadCRO(Kernel::HLERequestContext& ctx) {
        LoadCRO(ctx, link_on_load_bug_fix);
    }

    /**
     * RO::UnloadCRO service function
     *  Inputs:
     *      0 : 0x000500C2
     *      1 : mapped CRO pointer
     *      2 : zero? (RO service doesn't care)
     *      3 : original CRO pointer
     *      4 : handle translation descriptor (zero)
     *      5 : KProcess handle
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void UnloadCRO(Kernel::HLERequestContext& self);

    /**
     * RO::LinkCRO service function
     *  Inputs:
     *      0 : 0x00060042
     *      1 : mapped CRO pointer
     *      2 : handle translation descriptor (zero)
     *      3 : KProcess handle
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void LinkCRO(Kernel::HLERequestContext& self);

    /**
     * RO::UnlinkCRO service function
     *  Inputs:
     *      0 : 0x00070042
     *      1 : mapped CRO pointer
     *      2 : handle translation descriptor (zero)
     *      3 : KProcess handle
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void UnlinkCRO(Kernel::HLERequestContext& self);

    /**
     * RO::Shutdown service function
     *  Inputs:
     *      0 : 0x00080042
     *      1 : original CRS buffer pointer
     *      2 : handle translation descriptor (zero)
     *      3 : KProcess handle
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Shutdown(Kernel::HLERequestContext& self);

    Core::System& system;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
    }
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::LDR

SERVICE_CONSTRUCT(Service::LDR::RO)
BOOST_CLASS_EXPORT_KEY(Service::LDR::RO)
BOOST_CLASS_EXPORT_KEY(Service::LDR::ClientSlot)
