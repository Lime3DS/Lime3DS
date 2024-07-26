// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <functional>
#include <vector>
#include "common/logging/log.h"
#include "core/hle/service/gsp/gsp_command.h"

namespace VideoCore {

class GraphicsDebugger {
public:
    // Base class for all objects which need to be notified about GPU events
    class DebuggerObserver {
        friend class GraphicsDebugger;

    public:
        DebuggerObserver() = default;
        virtual ~DebuggerObserver() {
            if (observed) {
                observed->UnregisterObserver(this);
            }
        }

        /**
         * Called when a GX command has been processed and is ready for being
         * read via GraphicsDebugger::ReadGXCommandHistory.
         * @param total_command_count Total number of commands in the GX history
         * @note All methods in this class are called from the GSP thread
         */
        virtual void GXCommandProcessed(int total_command_count) {
            [[maybe_unused]] const Service::GSP::Command& cmd =
                observed->ReadGXCommandHistory(total_command_count - 1);
            LOG_TRACE(Debug_GPU, "Received command: id={:x}", (int)cmd.id.Value());
        }

    protected:
        const GraphicsDebugger* GetDebugger() const {
            return observed;
        }

    private:
        GraphicsDebugger* observed{};
    };

    void GXCommandProcessed(Service::GSP::Command& command_data) {
        if (observers.empty()) {
            return;
        }

        gx_command_history.emplace_back(command_data);
        ForEachObserver([this](DebuggerObserver* observer) {
            observer->GXCommandProcessed(static_cast<int>(this->gx_command_history.size()));
        });
    }

    const Service::GSP::Command& ReadGXCommandHistory(int index) const {
        // TODO: Is this thread-safe?
        return gx_command_history[index];
    }

    void RegisterObserver(DebuggerObserver* observer) {
        // TODO: Check for duplicates
        observers.push_back(observer);
        observer->observed = this;
    }

    void UnregisterObserver(DebuggerObserver* observer) {
        observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
        observer->observed = nullptr;
    }

private:
    void ForEachObserver(std::function<void(DebuggerObserver*)> func) {
        std::for_each(observers.begin(), observers.end(), func);
    }

    std::vector<DebuggerObserver*> observers;
    std::vector<Service::GSP::Command> gx_command_history;
};

} // namespace VideoCore
