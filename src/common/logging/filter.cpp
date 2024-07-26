// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include "common/assert.h"
#include "common/logging/filter.h"
#include "common/string_util.h"

namespace Common::Log {
namespace {
template <typename It>
Level GetLevelByName(const It begin, const It end) {
    for (u8 i = 0; i < static_cast<u8>(Level::Count); ++i) {
        const char* level_name = GetLevelName(static_cast<Level>(i));
        if (Common::ComparePartialString(begin, end, level_name)) {
            return static_cast<Level>(i);
        }
    }
    return Level::Count;
}

template <typename It>
Class GetClassByName(const It begin, const It end) {
    for (u8 i = 0; i < static_cast<u8>(Class::Count); ++i) {
        const char* level_name = GetLogClassName(static_cast<Class>(i));
        if (Common::ComparePartialString(begin, end, level_name)) {
            return static_cast<Class>(i);
        }
    }
    return Class::Count;
}

template <typename Iterator>
bool ParseFilterRule(Filter& instance, Iterator begin, Iterator end) {
    auto level_separator = std::find(begin, end, ':');
    if (level_separator == end) {
        LOG_ERROR(Log, "Invalid log filter. Must specify a log level after `:`: {}",
                  std::string(begin, end));
        return false;
    }

    const Level level = GetLevelByName(level_separator + 1, end);
    if (level == Level::Count) {
        LOG_ERROR(Log, "Unknown log level in filter: {}", std::string(begin, end));
        return false;
    }

    if (Common::ComparePartialString(begin, level_separator, "*")) {
        instance.ResetAll(level);
        return true;
    }

    const Class log_class = GetClassByName(begin, level_separator);
    if (log_class == Class::Count) {
        LOG_ERROR(Log, "Unknown log class in filter: {}", std::string(begin, end));
        return false;
    }

    instance.SetClassLevel(log_class, level);
    return true;
}
} // Anonymous namespace

/// Macro listing all log classes. Code should define CLS and SUB as desired before invoking this.
#define ALL_LOG_CLASSES()                                                                          \
    CLS(Log)                                                                                       \
    CLS(Common)                                                                                    \
    SUB(Common, Filesystem)                                                                        \
    SUB(Common, Memory)                                                                            \
    CLS(Core)                                                                                      \
    SUB(Core, ARM11)                                                                               \
    SUB(Core, Timing)                                                                              \
    SUB(Core, Cheats)                                                                              \
    CLS(Config)                                                                                    \
    CLS(Debug)                                                                                     \
    SUB(Debug, Emulated)                                                                           \
    SUB(Debug, GPU)                                                                                \
    SUB(Debug, Breakpoint)                                                                         \
    SUB(Debug, GDBStub)                                                                            \
    CLS(Kernel)                                                                                    \
    SUB(Kernel, SVC)                                                                               \
    CLS(Applet)                                                                                    \
    SUB(Applet, SWKBD)                                                                             \
    CLS(Service)                                                                                   \
    SUB(Service, SRV)                                                                              \
    SUB(Service, FRD)                                                                              \
    SUB(Service, FS)                                                                               \
    SUB(Service, ERR)                                                                              \
    SUB(Service, ACT)                                                                              \
    SUB(Service, APT)                                                                              \
    SUB(Service, BOSS)                                                                             \
    SUB(Service, GSP)                                                                              \
    SUB(Service, AC)                                                                               \
    SUB(Service, AM)                                                                               \
    SUB(Service, PTM)                                                                              \
    SUB(Service, LDR)                                                                              \
    SUB(Service, MIC)                                                                              \
    SUB(Service, NDM)                                                                              \
    SUB(Service, NFC)                                                                              \
    SUB(Service, NIM)                                                                              \
    SUB(Service, NS)                                                                               \
    SUB(Service, NWM)                                                                              \
    SUB(Service, CAM)                                                                              \
    SUB(Service, CECD)                                                                             \
    SUB(Service, CFG)                                                                              \
    SUB(Service, CSND)                                                                             \
    SUB(Service, DSP)                                                                              \
    SUB(Service, DLP)                                                                              \
    SUB(Service, HID)                                                                              \
    SUB(Service, HTTP)                                                                             \
    SUB(Service, SOC)                                                                              \
    SUB(Service, IR)                                                                               \
    SUB(Service, Y2R)                                                                              \
    SUB(Service, PS)                                                                               \
    SUB(Service, PLGLDR)                                                                           \
    SUB(Service, NEWS)                                                                             \
    CLS(HW)                                                                                        \
    SUB(HW, Memory)                                                                                \
    SUB(HW, LCD)                                                                                   \
    SUB(HW, GPU)                                                                                   \
    SUB(HW, AES)                                                                                   \
    CLS(Frontend)                                                                                  \
    CLS(Render)                                                                                    \
    SUB(Render, Software)                                                                          \
    SUB(Render, OpenGL)                                                                            \
    SUB(Render, Vulkan)                                                                            \
    CLS(Audio)                                                                                     \
    SUB(Audio, DSP)                                                                                \
    SUB(Audio, Sink)                                                                               \
    CLS(Input)                                                                                     \
    CLS(Network)                                                                                   \
    CLS(Movie)                                                                                     \
    CLS(Loader)                                                                                    \
    CLS(WebService)                                                                                \
    CLS(RPC_Server)

// GetClassName is a macro defined by Windows.h, grrr...
const char* GetLogClassName(Class log_class) {
    switch (log_class) {
#define CLS(x)                                                                                     \
    case Class::x:                                                                                 \
        return #x;
#define SUB(x, y)                                                                                  \
    case Class::x##_##y:                                                                           \
        return #x "." #y;
        ALL_LOG_CLASSES()
#undef CLS
#undef SUB
    case Class::Count:
    default:
        break;
    }
    UNREACHABLE();
}

const char* GetLevelName(Level log_level) {
#define LVL(x)                                                                                     \
    case Level::x:                                                                                 \
        return #x
    switch (log_level) {
        LVL(Trace);
        LVL(Debug);
        LVL(Info);
        LVL(Warning);
        LVL(Error);
        LVL(Critical);
    case Level::Count:
    default:
        break;
    }
#undef LVL
    UNREACHABLE();
}

Filter::Filter(Level default_level) {
    ResetAll(default_level);
}

void Filter::ResetAll(Level level) {
    class_levels.fill(level);
}

void Filter::SetClassLevel(Class log_class, Level level) {
    class_levels[static_cast<std::size_t>(log_class)] = level;
}

void Filter::ParseFilterString(std::string_view filter_view) {
    auto clause_begin = filter_view.cbegin();
    while (clause_begin != filter_view.cend()) {
        auto clause_end = std::find(clause_begin, filter_view.cend(), ' ');

        // If clause isn't empty
        if (clause_end != clause_begin) {
            ParseFilterRule(*this, clause_begin, clause_end);
        }

        if (clause_end != filter_view.cend()) {
            // Skip over the whitespace
            ++clause_end;
        }
        clause_begin = clause_end;
    }
}

bool Filter::CheckMessage(Class log_class, Level level) const {
    return static_cast<u8>(level) >=
           static_cast<u8>(class_levels[static_cast<std::size_t>(log_class)]);
}

bool Filter::IsDebug() const {
    return std::any_of(class_levels.begin(), class_levels.end(), [](const Level& l) {
        return static_cast<u8>(l) <= static_cast<u8>(Level::Debug);
    });
}

} // namespace Common::Log
