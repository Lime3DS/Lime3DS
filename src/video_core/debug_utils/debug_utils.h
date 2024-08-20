// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <condition_variable>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "common/common_types.h"
#include "video_core/pica/regs_rasterizer.h"

namespace CiTrace {
class Recorder;
}

namespace Pica {

struct ShaderRegs;
struct ShaderSetup;

class DebugContext {
public:
    enum class Event {
        FirstEvent = 0,

        PicaCommandLoaded = FirstEvent,
        PicaCommandProcessed,
        IncomingPrimitiveBatch,
        FinishedPrimitiveBatch,
        VertexShaderInvocation,
        IncomingDisplayTransfer,
        GSPCommandProcessed,
        BufferSwapped,

        NumEvents
    };

    /**
     * Inherit from this class to be notified of events registered to some debug context.
     * Most importantly this is used for our debugger GUI.
     *
     * To implement event handling, override the OnPicaBreakPointHit and OnPicaResume methods.
     * @warning All BreakPointObservers need to be on the same thread to guarantee thread-safe state
     * access
     * @todo Evaluate an alternative interface, in which there is only one managing observer and
     * multiple child observers running (by design) on the same thread.
     */
    class BreakPointObserver {
    public:
        /// Constructs the object such that it observes events of the given DebugContext.
        BreakPointObserver(std::shared_ptr<DebugContext> debug_context)
            : context_weak(debug_context) {
            std::unique_lock lock{debug_context->breakpoint_mutex};
            debug_context->breakpoint_observers.push_back(this);
        }

        virtual ~BreakPointObserver() {
            auto context = context_weak.lock();
            if (context) {
                std::unique_lock lock(context->breakpoint_mutex);
                context->breakpoint_observers.remove(this);

                // If we are the last observer to be destroyed, tell the debugger context that
                // it is free to continue. In particular, this is required for a proper Lime3DS
                // shutdown, when the emulation thread is waiting at a breakpoint.
                if (context->breakpoint_observers.empty())
                    context->Resume();
            }
        }

        /**
         * Action to perform when a breakpoint was reached.
         * @param event Type of event which triggered the breakpoint
         * @param data Optional data pointer (if unused, this is a nullptr)
         * @note This function will perform nothing unless it is overridden in the child class.
         */
        virtual void OnPicaBreakPointHit(Event event, const void* data) {}

        /**
         * Action to perform when emulation is resumed from a breakpoint.
         * @note This function will perform nothing unless it is overridden in the child class.
         */
        virtual void OnPicaResume() {}

    protected:
        /**
         * Weak context pointer. This need not be valid, so when requesting a shared_ptr via
         * context_weak.lock(), always compare the result against nullptr.
         */
        std::weak_ptr<DebugContext> context_weak;
    };

    /**
     * Simple structure defining a breakpoint state
     */
    struct BreakPoint {
        bool enabled = false;
    };

    /**
     * Static constructor used to create a shared_ptr of a DebugContext.
     */
    static std::shared_ptr<DebugContext> Construct() {
        return std::shared_ptr<DebugContext>(new DebugContext);
    }

    /**
     * Used by the emulation core when a given event has happened. If a breakpoint has been set
     * for this event, OnEvent calls the event handlers of the registered breakpoint observers.
     * The current thread then is halted until Resume() is called from another thread (or until
     * emulation is stopped).
     * @param event Event which has happened
     * @param data Optional data pointer (pass nullptr if unused). Needs to remain valid until
     * Resume() is called.
     */
    void OnEvent(Event event, const void* data) {
        // This check is left in the header to allow the compiler to inline it.
        if (!breakpoints[(int)event].enabled)
            return;
        // For the rest of event handling, call a separate function.
        DoOnEvent(event, data);
    }

    void DoOnEvent(Event event, const void* data);

    /**
     * Resume from the current breakpoint.
     * @warning Calling this from the same thread that OnEvent was called in will cause a deadlock.
     * Calling from any other thread is safe.
     */
    void Resume();

    /**
     * Delete all set breakpoints and resume emulation.
     */
    void ClearBreakpoints() {
        for (auto& bp : breakpoints) {
            bp.enabled = false;
        }
        Resume();
    }

    // TODO: Evaluate if access to these members should be hidden behind a public interface.
    std::array<BreakPoint, (int)Event::NumEvents> breakpoints;
    Event active_breakpoint;
    bool at_breakpoint = false;

    std::shared_ptr<CiTrace::Recorder> recorder = nullptr;

private:
    /**
     * Private default constructor to make sure people always construct this through Construct()
     * instead.
     */
    DebugContext() = default;

    /// Mutex protecting current breakpoint state and the observer list.
    std::mutex breakpoint_mutex;

    /// Used by OnEvent to wait for resumption.
    std::condition_variable resume_from_breakpoint;

    /// List of registered observers
    std::list<BreakPointObserver*> breakpoint_observers;
};

extern std::shared_ptr<DebugContext> g_debug_context; // TODO: Get rid of this global

namespace DebugUtils {

void DumpShader(const std::string& filename, const ShaderRegs& config, const ShaderSetup& setup,
                const RasterizerRegs::VSOutputAttributes* output_attributes);

// Utility class to log Pica commands.
struct PicaTrace {
    struct Write {
        u16 cmd_id;
        u16 mask;
        u32 value;
    };
    std::vector<Write> writes;
};

extern bool g_is_pica_tracing;

void StartPicaTracing();
inline bool IsPicaTracing() {
    return g_is_pica_tracing;
}
void OnPicaRegWrite(u16 cmd_id, u16 mask, u32 value);
std::unique_ptr<PicaTrace> FinishPicaTracing();

} // namespace DebugUtils

} // namespace Pica
