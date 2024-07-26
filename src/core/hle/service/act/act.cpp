// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/act/act.h"
#include "core/hle/service/act/act_a.h"
#include "core/hle/service/act/act_u.h"

namespace Service::ACT {

Module::Interface::Interface(std::shared_ptr<Module> act, const char* name)
    : ServiceFramework(name, 3), act(std::move(act)) {}

Module::Interface::~Interface() = default;

void Module::Interface::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto sdk_version = rp.Pop<u32>();
    const auto shared_memory_size = rp.Pop<u32>();
    const auto caller_pid = rp.PopPID();
    [[maybe_unused]] const auto shared_memory = rp.PopObject<Kernel::SharedMemory>();

    LOG_DEBUG(Service_ACT,
              "(STUBBED) called sdk_version={:08X}, shared_memory_size={:08X}, caller_pid={}",
              sdk_version, shared_memory_size, caller_pid);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void Module::Interface::GetAccountDataBlock(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto unknown = rp.Pop<u8>();
    const auto size = rp.Pop<u32>();
    const auto block_id = rp.Pop<u32>();
    [[maybe_unused]] auto output_buffer = rp.PopMappedBuffer();

    LOG_DEBUG(Service_ACT, "(STUBBED) called unknown={:02X}, size={:08X}, block_id={:08X}", unknown,
              size, block_id);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto act = std::make_shared<Module>();
    std::make_shared<ACT_A>(act)->InstallAsService(service_manager);
    std::make_shared<ACT_U>(act)->InstallAsService(service_manager);
}

} // namespace Service::ACT
