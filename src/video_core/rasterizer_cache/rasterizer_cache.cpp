// Copyright 2022 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/microprofile.h"

namespace VideoCore {

MICROPROFILE_DEFINE(RasterizerCache_CopySurface, "RasterizerCache", "CopySurface",
                    MP_RGB(128, 192, 64));
MICROPROFILE_DEFINE(RasterizerCache_UploadSurface, "RasterizerCache", "UploadSurface",
                    MP_RGB(128, 192, 64));
MICROPROFILE_DEFINE(RasterizerCache_DownloadSurface, "RasterizerCache", "DownloadSurface",
                    MP_RGB(128, 192, 64));
MICROPROFILE_DEFINE(RasterizerCache_Invalidation, "RasterizerCache", "Invalidation",
                    MP_RGB(128, 64, 192));

} // namespace VideoCore
