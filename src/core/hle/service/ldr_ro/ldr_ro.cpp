// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "common/archives.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/ldr_ro/cro_helper.h"
#include "core/hle/service/ldr_ro/ldr_ro.h"

SERVICE_CONSTRUCT_IMPL(Service::LDR::RO)
SERIALIZE_EXPORT_IMPL(Service::LDR::RO)
SERIALIZE_EXPORT_IMPL(Service::LDR::ClientSlot)

namespace Service::LDR {

static const Result ERROR_ALREADY_INITIALIZED = // 0xD9612FF9
    Result(ErrorDescription::AlreadyInitialized, ErrorModule::RO, ErrorSummary::Internal,
           ErrorLevel::Permanent);
static const Result ERROR_NOT_INITIALIZED = // 0xD9612FF8
    Result(ErrorDescription::NotInitialized, ErrorModule::RO, ErrorSummary::Internal,
           ErrorLevel::Permanent);
static const Result ERROR_BUFFER_TOO_SMALL = // 0xE0E12C1F
    Result(static_cast<ErrorDescription>(31), ErrorModule::RO, ErrorSummary::InvalidArgument,
           ErrorLevel::Usage);
static const Result ERROR_MISALIGNED_ADDRESS = // 0xD9012FF1
    Result(ErrorDescription::MisalignedAddress, ErrorModule::RO, ErrorSummary::WrongArgument,
           ErrorLevel::Permanent);
static const Result ERROR_MISALIGNED_SIZE = // 0xD9012FF2
    Result(ErrorDescription::MisalignedSize, ErrorModule::RO, ErrorSummary::WrongArgument,
           ErrorLevel::Permanent);
static const Result ERROR_ILLEGAL_ADDRESS = // 0xE1612C0F
    Result(static_cast<ErrorDescription>(15), ErrorModule::RO, ErrorSummary::Internal,
           ErrorLevel::Usage);
static const Result ERROR_INVALID_MEMORY_STATE = // 0xD8A12C08
    Result(static_cast<ErrorDescription>(8), ErrorModule::RO, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
static const Result ERROR_NOT_LOADED = // 0xD8A12C0D
    Result(static_cast<ErrorDescription>(13), ErrorModule::RO, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);

static bool VerifyBufferState(Kernel::Process& process, VAddr buffer_ptr, u32 size) {
    auto vma = process.vm_manager.FindVMA(buffer_ptr);
    while (vma != process.vm_manager.vma_map.end()) {
        if (vma->second.permissions != Kernel::VMAPermission::ReadWrite ||
            vma->second.meminfo_state != Kernel::MemoryState::Private) {
            return false;
        }
        if (vma->second.base + vma->second.size >= buffer_ptr + size) {
            return true;
        }
        vma = std::next(vma);
    }
    return false;
}

void RO::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    VAddr crs_buffer_ptr = rp.Pop<u32>();
    u32 crs_size = rp.Pop<u32>();
    VAddr crs_address = rp.Pop<u32>();
    // TODO (wwylele): RO service checks the descriptor here and return error 0xD9001830 for
    // incorrect descriptor. This error return should be probably built in IPC::RequestParser.
    // All other service functions below have the same issue.
    auto process = rp.PopObject<Kernel::Process>();

    LOG_DEBUG(Service_LDR, "called, crs_buffer_ptr=0x{:08X}, crs_address=0x{:08X}, crs_size=0x{:X}",
              crs_buffer_ptr, crs_address, crs_size);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    ClientSlot* slot = GetSessionData(ctx.Session());
    if (slot->loaded_crs != 0) {
        LOG_ERROR(Service_LDR, "Already initialized");
        rb.Push(ERROR_ALREADY_INITIALIZED);
        return;
    }

    if (crs_size < CRO_HEADER_SIZE) {
        LOG_ERROR(Service_LDR, "CRS is too small");
        rb.Push(ERROR_BUFFER_TOO_SMALL);
        return;
    }

    if (crs_buffer_ptr & Memory::LIME3DS_PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRS original address is not aligned");
        rb.Push(ERROR_MISALIGNED_ADDRESS);
        return;
    }

    if (crs_address & Memory::LIME3DS_PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRS mapping address is not aligned");
        rb.Push(ERROR_MISALIGNED_ADDRESS);
        return;
    }

    if (crs_size & Memory::LIME3DS_PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRS size is not aligned");
        rb.Push(ERROR_MISALIGNED_SIZE);
        return;
    }

    if (!VerifyBufferState(*process, crs_buffer_ptr, crs_size)) {
        LOG_ERROR(Service_LDR, "CRS original buffer is in invalid state");
        rb.Push(ERROR_INVALID_MEMORY_STATE);
        return;
    }

    if (crs_address < Memory::PROCESS_IMAGE_VADDR ||
        crs_address + crs_size > Memory::PROCESS_IMAGE_VADDR_END) {
        LOG_ERROR(Service_LDR, "CRS mapping address is not in the process image region");
        rb.Push(ERROR_ILLEGAL_ADDRESS);
        return;
    }

    Result result = ResultSuccess;

    result = process->Map(crs_address, crs_buffer_ptr, crs_size, Kernel::VMAPermission::Read, true);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error mapping memory block {:08X}", result.raw);
        rb.Push(result);
        return;
    }

    CROHelper crs(crs_address, *process, system);
    crs.InitCRS();

    result = crs.Rebase(0, crs_size, 0, 0, 0, 0, true);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error rebasing CRS 0x{:08X}", result.raw);
        rb.Push(result);
        return;
    }

    slot->loaded_crs = crs_address;

    rb.Push(ResultSuccess);
}

void RO::LoadCRR(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    VAddr crr_buffer_ptr = rp.Pop<u32>();
    u32 crr_size = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_WARNING(Service_LDR, "(STUBBED) called, crr_buffer_ptr=0x{:08X}, crr_size=0x{:08X}",
                crr_buffer_ptr, crr_size);
}

void RO::UnloadCRR(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 crr_buffer_ptr = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_WARNING(Service_LDR, "(STUBBED) called, crr_buffer_ptr=0x{:08X}", crr_buffer_ptr);
}

void RO::LoadCRO(Kernel::HLERequestContext& ctx, bool link_on_load_bug_fix) {
    IPC::RequestParser rp(ctx);
    VAddr cro_buffer_ptr = rp.Pop<u32>();
    VAddr cro_address = rp.Pop<u32>();
    u32 cro_size = rp.Pop<u32>();
    VAddr data_segment_address = rp.Pop<u32>();
    u32 zero = rp.Pop<u32>();
    u32 data_segment_size = rp.Pop<u32>();
    u32 bss_segment_address = rp.Pop<u32>();
    u32 bss_segment_size = rp.Pop<u32>();
    bool auto_link = rp.Pop<bool>();
    u32 fix_level = rp.Pop<u32>();
    VAddr crr_address = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    LOG_DEBUG(Service_LDR,
              "called ({}), cro_buffer_ptr=0x{:08X}, cro_address=0x{:08X}, cro_size=0x{:X}, "
              "data_segment_address=0x{:08X}, zero={}, data_segment_size=0x{:X}, "
              "bss_segment_address=0x{:08X}, bss_segment_size=0x{:X}, auto_link={}, "
              "fix_level={}, crr_address=0x{:08X}",
              link_on_load_bug_fix ? "new" : "old", cro_buffer_ptr, cro_address, cro_size,
              data_segment_address, zero, data_segment_size, bss_segment_address, bss_segment_size,
              auto_link ? "true" : "false", fix_level, crr_address);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    ClientSlot* slot = GetSessionData(ctx.Session());
    if (slot->loaded_crs == 0) {
        LOG_ERROR(Service_LDR, "Not initialized");
        rb.Push(ERROR_NOT_INITIALIZED);
        rb.Push<u32>(0);
        return;
    }

    if (cro_size < CRO_HEADER_SIZE) {
        LOG_ERROR(Service_LDR, "CRO too small");
        rb.Push(ERROR_BUFFER_TOO_SMALL);
        rb.Push<u32>(0);
        return;
    }

    if (cro_buffer_ptr & Memory::LIME3DS_PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO original address is not aligned");
        rb.Push(ERROR_MISALIGNED_ADDRESS);
        rb.Push<u32>(0);
        return;
    }

    if (cro_address & Memory::LIME3DS_PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO mapping address is not aligned");
        rb.Push(ERROR_MISALIGNED_ADDRESS);
        rb.Push<u32>(0);
        return;
    }

    if (cro_size & Memory::LIME3DS_PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO size is not aligned");
        rb.Push(ERROR_MISALIGNED_SIZE);
        rb.Push<u32>(0);
        return;
    }

    if (!VerifyBufferState(*process, cro_buffer_ptr, cro_size)) {
        LOG_ERROR(Service_LDR, "CRO original buffer is in invalid state");
        rb.Push(ERROR_INVALID_MEMORY_STATE);
        rb.Push<u32>(0);
        return;
    }

    if (cro_address < Memory::PROCESS_IMAGE_VADDR ||
        cro_address + cro_size > Memory::PROCESS_IMAGE_VADDR_END) {
        LOG_ERROR(Service_LDR, "CRO mapping address is not in the process image region");
        rb.Push(ERROR_ILLEGAL_ADDRESS);
        rb.Push<u32>(0);
        return;
    }

    if (zero) {
        LOG_ERROR(Service_LDR, "Zero is not zero {}", zero);
        rb.Push(Result(static_cast<ErrorDescription>(29), ErrorModule::RO, ErrorSummary::Internal,
                       ErrorLevel::Usage));
        rb.Push<u32>(0);
        return;
    }

    Result result = ResultSuccess;

    result = process->Map(cro_address, cro_buffer_ptr, cro_size, Kernel::VMAPermission::Read, true);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error mapping memory block {:08X}", result.raw);
        rb.Push(result);
        rb.Push<u32>(0);
        return;
    }

    CROHelper cro(cro_address, *process, system);

    result = cro.VerifyHash(cro_size, crr_address);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error verifying CRO in CRR {:08X}", result.raw);
        process->Unmap(cro_address, cro_buffer_ptr, cro_size, Kernel::VMAPermission::ReadWrite,
                       true);
        rb.Push(result);
        rb.Push<u32>(0);
        return;
    }

    result = cro.Rebase(slot->loaded_crs, cro_size, data_segment_address, data_segment_size,
                        bss_segment_address, bss_segment_size, false);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error rebasing CRO {:08X}", result.raw);
        process->Unmap(cro_address, cro_buffer_ptr, cro_size, Kernel::VMAPermission::ReadWrite,
                       true);
        rb.Push(result);
        rb.Push<u32>(0);
        return;
    }

    result = cro.Link(slot->loaded_crs, link_on_load_bug_fix);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error linking CRO {:08X}", result.raw);
        process->Unmap(cro_address, cro_buffer_ptr, cro_size, Kernel::VMAPermission::ReadWrite,
                       true);
        rb.Push(result);
        rb.Push<u32>(0);
        return;
    }

    cro.Register(slot->loaded_crs, auto_link);

    u32 fix_size = cro.Fix(fix_level);

    if (fix_size != cro_size) {
        result = process->Unmap(cro_address + fix_size, cro_buffer_ptr + fix_size,
                                cro_size - fix_size, Kernel::VMAPermission::ReadWrite, true);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error unmapping memory block {:08X}", result.raw);
            process->Unmap(cro_address, cro_buffer_ptr, cro_size, Kernel::VMAPermission::ReadWrite,
                           true);
            rb.Push(result);
            rb.Push<u32>(0);
            return;
        }
    }

    auto [exe_begin, exe_size] = cro.GetExecutablePages();
    if (exe_begin) {
        result = process->vm_manager.ReprotectRange(exe_begin, exe_size,
                                                    Kernel::VMAPermission::ReadExecute);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error reprotecting memory block {:08X}", result.raw);
            process->Unmap(cro_address, cro_buffer_ptr, cro_size, Kernel::VMAPermission::ReadWrite,
                           true);
            rb.Push(result);
            rb.Push<u32>(0);
            return;
        }
    }

    system.InvalidateCacheRange(cro_address, cro_size);

    LOG_INFO(Service_LDR, "CRO \"{}\" loaded at 0x{:08X}, fixed_end=0x{:08X}", cro.ModuleName(),
             cro_address, cro_address + fix_size);

    rb.Push(ResultSuccess, fix_size);
}

void RO::UnloadCRO(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    VAddr cro_address = rp.Pop<u32>();
    u32 zero = rp.Pop<u32>();
    VAddr cro_buffer_ptr = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    LOG_DEBUG(Service_LDR, "called, cro_address=0x{:08X}, zero={}, cro_buffer_ptr=0x{:08X}",
              cro_address, zero, cro_buffer_ptr);

    CROHelper cro(cro_address, *process, system);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    ClientSlot* slot = GetSessionData(ctx.Session());
    if (slot->loaded_crs == 0) {
        LOG_ERROR(Service_LDR, "Not initialized");
        rb.Push(ERROR_NOT_INITIALIZED);
        return;
    }

    if (cro_address & Memory::LIME3DS_PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO address is not aligned");
        rb.Push(ERROR_MISALIGNED_ADDRESS);
        return;
    }

    if (!cro.IsLoaded()) {
        LOG_ERROR(Service_LDR, "Invalid or not loaded CRO");
        rb.Push(ERROR_NOT_LOADED);
        return;
    }

    LOG_INFO(Service_LDR, "Unloading CRO \"{}\"", cro.ModuleName());

    u32 fixed_size = cro.GetFixedSize();

    cro.Unregister(slot->loaded_crs);

    Result result = cro.Unlink(slot->loaded_crs);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error unlinking CRO {:08X}", result.raw);
        rb.Push(result);
        return;
    }

    // If the module is not fixed, clears all external/internal relocations
    // to restore the state before loading, so that it can be loaded again(?)
    if (!cro.IsFixed()) {
        result = cro.ClearRelocations();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error clearing relocations {:08X}", result.raw);
            rb.Push(result);
            return;
        }
    }

    cro.Unrebase(false);

    result = process->Unmap(cro_address, cro_buffer_ptr, fixed_size,
                            Kernel::VMAPermission::ReadWrite, true);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error unmapping CRO {:08X}", result.raw);
    }

    system.InvalidateCacheRange(cro_address, fixed_size);

    rb.Push(result);
}

void RO::LinkCRO(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    VAddr cro_address = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    LOG_DEBUG(Service_LDR, "called, cro_address=0x{:08X}", cro_address);

    CROHelper cro(cro_address, *process, system);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    ClientSlot* slot = GetSessionData(ctx.Session());
    if (slot->loaded_crs == 0) {
        LOG_ERROR(Service_LDR, "Not initialized");
        rb.Push(ERROR_NOT_INITIALIZED);
        return;
    }

    if (cro_address & Memory::LIME3DS_PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO address is not aligned");
        rb.Push(ERROR_MISALIGNED_ADDRESS);
        return;
    }

    if (!cro.IsLoaded()) {
        LOG_ERROR(Service_LDR, "Invalid or not loaded CRO");
        rb.Push(ERROR_NOT_LOADED);
        return;
    }

    LOG_INFO(Service_LDR, "Linking CRO \"{}\"", cro.ModuleName());

    Result result = cro.Link(slot->loaded_crs, false);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error linking CRO {:08X}", result.raw);
    }

    rb.Push(result);
}

void RO::UnlinkCRO(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    VAddr cro_address = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    LOG_DEBUG(Service_LDR, "called, cro_address=0x{:08X}", cro_address);

    CROHelper cro(cro_address, *process, system);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    ClientSlot* slot = GetSessionData(ctx.Session());
    if (slot->loaded_crs == 0) {
        LOG_ERROR(Service_LDR, "Not initialized");
        rb.Push(ERROR_NOT_INITIALIZED);
        return;
    }

    if (cro_address & Memory::LIME3DS_PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO address is not aligned");
        rb.Push(ERROR_MISALIGNED_ADDRESS);
        return;
    }

    if (!cro.IsLoaded()) {
        LOG_ERROR(Service_LDR, "Invalid or not loaded CRO");
        rb.Push(ERROR_NOT_LOADED);
        return;
    }

    LOG_INFO(Service_LDR, "Unlinking CRO \"{}\"", cro.ModuleName());

    Result result = cro.Unlink(slot->loaded_crs);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error unlinking CRO {:08X}", result.raw);
    }

    rb.Push(result);
}

void RO::Shutdown(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    VAddr crs_buffer_ptr = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    LOG_DEBUG(Service_LDR, "called, crs_buffer_ptr=0x{:08X}", crs_buffer_ptr);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    ClientSlot* slot = GetSessionData(ctx.Session());
    if (slot->loaded_crs == 0) {
        LOG_ERROR(Service_LDR, "Not initialized");
        rb.Push(ERROR_NOT_INITIALIZED);
        return;
    }

    CROHelper crs(slot->loaded_crs, *process, system);
    crs.Unrebase(true);

    Result result = ResultSuccess;

    result = process->Unmap(slot->loaded_crs, crs_buffer_ptr, crs.GetFileSize(),
                            Kernel::VMAPermission::ReadWrite, true);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error unmapping CRS {:08X}", result.raw);
    }

    slot->loaded_crs = 0;
    rb.Push(result);
}

RO::RO(Core::System& system) : ServiceFramework("ldr:ro", 2), system(system) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &RO::Initialize, "Initialize"},
        {0x0002, &RO::LoadCRR, "LoadCRR"},
        {0x0003, &RO::UnloadCRR, "UnloadCRR"},
        {0x0004, &RO::LoadCRO<false>, "LoadCRO"},
        {0x0005, &RO::UnloadCRO, "UnloadCRO"},
        {0x0006, &RO::LinkCRO, "LinkCRO"},
        {0x0007, &RO::UnlinkCRO, "UnlinkCRO"},
        {0x0008, &RO::Shutdown, "Shutdown"},
        {0x0009, &RO::LoadCRO<true>, "LoadCRO_New"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<RO>(system)->InstallAsService(service_manager);
}

} // namespace Service::LDR
