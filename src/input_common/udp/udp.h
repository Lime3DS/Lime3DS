// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "input_common/udp/client.h"

namespace InputCommon::CemuhookUDP {

class State {
public:
    State();
    ~State();
    void ReloadUDPClient();

private:
    std::unique_ptr<Client> client;
};

std::unique_ptr<State> Init();

} // namespace InputCommon::CemuhookUDP
