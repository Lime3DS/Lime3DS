// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Memory {
class MemorySystem;
}

namespace Service::Y2R {
struct ConversionConfiguration;
} // namespace Service::Y2R

namespace HW::Y2R {
void PerformConversion(Memory::MemorySystem& memory, Service::Y2R::ConversionConfiguration cvt);
} // namespace HW::Y2R
