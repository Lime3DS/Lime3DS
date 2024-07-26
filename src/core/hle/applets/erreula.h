// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/applets/applet.h"
#include "core/hle/kernel/shared_memory.h"

namespace HLE::Applets {

class ErrEula final : public Applet {
public:
    explicit ErrEula(Core::System& system, Service::APT::AppletId id, Service::APT::AppletId parent,
                     bool preload, std::weak_ptr<Service::APT::AppletManager> manager)
        : Applet(system, id, parent, preload, std::move(manager)) {}

    Result ReceiveParameterImpl(const Service::APT::MessageParameter& parameter) override;
    Result Start(const Service::APT::MessageParameter& parameter) override;
    Result Finalize() override;
    void Update() override;

private:
    /// This SharedMemory will be created when we receive the LibAppJustStarted message.
    /// It holds the framebuffer info retrieved by the application with
    /// GSPGPU::ImportDisplayCaptureInfo
    std::shared_ptr<Kernel::SharedMemory> framebuffer_memory;

    /// Parameter received by the applet on start.
    std::vector<u8> startup_param;
};

} // namespace HLE::Applets
