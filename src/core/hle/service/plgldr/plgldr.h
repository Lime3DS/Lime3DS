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

#pragma once

#include <memory>
#include <boost/serialization/export.hpp>
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::PLGLDR {

class PLG_LDR final : public ServiceFramework<PLG_LDR> {
public:
    enum class PluginMemoryStrategy : u8 {
        PLG_STRATEGY_NONE = 2,
        PLG_STRATEGY_SWAP = 0,
        PLG_STRATEGY_MODE3 = 1,
    };

    struct PluginLoaderContext {
        struct PluginLoadParameters {
            u8 no_flash = 0;
            PluginMemoryStrategy plugin_memory_strategy = PluginMemoryStrategy::PLG_STRATEGY_SWAP;
            u32_le low_title_Id = 0;
            char path[256] = {0};
            u32_le config[32] = {0};

            template <class Archive>
            void serialize(Archive& ar, const unsigned int) {
                ar & no_flash;
                ar & plugin_memory_strategy;
                ar & low_title_Id;
                ar & path;
                ar & config;
            }
            friend class boost::serialization::access;
        };
        bool is_enabled = true;
        bool allow_game_change = true;
        bool plugin_loaded = false;
        bool is_default_path = false;
        std::string plugin_path = "";

        bool use_user_load_parameters = false;
        PluginLoadParameters user_load_parameters;

        VAddr plg_event = 0;
        VAddr plg_reply = 0;
        Kernel::Handle memory_changed_handle = 0;

        bool is_exe_load_function_set = false;
        u32 exe_load_checksum = 0;

        std::vector<u32> load_exe_func;
        u32_le load_exe_args[4] = {0};

        PAddr plugin_fb_addr = 0;

        template <class Archive>
        void serialize(Archive& ar, const unsigned int);
        friend class boost::serialization::access;
    };

    PLG_LDR(Core::System& system_);
    ~PLG_LDR() {}

    void OnProcessRun(Kernel::Process& process, Kernel::KernelSystem& kernel);
    void OnProcessExit(Kernel::Process& process, Kernel::KernelSystem& kernel);
    ResultVal<Kernel::Handle> GetMemoryChangedHandle(Kernel::KernelSystem& kernel);
    void OnMemoryChanged(Kernel::Process& process, Kernel::KernelSystem& kernel);

    PluginLoaderContext& GetPluginLoaderContext() {
        return plgldr_context;
    }
    void SetPluginLoaderContext(PluginLoaderContext& context) {
        plgldr_context = context;
    }
    void SetEnabled(bool enabled) {
        plgldr_context.is_enabled = enabled;
    }
    bool GetEnabled() {
        return plgldr_context.is_enabled;
    }
    void SetAllowGameChangeState(bool allow) {
        plgldr_context.allow_game_change = allow;
    }
    bool GetAllowGameChangeState() {
        return plgldr_context.allow_game_change;
    }
    void SetPluginFBAddr(PAddr addr) {
        plgldr_context.plugin_fb_addr = addr;
    }
    PAddr GetPluginFBAddr() {
        return plgldr_context.plugin_fb_addr;
    }

private:
    Core::System& system;

    PluginLoaderContext plgldr_context;

    void IsEnabled(Kernel::HLERequestContext& ctx);
    void SetEnabled(Kernel::HLERequestContext& ctx);
    void SetLoadSettings(Kernel::HLERequestContext& ctx);
    void DisplayErrorMessage(Kernel::HLERequestContext& ctx);
    void GetPLGLDRVersion(Kernel::HLERequestContext& ctx);
    void GetArbiter(Kernel::HLERequestContext& ctx);
    void GetPluginPath(Kernel::HLERequestContext& ctx);

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

std::shared_ptr<PLG_LDR> GetService(Core::System& system);

void InstallInterfaces(Core::System& system);

} // namespace Service::PLGLDR

BOOST_CLASS_EXPORT_KEY(Service::PLGLDR::PLG_LDR)
SERVICE_CONSTRUCT(Service::PLGLDR::PLG_LDR)
