// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Service::CAM {

enum CameraIndex {
    OuterRightCamera = 0,
    InnerCamera = 1,
    OuterLeftCamera = 2,

    NumCameras = 3,
};

enum class Effect : u8 {
    NoEffect = 0,
    Mono = 1,
    Sepia = 2,
    Negative = 3,
    Negafilm = 4,
    Sepia01 = 5,
};

enum class Flip : u8 {
    NoFlip = 0,
    Horizontal = 1,
    Vertical = 2,
    Reverse = 3,
};

enum class Size : u8 {
    VGA = 0,
    QVGA = 1,
    QQVGA = 2,
    CIF = 3,
    QCIF = 4,
    DS_LCD = 5,
    DS_LCDx4 = 6,
    CTR_TOP_LCD = 7,
    CTR_BOTTOM_LCD = QVGA,
};

enum class FrameRate : u8 {
    Rate_15 = 0,
    Rate_15_To_5 = 1,
    Rate_15_To_2 = 2,
    Rate_10 = 3,
    Rate_8_5 = 4,
    Rate_5 = 5,
    Rate_20 = 6,
    Rate_20_To_5 = 7,
    Rate_30 = 8,
    Rate_30_To_5 = 9,
    Rate_15_To_10 = 10,
    Rate_20_To_10 = 11,
    Rate_30_To_10 = 12,
};

enum class ShutterSoundType : u8 {
    Normal = 0,
    Movie = 1,
    MovieEnd = 2,
};

enum class WhiteBalance : u8 {
    BalanceAuto = 0,
    Balance3200K = 1,
    Balance4150K = 2,
    Balance5200K = 3,
    Balance6000K = 4,
    Balance7000K = 5,
    BalanceMax = 6,
    BalanceNormal = BalanceAuto,
    BalanceTungsten = Balance3200K,
    BalanceWhiteFluorescentLight = Balance4150K,
    BalanceDaylight = Balance5200K,
    BalanceCloudy = Balance6000K,
    BalanceHorizon = Balance6000K,
    BalanceShade = Balance7000K,
};

enum class PhotoMode : u8 {
    Normal = 0,
    Portrait = 1,
    Landscape = 2,
    Nightview = 3,
    Letter0 = 4,
};

enum class LensCorrection : u8 {
    Off = 0,
    On70 = 1,
    On90 = 2,
    Dark = Off,
    Normal = On70,
    Bright = On90,
};

enum class Contrast : u8 {
    Pattern01 = 1,
    Pattern02 = 2,
    Pattern03 = 3,
    Pattern04 = 4,
    Pattern05 = 5,
    Pattern06 = 6,
    Pattern07 = 7,
    Pattern08 = 8,
    Pattern09 = 9,
    Pattern10 = 10,
    Pattern11 = 11,
    Low = Pattern05,
    Normal = Pattern06,
    High = Pattern07,
};

enum class OutputFormat : u8 {
    YUV422 = 0,
    RGB565 = 1,
};

} // namespace Service::CAM
