// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Copyright 2022 The Pixellizer Group
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
// associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
// NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <boost/serialization/weak_ptr.hpp>
#include <fmt/format.h>
#include "common/archives.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/plugin_3gx.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/plgldr/plgldr.h"
#include "core/loader/loader.h"

SERIALIZE_EXPORT_IMPL(Service::PLGLDR::PLG_LDR)
SERVICE_CONSTRUCT_IMPL(Service::PLGLDR::PLG_LDR)

namespace Service::PLGLDR {

static const Kernel::CoreVersion plgldr_version = Kernel::CoreVersion(1, 0, 0);

PLG_LDR::PLG_LDR(Core::System& system_) : ServiceFramework{"plg:ldr", 1}, system(system_) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "LoadPlugin"},
        {0x0002, &PLG_LDR::IsEnabled, "IsEnabled"},
        {0x0003, &PLG_LDR::SetEnabled, "SetEnabled"},
        {0x0004, &PLG_LDR::SetLoadSettings, "SetLoadSettings"},
        {0x0005, nullptr, "DisplayMenu"},
        {0x0006, nullptr, "DisplayMessage"},
        {0x0007, &PLG_LDR::DisplayErrorMessage, "DisplayErrorMessage"},
        {0x0008, &PLG_LDR::GetPLGLDRVersion, "GetPLGLDRVersion"},
        {0x0009, &PLG_LDR::GetArbiter, "GetArbiter"},
        {0x000A, &PLG_LDR::GetPluginPath, "GetPluginPath"},
        {0x000B, nullptr, "SetRosalinaMenuBlock"},
        {0x000C, nullptr, "SetSwapParam"},
        {0x000D, nullptr, "SetLoadExeParam"},
        // clang-format on
    };
    RegisterHandlers(functions);
    plgldr_context.memory_changed_handle = 0;
    plgldr_context.plugin_loaded = false;
}

template <class Archive>
void PLG_LDR::PluginLoaderContext::serialize(Archive& ar, const unsigned int) {
    ar & is_enabled;
    ar & allow_game_change;
    ar & plugin_loaded;
    ar & is_default_path;
    ar & plugin_path;
    ar & use_user_load_parameters;
    ar & user_load_parameters;
    ar & plg_event;
    ar & plg_reply;
    ar & memory_changed_handle;
    ar & is_exe_load_function_set;
    ar & exe_load_checksum;
    ar & load_exe_func;
    ar & load_exe_args;
    ar & plugin_fb_addr;
}
SERIALIZE_IMPL(PLG_LDR::PluginLoaderContext)

template <class Archive>
void PLG_LDR::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
    ar & plgldr_context;
}
SERIALIZE_IMPL(PLG_LDR)

void PLG_LDR::OnProcessRun(Kernel::Process& process, Kernel::KernelSystem& kernel) {
    if (!plgldr_context.is_enabled || plgldr_context.plugin_loaded) {
        return;
    }
    {
        // Same check as original plugin loader, plugins are not supported in homebrew apps
        u32 value1, value2;
        kernel.memory.ReadBlock(process, process.codeset->CodeSegment().addr, &value1, 4);
        kernel.memory.ReadBlock(process, process.codeset->CodeSegment().addr + 32, &value2, 4);
        // Check for "B #0x20" and "MOV R4, LR" instructions
        bool is_homebrew = u32_le(value1) == 0xEA000006 && u32_le(value2) == 0xE1A0400E;
        if (is_homebrew) {
            return;
        }
    }
    FileSys::Plugin3GXLoader plugin_loader;
    if (plgldr_context.use_user_load_parameters &&
        plgldr_context.user_load_parameters.low_title_Id ==
            static_cast<u32>(process.codeset->program_id) &&
        plgldr_context.user_load_parameters.path[0]) {
        std::string plugin_file = FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir) +
                                  std::string(plgldr_context.user_load_parameters.path + 1);
        plgldr_context.is_default_path = false;
        plgldr_context.plugin_path = plugin_file;
        plugin_loader.Load(plgldr_context, process, kernel, *this);
    } else {
        const std::string plugin_root =
            FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir) + "luma/plugins/";
        const std::string plugin_tid =
            plugin_root + fmt::format("{:016X}", process.codeset->program_id);
        FileUtil::FSTEntry entry;
        FileUtil::ScanDirectoryTree(plugin_tid, entry);
        for (const auto& child : entry.children) {
            if (!child.isDirectory && child.physicalName.ends_with(".3gx")) {
                plgldr_context.is_default_path = false;
                plgldr_context.plugin_path = child.physicalName;
                if (plugin_loader.Load(plgldr_context, process, kernel, *this) ==
                    Loader::ResultStatus::Success) {
                    return;
                }
            }
        }

        const std::string default_path = plugin_root + "default.3gx";
        if (FileUtil::Exists(default_path)) {
            plgldr_context.is_default_path = true;
            plgldr_context.plugin_path = default_path;
            plugin_loader.Load(plgldr_context, process, kernel, *this);
        }
    }
}

void PLG_LDR::OnProcessExit(Kernel::Process& process, Kernel::KernelSystem& kernel) {
    if (plgldr_context.plugin_loaded) {
        u32 status = kernel.memory.Read32(FileSys::Plugin3GXLoader::_3GX_exe_load_addr - 0xC);
        if (status == 0) {
            LOG_CRITICAL(Service_PLGLDR, "Failed to launch {}: Checksum failed",
                         plgldr_context.plugin_path);
        }
    }
}

ResultVal<Kernel::Handle> PLG_LDR::GetMemoryChangedHandle(Kernel::KernelSystem& kernel) {
    if (plgldr_context.memory_changed_handle) {
        return plgldr_context.memory_changed_handle;
    }

    std::shared_ptr<Kernel::Event> evt =
        kernel.CreateEvent(Kernel::ResetType::OneShot,
                           fmt::format("event-{:08x}", system.GetRunningCore().GetReg(14)));
    R_TRY(kernel.GetCurrentProcess()->handle_table.Create(
        std::addressof(plgldr_context.memory_changed_handle), std::move(evt)));
    return plgldr_context.memory_changed_handle;
}

void PLG_LDR::OnMemoryChanged(Kernel::Process& process, Kernel::KernelSystem& kernel) {
    if (!plgldr_context.plugin_loaded || !plgldr_context.memory_changed_handle) {
        return;
    }

    std::shared_ptr<Kernel::Event> evt =
        kernel.GetCurrentProcess()->handle_table.Get<Kernel::Event>(
            plgldr_context.memory_changed_handle);
    if (evt == nullptr)
        return;

    evt->Signal();
}

void PLG_LDR::IsEnabled(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(plgldr_context.is_enabled);
}

void PLG_LDR::SetEnabled(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    bool enabled = rp.Pop<u32>() == 1;

    bool can_change = enabled == plgldr_context.is_enabled || plgldr_context.allow_game_change;
    if (can_change) {
        plgldr_context.is_enabled = enabled;
        Settings::values.plugin_loader_enabled.SetValue(enabled);
    }
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push((can_change) ? ResultSuccess : Kernel::ResultNotAuthorized);
}

void PLG_LDR::SetLoadSettings(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    plgldr_context.use_user_load_parameters = true;
    u32_le flags = rp.Pop<u32>();
    plgldr_context.user_load_parameters.no_flash = (flags & 1) == 1;
    plgldr_context.user_load_parameters.plugin_memory_strategy =
        static_cast<PluginMemoryStrategy>((flags >> 8) & 0xF);
    plgldr_context.user_load_parameters.low_title_Id = rp.Pop<u32>();

    auto path = rp.PopMappedBuffer();

    path.Read(
        plgldr_context.user_load_parameters.path, 0,
        std::min(sizeof(PluginLoaderContext::PluginLoadParameters::path) - 1, path.GetSize()));
    plgldr_context.user_load_parameters.path[std::min(
        sizeof(PluginLoaderContext::PluginLoadParameters::path) - 1, path.GetSize())] = '\0';

    auto config = rp.PopMappedBuffer();
    config.Read(
        plgldr_context.user_load_parameters.config, 0,
        std::min(sizeof(PluginLoaderContext::PluginLoadParameters::config), config.GetSize()));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void PLG_LDR::DisplayErrorMessage(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 error_code = rp.Pop<u32>();
    auto title = rp.PopMappedBuffer();
    auto desc = rp.PopMappedBuffer();

    std::vector<char> title_data(title.GetSize() + 1);
    std::vector<char> desc_data(desc.GetSize() + 1);

    title.Read(title_data.data(), 0, title.GetSize());
    title_data[title.GetSize()] = '\0';

    desc.Read(desc_data.data(), 0, desc.GetSize());
    desc_data[desc.GetSize()] = '\0';

    LOG_ERROR(Service_PLGLDR, "Plugin error - Code: {} - Title: {} - Description: {}", error_code,
              std::string(title_data.data()), std::string(desc_data.data()));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void PLG_LDR::GetPLGLDRVersion(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(plgldr_version.raw);
}

void PLG_LDR::GetArbiter(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    // NOTE: It doesn't make sense to send an arbiter in HLE, as it's used to
    // signal the plgldr service thread when a event is ready. Instead we just send
    // an error and the 3GX plugin will take care of it.
    // (We never send any events anyways)
    rb.Push(Kernel::ResultNotImplemented);
}

void PLG_LDR::GetPluginPath(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    auto path = rp.PopMappedBuffer();

    // Same behaviour as strncpy
    std::string root_sd = FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir);
    std::string plugin_path = plgldr_context.plugin_path;
    auto it = plugin_path.find(root_sd);
    if (it != plugin_path.npos)
        plugin_path.erase(it, root_sd.size());

    std::replace(plugin_path.begin(), plugin_path.end(), '\\', '/');
    if (plugin_path.empty() || plugin_path[0] != '/')
        plugin_path = "/" + plugin_path;

    path.Write(plugin_path.c_str(), 0, std::min(path.GetSize(), plugin_path.length() + 1));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(path);
}

std::shared_ptr<PLG_LDR> GetService(Core::System& system) {
    if (!system.KernelRunning())
        return nullptr;
    auto it = system.Kernel().named_ports.find("plg:ldr");
    if (it != system.Kernel().named_ports.end())
        return std::static_pointer_cast<PLG_LDR>(it->second->GetServerPort()->hle_handler);
    return nullptr;
}

void InstallInterfaces(Core::System& system) {
    std::make_shared<PLG_LDR>(system)->InstallAsNamedPort(system.Kernel());
}

} // namespace Service::PLGLDR
