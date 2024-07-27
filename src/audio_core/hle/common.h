// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstddef>

namespace AudioCore::HLE {

constexpr std::size_t num_sources = 24;

/**
 * This performs the filter operation defined by FilterT::ProcessSample on the frame in-place.
 * FilterT::ProcessSample is called sequentially on the samples.
 */
template <typename FrameT, typename FilterT>
void FilterFrame(FrameT& frame, FilterT& filter) {
    std::transform(frame.begin(), frame.end(), frame.begin(),
                   [&filter](const auto& sample) { return filter.ProcessSample(sample); });
}

} // namespace AudioCore::HLE
