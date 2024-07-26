// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "core/dumping/backend.h"

namespace VideoDumper {

VideoFrame::VideoFrame(std::size_t width_, std::size_t height_, u8* data_)
    : width(width_), height(height_), stride(static_cast<u32>(width * 4)),
      data(data_, data_ + width * height * 4) {}

Backend::~Backend() = default;
NullBackend::~NullBackend() = default;

} // namespace VideoDumper