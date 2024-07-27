// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "audio_core/audio_types.h"
#include "audio_core/hle/shared_memory.h"
#include "common/common_types.h"

namespace AudioCore::HLE {

/// Preprocessing filters. There is an independent set of filters for each Source.
class SourceFilters final {
public:
    SourceFilters() {
        Reset();
    }

    /// Reset internal state.
    void Reset();

    /**
     * Enable/Disable filters
     * See also: SourceConfiguration::Configuration::simple_filter_enabled,
     *           SourceConfiguration::Configuration::biquad_filter_enabled.
     * @param simple If true, enables the simple filter. If false, disables it.
     * @param biquad If true, enables the biquad filter. If false, disables it.
     */
    void Enable(bool simple, bool biquad);

    /**
     * Configure simple filter.
     * @param config Configuration from DSP shared memory.
     */
    void Configure(SourceConfiguration::Configuration::SimpleFilter config);

    /**
     * Configure biquad filter.
     * @param config Configuration from DSP shared memory.
     */
    void Configure(SourceConfiguration::Configuration::BiquadFilter config);

    /**
     * Processes a frame in-place.
     * @param frame Audio samples to process. Modified in-place.
     */
    void ProcessFrame(StereoFrame16& frame);

private:
    bool simple_filter_enabled;
    bool biquad_filter_enabled;

    struct SimpleFilter {
        SimpleFilter() {
            Reset();
        }

        /// Resets internal state.
        void Reset();

        /**
         * Configures this filter with application settings.
         * @param config Configuration from DSP shared memory.
         */
        void Configure(SourceConfiguration::Configuration::SimpleFilter config);

        /**
         * Processes a single stereo PCM16 sample.
         * @param x0 Input sample
         * @return Output sample
         */
        std::array<s16, 2> ProcessSample(const std::array<s16, 2>& x0);

    private:
        // Configuration
        s32 a1, b0;
        // Internal state
        std::array<s16, 2> y1;
    } simple_filter;

    struct BiquadFilter {
        BiquadFilter() {
            Reset();
        }

        /// Resets internal state.
        void Reset();

        /**
         * Configures this filter with application settings.
         * @param config Configuration from DSP shared memory.
         */
        void Configure(SourceConfiguration::Configuration::BiquadFilter config);

        /**
         * Processes a single stereo PCM16 sample.
         * @param x0 Input sample
         * @return Output sample
         */
        std::array<s16, 2> ProcessSample(const std::array<s16, 2>& x0);

    private:
        // Configuration
        s32 a1, a2, b0, b1, b2;
        // Internal state
        std::array<s16, 2> x1;
        std::array<s16, 2> x2;
        std::array<s16, 2> y1;
        std::array<s16, 2> y2;
    } biquad_filter;
};

} // namespace AudioCore::HLE
