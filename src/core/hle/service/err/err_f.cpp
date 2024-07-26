// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "common/archives.h"
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/result.h"
#include "core/hle/service/err/err_f.h"
#undef exception_info // We use 'exception_info' as a plain identifier, but MSVC defines this in one
                      // of its many headers.

SERIALIZE_EXPORT_IMPL(Service::ERR::ERR_F)

namespace boost::serialization {
template <class Archive>
void load_construct_data(Archive& ar, Service::ERR::ERR_F* t, const unsigned int) {
    ::new (t) Service::ERR::ERR_F(Core::Global<Core::System>());
}

template void load_construct_data<iarchive>(iarchive& ar, Service::ERR::ERR_F* t,
                                            const unsigned int);
} // namespace boost::serialization

namespace Service::ERR {

enum class FatalErrType : u32 {
    Generic = 0,
    Corrupted = 1,
    CardRemoved = 2,
    Exception = 3,
    ResultFailure = 4,
    Logged = 5,
};

enum class ExceptionType : u32 {
    PrefetchAbort = 0,
    DataAbort = 1,
    Undefined = 2,
    VectorFP = 3,
};

struct ExceptionInfo {
    u8 exception_type;
    INSERT_PADDING_BYTES(3);
    u32 sr;
    u32 ar;
    u32 fpexc;
    u32 fpinst;
    u32 fpinst2;
};
static_assert(sizeof(ExceptionInfo) == 0x18, "ExceptionInfo struct has incorrect size");

struct ExceptionContext final {
    std::array<u32, 16> arm_regs;
    u32 cpsr;
};
static_assert(sizeof(ExceptionContext) == 0x44, "ExceptionContext struct has incorrect size");

struct ExceptionData {
    ExceptionInfo exception_info;
    ExceptionContext exception_context;
    INSERT_PADDING_WORDS(1);
};
static_assert(sizeof(ExceptionData) == 0x60, "ExceptionData struct has incorrect size");

struct ErrInfo {
    struct ErrInfoCommon {
        u8 specifier;          // 0x0
        u8 rev_high;           // 0x1
        u16 rev_low;           // 0x2
        u32 result_code;       // 0x4
        u32 pc_address;        // 0x8
        u32 pid;               // 0xC
        u32 title_id_low;      // 0x10
        u32 title_id_high;     // 0x14
        u32 app_title_id_low;  // 0x18
        u32 app_title_id_high; // 0x1C
    } errinfo_common;
    static_assert(sizeof(ErrInfoCommon) == 0x20, "ErrInfoCommon struct has incorrect size");

    union {
        struct {
            char data[0x60]; // 0x20
        } generic;

        struct {
            ExceptionData exception_data; // 0x20
        } exception;

        struct {
            char message[0x60]; // 0x20
        } result_failure;
    };
};

static std::string GetErrType(u8 type_code) {
    switch (static_cast<FatalErrType>(type_code)) {
    case FatalErrType::Generic:
        return "Generic";
    case FatalErrType::Corrupted:
        return "Corrupted";
    case FatalErrType::CardRemoved:
        return "CardRemoved";
    case FatalErrType::Exception:
        return "Exception";
    case FatalErrType::ResultFailure:
        return "ResultFailure";
    case FatalErrType::Logged:
        return "Logged";
    default:
        return "Unknown Error Type";
    }
}

static std::string GetExceptionType(u8 type_code) {
    switch (static_cast<ExceptionType>(type_code)) {
    case ExceptionType::PrefetchAbort:
        return "Prefetch Abort";
    case ExceptionType::DataAbort:
        return "Data Abort";
    case ExceptionType::Undefined:
        return "Undefined Exception";
    case ExceptionType::VectorFP:
        return "Vector Floating Point Exception";
    default:
        return "Unknown Exception Type";
    }
}

static std::string GetCurrentSystemTime() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::stringstream time_stream;
    time_stream << std::put_time(std::localtime(&time), "%Y/%m/%d %H:%M:%S");
    return time_stream.str();
}

static void LogGenericInfo(const ErrInfo::ErrInfoCommon& errinfo_common) {
    LOG_CRITICAL(Service_ERR, "PID: 0x{:08X}", errinfo_common.pid);
    LOG_CRITICAL(Service_ERR, "REV: 0x{:08X}_0x{:08X}", errinfo_common.rev_high,
                 errinfo_common.rev_low);
    LOG_CRITICAL(Service_ERR, "TID: 0x{:08X}_0x{:08X}", errinfo_common.title_id_high,
                 errinfo_common.title_id_low);
    LOG_CRITICAL(Service_ERR, "AID: 0x{:08X}_0x{:08X}", errinfo_common.app_title_id_high,
                 errinfo_common.app_title_id_low);
    LOG_CRITICAL(Service_ERR, "ADR: 0x{:08X}", errinfo_common.pc_address);

    Result result_code{errinfo_common.result_code};
    LOG_CRITICAL(Service_ERR, "RSL: 0x{:08X}", result_code.raw);
    LOG_CRITICAL(Service_ERR, "  Level: {}", static_cast<u32>(result_code.level.Value()));
    LOG_CRITICAL(Service_ERR, "  Summary: {}", static_cast<u32>(result_code.summary.Value()));
    LOG_CRITICAL(Service_ERR, "  Module: {}", static_cast<u32>(result_code.module.Value()));
    LOG_CRITICAL(Service_ERR, "  Desc: {}", static_cast<u32>(result_code.description.Value()));
}

void ERR_F::ThrowFatalError(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_CRITICAL(Service_ERR, "Fatal error");
    const ErrInfo errinfo = rp.PopRaw<ErrInfo>();
    LOG_CRITICAL(Service_ERR, "Fatal error type: {}", GetErrType(errinfo.errinfo_common.specifier));
    system.SetStatus(Core::System::ResultStatus::ErrorUnknown);

    // Generic Info
    LogGenericInfo(errinfo.errinfo_common);

    switch (static_cast<FatalErrType>(errinfo.errinfo_common.specifier)) {
    case FatalErrType::Generic:
    case FatalErrType::Corrupted:
    case FatalErrType::CardRemoved:
    case FatalErrType::Logged: {
        LOG_CRITICAL(Service_ERR, "Datetime: {}", GetCurrentSystemTime());
        break;
    }
    case FatalErrType::Exception: {
        const auto& errtype = errinfo.exception;

        // Register Info
        LOG_CRITICAL(Service_ERR, "ARM Registers:");
        for (u32 index = 0; index < errtype.exception_data.exception_context.arm_regs.size();
             ++index) {
            if (index < 13) {
                LOG_DEBUG(Service_ERR, "r{}=0x{:08X}", index,
                          errtype.exception_data.exception_context.arm_regs.at(index));
            } else if (index == 13) {
                LOG_CRITICAL(Service_ERR, "SP=0x{:08X}",
                             errtype.exception_data.exception_context.arm_regs.at(index));
            } else if (index == 14) {
                LOG_CRITICAL(Service_ERR, "LR=0x{:08X}",
                             errtype.exception_data.exception_context.arm_regs.at(index));
            } else if (index == 15) {
                LOG_CRITICAL(Service_ERR, "PC=0x{:08X}",
                             errtype.exception_data.exception_context.arm_regs.at(index));
            }
        }
        LOG_CRITICAL(Service_ERR, "CPSR=0x{:08X}", errtype.exception_data.exception_context.cpsr);

        // Exception Info
        LOG_CRITICAL(Service_ERR, "EXCEPTION TYPE: {}",
                     GetExceptionType(errtype.exception_data.exception_info.exception_type));
        switch (static_cast<ExceptionType>(errtype.exception_data.exception_info.exception_type)) {
        case ExceptionType::PrefetchAbort:
            LOG_CRITICAL(Service_ERR, "IFSR: 0x{:08X}", errtype.exception_data.exception_info.sr);
            LOG_CRITICAL(Service_ERR, "r15: 0x{:08X}", errtype.exception_data.exception_info.ar);
            break;
        case ExceptionType::DataAbort:
            LOG_CRITICAL(Service_ERR, "DFSR: 0x{:08X}", errtype.exception_data.exception_info.sr);
            LOG_CRITICAL(Service_ERR, "DFAR: 0x{:08X}", errtype.exception_data.exception_info.ar);
            break;
        case ExceptionType::VectorFP:
            LOG_CRITICAL(Service_ERR, "FPEXC: 0x{:08X}",
                         errtype.exception_data.exception_info.fpinst);
            LOG_CRITICAL(Service_ERR, "FINST: 0x{:08X}",
                         errtype.exception_data.exception_info.fpinst);
            LOG_CRITICAL(Service_ERR, "FINST2: 0x{:08X}",
                         errtype.exception_data.exception_info.fpinst2);
            break;
        case ExceptionType::Undefined:
            break; // Not logging exception_info for this case
        }
        LOG_CRITICAL(Service_ERR, "Datetime: {}", GetCurrentSystemTime());
        break;
    }

    case FatalErrType::ResultFailure: {
        const auto& errtype = errinfo.result_failure;

        // Failure Message
        LOG_CRITICAL(Service_ERR, "Failure Message: {}", errtype.message);
        LOG_CRITICAL(Service_ERR, "Datetime: {}", GetCurrentSystemTime());
        break;
    }

    } // switch FatalErrType

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

ERR_F::ERR_F(Core::System& system) : ServiceFramework("err:f", 1), system(system) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &ERR_F::ThrowFatalError, "ThrowFatalError"},
        {0x0002, nullptr, "SetUserString"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

ERR_F::~ERR_F() = default;

void InstallInterfaces(Core::System& system) {
    auto errf = std::make_shared<ERR_F>(system);
    errf->InstallAsNamedPort(system.Kernel());
}

} // namespace Service::ERR
