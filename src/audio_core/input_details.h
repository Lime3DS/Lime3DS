// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace Core {
class System;
}

namespace AudioCore {

class Input;

enum class InputType : u32 {
    Auto = 0,
    Null = 1,
    Static = 2,
    Cubeb = 3,
    OpenAL = 4,
};

struct InputDetails {
    using FactoryFn = std::unique_ptr<Input> (*)(Core::System& system, std::string_view device_id);
    using ListDevicesFn = std::vector<std::string> (*)();

    /// Type of this input.
    InputType type;
    /// Name for this input.
    std::string_view name;
    /// Whether the input is backed by real devices.
    bool real;
    /// A method to call to construct an instance of this type of input.
    FactoryFn create_input;
    /// A method to call to list available devices.
    ListDevicesFn list_devices;
};

/// Lists all available input types.
std::vector<InputDetails> ListInputs();

/// Gets the details of an input type.
const InputDetails& GetInputDetails(InputType input_type);

} // namespace AudioCore
