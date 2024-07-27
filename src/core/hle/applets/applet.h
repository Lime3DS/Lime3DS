// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/result.h"
#include "core/hle/service/apt/applet_manager.h"

namespace HLE::Applets {

class Applet {
public:
    virtual ~Applet() = default;

    /**
     * Handles a parameter from the application.
     * @param parameter Parameter data to handle.
     * @returns Result Whether the operation was successful or not.
     */
    Result ReceiveParameter(const Service::APT::MessageParameter& parameter);

    /**
     * Whether the applet is currently running.
     */
    [[nodiscard]] bool IsRunning() const;

    /**
     * Whether the applet is currently active instead of the host application or not.
     */
    [[nodiscard]] bool IsActive() const;

    /**
     * Handles an update tick for the Applet, lets it update the screen, send commands, etc.
     */
    virtual void Update() = 0;

protected:
    Applet(Core::System& system, Service::APT::AppletId id, Service::APT::AppletId parent,
           bool preload, std::weak_ptr<Service::APT::AppletManager> manager)
        : system(system), id(id), parent(parent), preload(preload), manager(std::move(manager)) {}

    /**
     * Handles a parameter from the application.
     * @param parameter Parameter data to handle.
     * @returns Result Whether the operation was successful or not.
     */
    virtual Result ReceiveParameterImpl(const Service::APT::MessageParameter& parameter) = 0;

    /**
     * Handles the Applet start event, triggered from the application.
     * @param parameter Parameter data to handle.
     * @returns Result Whether the operation was successful or not.
     */
    virtual Result Start(const Service::APT::MessageParameter& parameter) = 0;

    /**
     * Sends the LibAppletClosing signal to the application,
     * along with the relevant data buffers.
     */
    virtual Result Finalize() = 0;

    Core::System& system;

    Service::APT::AppletId id;                    ///< Id of this Applet
    Service::APT::AppletId parent;                ///< Id of this Applet's parent
    bool preload;                                 ///< Whether the Applet is being preloaded.
    std::shared_ptr<std::vector<u8>> heap_memory; ///< Heap memory for this Applet

    /// Whether this applet is running.
    bool is_running = true;

    /// Whether this applet is currently active instead of the host application or not.
    bool is_active = false;

    void SendParameter(const Service::APT::MessageParameter& parameter);
    void CloseApplet(std::shared_ptr<Kernel::Object> object, const std::vector<u8>& buffer);

private:
    std::weak_ptr<Service::APT::AppletManager> manager;
};

} // namespace HLE::Applets
