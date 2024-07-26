// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <openssl/rand.h>
#include "common/archives.h"
#include "common/common_types.h"
#include "core/core.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/ssl/ssl_c.h"

SERIALIZE_EXPORT_IMPL(Service::SSL::SSL_C)
namespace Service::SSL {

void SSL_C::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    rp.PopPID();

    // Stub, return success
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void SSL_C::GenerateRandomData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    auto buffer = rp.PopMappedBuffer();

    std::vector<u8> out_data(size);
    SSL::GenerateRandomData(out_data);
    buffer.Write(out_data.data(), 0, size);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(buffer);
}

SSL_C::SSL_C() : ServiceFramework("ssl:C") {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &SSL_C::Initialize, "Initialize"},
        {0x0002, nullptr, "CreateContext"},
        {0x0003, nullptr, "CreateRootCertChain"},
        {0x0004, nullptr, "DestroyRootCertChain"},
        {0x0005, nullptr, "AddTrustedRootCA"},
        {0x0006, nullptr, "RootCertChainAddDefaultCert"},
        {0x0007, nullptr, "RootCertChainRemoveCert"},
        {0x000D, nullptr, "OpenClientCertContext"},
        {0x000E, nullptr, "OpenDefaultClientCertContext"},
        {0x000F, nullptr, "CloseClientCertContext"},
        {0x0011, &SSL_C::GenerateRandomData, "GenerateRandomData"},
        {0x0012, nullptr, "InitializeConnectionSession"},
        {0x0013, nullptr, "StartConnection"},
        {0x0014, nullptr, "StartConnectionGetOut"},
        {0x0015, nullptr, "Read"},
        {0x0016, nullptr, "ReadPeek"},
        {0x0017, nullptr, "Write"},
        {0x0018, nullptr, "ContextSetRootCertChain"},
        {0x0019, nullptr, "ContextSetClientCert"},
        {0x001B, nullptr, "ContextClearOpt"},
        {0x001C, nullptr, "ContextGetProtocolCipher"},
        {0x001E, nullptr, "DestroyContext"},
        {0x001F, nullptr, "ContextInitSharedmem"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<SSL_C>()->InstallAsService(service_manager);
}

void GenerateRandomData(std::vector<u8>& out) {
    // Fill the output buffer with random data.
    RAND_bytes(out.data(), static_cast<int>(out.size()));
}

} // namespace Service::SSL
