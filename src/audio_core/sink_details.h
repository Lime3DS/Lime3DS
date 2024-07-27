// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace AudioCore {

class Sink;

enum class SinkType : u32 {
    Auto = 0,
    Null = 1,
    Cubeb = 2,
    OpenAL = 3,
    SDL2 = 4,
};

struct SinkDetails {
    using FactoryFn = std::unique_ptr<Sink> (*)(std::string_view);
    using ListDevicesFn = std::vector<std::string> (*)();

    /// Type of this sink.
    SinkType type;
    /// Name for this sink.
    std::string_view name;
    /// A method to call to construct an instance of this type of sink.
    FactoryFn create_sink;
    /// A method to call to list available devices.
    ListDevicesFn list_devices;
};

/// Lists all available sink types.
std::vector<SinkDetails> ListSinks();

/// Gets the details of an sink type.
const SinkDetails& GetSinkDetails(SinkType input_type);

} // namespace AudioCore
