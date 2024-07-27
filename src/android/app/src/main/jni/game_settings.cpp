// Copyright 2019 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/settings.h"

namespace GameSettings {

void LoadOverrides(u64 program_id) {
    switch (program_id) {
        // JAP / The Legend of Zelda: Ocarina of Time 3D
    case 0x0004000000033400:
        // USA / The Legend of Zelda: Ocarina of Time 3D
    case 0x0004000000033500:
        // EUR / The Legend of Zelda: Ocarina of Time 3D
    case 0x0004000000033600:
        // KOR / The Legend of Zelda: Ocarina of Time 3D
    case 0x000400000008F800:
        // CHI / The Legend of Zelda: Ocarina of Time 3D
    case 0x000400000008F900:
        // This game requires accurate multiplication to render properly
        Settings::values.shaders_accurate_mul = true;
        break;

        // USA / Mario & Luigi: Superstar Saga + Bowsers Minions
    case 0x00040000001B8F00:
        // EUR / Mario & Luigi: Superstar Saga + Bowsers Minions
    case 0x00040000001B9000:
        // JAP / Mario & Luigi: Superstar Saga + Bowsers Minions
    case 0x0004000000194B00:
        // This game requires accurate multiplication to render properly
        Settings::values.shaders_accurate_mul = true;
        break;

        // USA / Mario & Luigi: Bowsers Inside Story + Bowser Jrs Journey
    case 0x00040000001D1400:
        // EUR / Mario & Luigi: Bowsers Inside Story + Bowser Jrs Journey
    case 0x00040000001D1500:
        // This game requires accurate multiplication to render properly
        Settings::values.shaders_accurate_mul = true;
        break;
    }
}

} // namespace GameSettings
