// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <fmt/format.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/service/ac/ac.h"
#include "core/hle/service/act/act.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/apt/apt.h"
#include "core/hle/service/boss/boss.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/cam/y2r_u.h"
#include "core/hle/service/cecd/cecd.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/csnd/csnd_snd.h"
#include "core/hle/service/dlp/dlp.h"
#include "core/hle/service/dsp/dsp_dsp.h"
#include "core/hle/service/err/err_f.h"
#include "core/hle/service/frd/frd.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/fs/fs_user.h"
#include "core/hle/service/gsp/gsp.h"
#include "core/hle/service/gsp/gsp_lcd.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/http/http_c.h"
#include "core/hle/service/ir/ir.h"
#include "core/hle/service/ldr_ro/ldr_ro.h"
#include "core/hle/service/mcu/mcu.h"
#include "core/hle/service/mic/mic_u.h"
#include "core/hle/service/mvd/mvd.h"
#include "core/hle/service/ndm/ndm_u.h"
#include "core/hle/service/news/news.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/nim/nim.h"
#include "core/hle/service/nwm/nwm.h"
#include "core/hle/service/plgldr/plgldr.h"
#include "core/hle/service/pm/pm.h"
#include "core/hle/service/ps/ps_ps.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hle/service/pxi/pxi.h"
#include "core/hle/service/qtm/qtm.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/sm/srv.h"
#include "core/hle/service/soc/soc_u.h"
#include "core/hle/service/ssl/ssl_c.h"
#include "core/loader/loader.h"

namespace Service {

const std::array<ServiceModuleInfo, 41> service_module_map{
    {{"FS", 0x00040130'00001102, FS::InstallInterfaces},
     {"PM", 0x00040130'00001202, PM::InstallInterfaces},
     {"LDR", 0x00040130'00003702, LDR::InstallInterfaces},
     {"PXI", 0x00040130'00001402, PXI::InstallInterfaces},

     {"ERR", 0x00040030'00008A02, ERR::InstallInterfaces},
     {"AC", 0x00040130'00002402, AC::InstallInterfaces},
     {"ACT", 0x00040130'00003802, ACT::InstallInterfaces},
     {"AM", 0x00040130'00001502, AM::InstallInterfaces},
     {"BOSS", 0x00040130'00003402, BOSS::InstallInterfaces},
     {"CAM", 0x00040130'00001602,
      [](Core::System& system) {
          CAM::InstallInterfaces(system);
          Y2R::InstallInterfaces(system);
      }},
     {"CECD", 0x00040130'00002602, CECD::InstallInterfaces},
     {"CFG", 0x00040130'00001702, CFG::InstallInterfaces},
     {"DLP", 0x00040130'00002802, DLP::InstallInterfaces},
     {"DSP", 0x00040130'00001A02, DSP::InstallInterfaces},
     {"FRD", 0x00040130'00003202, FRD::InstallInterfaces},
     {"GSP", 0x00040130'00001C02, GSP::InstallInterfaces},
     {"HID", 0x00040130'00001D02, HID::InstallInterfaces},
     {"IR", 0x00040130'00003302, IR::InstallInterfaces},
     {"MIC", 0x00040130'00002002, MIC::InstallInterfaces},
     {"MVD", 0x00040130'20004102, MVD::InstallInterfaces},
     {"NDM", 0x00040130'00002B02, NDM::InstallInterfaces},
     {"NEWS", 0x00040130'00003502, NEWS::InstallInterfaces},
     {"NFC", 0x00040130'00004002, NFC::InstallInterfaces},
     {"NIM", 0x00040130'00002C02, NIM::InstallInterfaces},
     {"NS", 0x00040130'00008002, APT::InstallInterfaces},
     {"NWM", 0x00040130'00002D02, NWM::InstallInterfaces},
     {"PTM", 0x00040130'00002202, PTM::InstallInterfaces},
     {"QTM", 0x00040130'00004202, QTM::InstallInterfaces},
     {"CSND", 0x00040130'00002702, CSND::InstallInterfaces},
     {"HTTP", 0x00040130'00002902, HTTP::InstallInterfaces},
     {"SOC", 0x00040130'00002E02, SOC::InstallInterfaces},
     {"SSL", 0x00040130'00002F02, SSL::InstallInterfaces},
     {"PS", 0x00040130'00003102, PS::InstallInterfaces},
     {"PLGLDR", 0x00040130'00006902, PLGLDR::InstallInterfaces},
     // no HLE implementation
     {"CDC", 0x00040130'00001802, nullptr},
     {"GPIO", 0x00040130'00001B02, nullptr},
     {"I2C", 0x00040130'00001E02, nullptr},
     {"MCU", 0x00040130'00001F02, MCU::InstallInterfaces},
     {"MP", 0x00040130'00002A02, nullptr},
     {"PDN", 0x00040130'00002102, nullptr},
     {"SPI", 0x00040130'00002302, nullptr}}};

/**
 * Creates a function string for logging, complete with the name (or header code, depending
 * on what's passed in) the port name, and all the cmd_buff arguments.
 */
[[maybe_unused]] static std::string MakeFunctionString(std::string_view name,
                                                       std::string_view port_name,
                                                       const u32* cmd_buff) {
    // Number of params == bits 0-5 + bits 6-11
    int num_params = (cmd_buff[0] & 0x3F) + ((cmd_buff[0] >> 6) & 0x3F);

    std::string function_string = fmt::format("function '{}': port={}", name, port_name);
    for (int i = 1; i <= num_params; ++i) {
        function_string += fmt::format(", cmd_buff[{}]={:#X}", i, cmd_buff[i]);
    }
    return function_string;
}

ServiceFrameworkBase::ServiceFrameworkBase(const char* service_name, u32 max_sessions,
                                           InvokerFn* handler_invoker)
    : service_name(service_name), max_sessions(max_sessions), handler_invoker(handler_invoker) {}

ServiceFrameworkBase::~ServiceFrameworkBase() = default;

void ServiceFrameworkBase::InstallAsService(SM::ServiceManager& service_manager) {
    std::shared_ptr<Kernel::ServerPort> port;
    R_ASSERT(service_manager.RegisterService(std::addressof(port), service_name, max_sessions));
    port->SetHleHandler(shared_from_this());
}

void ServiceFrameworkBase::InstallAsNamedPort(Kernel::KernelSystem& kernel) {
    auto [server_port, client_port] = kernel.CreatePortPair(max_sessions, service_name);
    server_port->SetHleHandler(shared_from_this());
    kernel.AddNamedPort(service_name, std::move(client_port));
}

void ServiceFrameworkBase::RegisterHandlersBase(const FunctionInfoBase* functions, std::size_t n) {
    handlers.reserve(handlers.size() + n);
    for (std::size_t i = 0; i < n; ++i) {
        // Usually this array is sorted by id already, so hint to insert at the end
        handlers.emplace_hint(handlers.cend(), functions[i].command_id, functions[i]);
    }
}

void ServiceFrameworkBase::ReportUnimplementedFunction(u32* cmd_buf, const FunctionInfoBase* info) {
    IPC::Header header{cmd_buf[0]};
    int num_params = header.normal_params_size + header.translate_params_size;
    std::string function_name = info == nullptr ? fmt::format("{:#08x}", cmd_buf[0]) : info->name;

    std::string result =
        fmt::format("function '{}': port='{}' cmd_buf={{[0]={:#x} (0x{:04X}, {}, {})",
                    function_name, service_name, header.raw, header.command_id.Value(),
                    header.normal_params_size.Value(), header.translate_params_size.Value());
    for (int i = 1; i <= num_params; ++i) {
        result += fmt::format(", [{}]={:#x}", i, cmd_buf[i]);
    }

    result.push_back('}');

    LOG_ERROR(Service, "unknown / unimplemented {}", result);
    // TODO(bunnei): Hack - ignore error
    header.normal_params_size.Assign(1);
    header.translate_params_size.Assign(0);
    cmd_buf[0] = header.raw;
    cmd_buf[1] = 0;
}

void ServiceFrameworkBase::HandleSyncRequest(Kernel::HLERequestContext& context) {
    auto itr = handlers.find(context.CommandHeader().command_id.Value());
    const FunctionInfoBase* info = itr == handlers.end() ? nullptr : &itr->second;
    if (info == nullptr || info->handler_callback == nullptr) {
        context.ReportUnimplemented();
        return ReportUnimplementedFunction(context.CommandBuffer(), info);
    }

    LOG_TRACE(Service, "{}",
              MakeFunctionString(info->name, GetServiceName(), context.CommandBuffer()));
    handler_invoker(this, info->handler_callback, context);
}

std::string ServiceFrameworkBase::GetFunctionName(IPC::Header header) const {
    auto itr = handlers.find(header.command_id.Value());
    if (itr == handlers.end()) {
        return "";
    }

    return itr->second.name;
}

static bool AttemptLLE(const ServiceModuleInfo& service_module) {
    if (!Settings::values.lle_modules.at(service_module.name))
        return false;
    std::unique_ptr<Loader::AppLoader> loader =
        Loader::GetLoader(AM::GetTitleContentPath(FS::MediaType::NAND, service_module.title_id));
    if (!loader) {
        LOG_ERROR(Service,
                  "Service module \"{}\" could not be loaded; Defaulting to HLE implementation.",
                  service_module.name);
        return false;
    }
    std::shared_ptr<Kernel::Process> process;
    loader->Load(process);
    LOG_DEBUG(Service, "Service module \"{}\" has been successfully loaded.", service_module.name);
    return true;
}

/// Initialize ServiceManager
void Init(Core::System& core) {
    SM::ServiceManager::InstallInterfaces(core);
    core.Kernel().SetAppMainThreadExtendedSleep(false);
    bool lle_module_present = false;

    for (const auto& service_module : service_module_map) {
        const bool has_lle = AttemptLLE(service_module);
        if (!has_lle && service_module.init_function != nullptr) {
            service_module.init_function(core);
        }
        lle_module_present |= has_lle;
    }
    if (lle_module_present) {
        // If there is at least one LLE module, tell the kernel to
        // add a extended sleep to the app main thread (if option enabled).
        core.Kernel().SetAppMainThreadExtendedSleep(true);
    }
    LOG_DEBUG(Service, "initialized OK");
}

} // namespace Service
