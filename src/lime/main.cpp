#include <iostream>

#ifdef ENABLE_QT
#include "lime_qt/lime_qt.h"
#endif
#ifdef ENABLE_SDL2_FRONTEND
#include "lime_sdl/lime_sdl.h"
#endif

int main(int argc, char* argv[]) {
#if ENABLE_QT
    bool no_gui = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-')
            continue;

        if (argv[i][1] == 'n') {
            no_gui = true;
        }
    }

    if (!no_gui) {
        LaunchQtFrontend(argc, argv);
        return 0;
    }
#endif

#if ENABLE_SDL2_FRONTEND
    LaunchSdlFrontend(argc, argv);
#else
    std::cout << "Cannot use SDL frontend as it was excluded at compile time. Exiting."
              << std::endl;
    return -1;
#endif

    return 0;
}