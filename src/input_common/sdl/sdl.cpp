// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "input_common/sdl/sdl.h"
#ifdef HAVE_SDL2
#include "input_common/sdl/sdl_impl.h"
#endif

namespace InputCommon::SDL {

std::unique_ptr<State> Init() {
#ifdef HAVE_SDL2
    return std::make_unique<SDLState>();
#else
    return std::make_unique<NullState>();
#endif
}
} // namespace InputCommon::SDL
