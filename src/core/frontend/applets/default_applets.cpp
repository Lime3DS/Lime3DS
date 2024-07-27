// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/frontend/applets/default_applets.h"
#include "core/frontend/applets/mii_selector.h"
#include "core/frontend/applets/swkbd.h"

namespace Frontend {
void RegisterDefaultApplets(Core::System& system) {
    system.RegisterSoftwareKeyboard(std::make_shared<DefaultKeyboard>(system));
    system.RegisterMiiSelector(std::make_shared<DefaultMiiSelector>());
}
} // namespace Frontend
