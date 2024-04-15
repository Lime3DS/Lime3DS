// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <boost/serialization/access.hpp>
#include "audio_core/audio_types.h"
#include "audio_core/hle/common.h"
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"

namespace AudioCore::HLE {

// The application-accessible region of DSP memory consists of two parts. Both are marked as IO and
// have Read/Write permissions.
//
// First Region:  0x1FF50000 (Size: 0x8000)
// Second Region: 0x1FF70000 (Size: 0x8000)
//
// The DSP reads from each region alternately based on the frame counter for each region much like a
// double-buffer. The frame counter is located as the very last u16 of each region and is
// incremented each audio tick.

constexpr u32 region0_offset = 0x50000;
constexpr u32 region1_offset = 0x70000;

/**
 * The DSP is native 16-bit. The DSP also appears to be big-endian. When reading 32-bit numbers from
 * its memory regions, the higher and lower 16-bit halves are swapped compared to the little-endian
 * layout of the ARM11. Hence from the ARM11's point of view the memory space appears to be
 * middle-endian.
 *
 * Unusually this does not appear to be an issue for floating point numbers. The DSP makes the more
 * sensible choice of keeping that little-endian. There are also some exceptions such as the
 * IntermediateMixSamples structure, which is little-endian.
 *
 * This struct implements the conversion to and from this middle-endianness.
 */
struct u32_dsp {
    u32_dsp() = default;
    operator u32() const {
        return Convert(storage);
    }
    void operator=(u32 new_value) {
        storage = Convert(new_value);
    }

private:
    static constexpr u32 Convert(u32 value) {
        return (value << 16) | (value >> 16);
    }
    u32_le storage;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & storage;
    }
    friend class boost::serialization::access;
};
static_assert(std::is_trivially_copyable<u32_dsp>::value, "u32_dsp isn't trivially copyable");

// There are 15 structures in each memory region. A table of them in the order they appear in memory
// is presented below:
//
//       #           First Region DSP Address   Purpose                               Control
//       5           0x8400                     DSP Status                            DSP
//       9           0x8410                     DSP Debug Info                        DSP
//       6           0x8540                     Final Mix Samples                     DSP
//       2           0x8680                     Source Status [24]                    DSP
//       8           0x8710                     Compressor Table                      Application
//       4           0x9430                     DSP Configuration                     Application
//       7           0x9492                     Intermediate Mix Samples              DSP + App
//       1           0x9E92                     Source Configuration [24]             Application
//       3           0xA792                     Source ADPCM Coefficients [24]        Application
//       10          0xA912                     Surround Sound Related
//       11          0xAA12                     Surround Sound Related
//       12          0xAAD2                     Surround Sound Related
//       13          0xAC52                     Surround Sound Related
//       14          0xAC5C                     Surround Sound Related
//       0           0xBFFF                     Frame Counter                         Application
//
// #: This refers to the order in which they appear in the DspPipe::Audio DSP pipe.
//    See also: HLE::PipeRead.
//
// Note that the above addresses do vary slightly between audio firmwares observed; the addresses
// are not fixed in stone. The addresses above are only an examplar; they're what this
// implementation does and provides to applications.
//
// Application requests the DSP service to convert DSP addresses into ARM11 virtual addresses using
// the ConvertProcessAddressFromDspDram service call. Applications seem to derive the addresses for
// the second region via:
//     second_region_dsp_addr = first_region_dsp_addr | 0x10000
//
// Applications maintain most of its own audio state, the memory region is used mainly for
// communication and not storage of state.
//
// In the documentation below, filter and effect transfer functions are specified in the z domain.
// (If you are more familiar with the Laplace transform, z = exp(sT). The z domain is the digital
// frequency domain, just like how the s domain is the analog frequency domain.)

#define INSERT_PADDING_DSPWORDS(num_words) INSERT_PADDING_BYTES(2 * (num_words))

#define ASSERT_DSP_STRUCT(name, size)                                                              \
    static_assert(std::is_standard_layout<name>::value,                                            \
                  "DSP structure " #name " doesn't use standard layout");                          \
    static_assert(std::is_trivially_copyable<name>::value,                                         \
                  "DSP structure " #name " isn't trivially copyable");                             \
    static_assert(sizeof(name) == (size), "Unexpected struct size for DSP structure " #name)

struct SourceConfiguration {
    struct Configuration {
        /// These dirty flags are set by the application when it updates the fields in this struct.
        /// The DSP clears these each audio frame.
        union {
            u32_le dirty_raw;

            BitField<0, 1, u32> format_dirty;
            BitField<1, 1, u32> mono_or_stereo_dirty;
            BitField<2, 1, u32> adpcm_coefficients_dirty;
            /// Tends to be set when a looped buffer is queued.
            BitField<3, 1, u32> partial_embedded_buffer_dirty;
            BitField<4, 1, u32> partial_reset_flag;

            BitField<16, 1, u32> enable_dirty;
            BitField<17, 1, u32> interpolation_dirty;
            BitField<18, 1, u32> rate_multiplier_dirty;
            BitField<19, 1, u32> buffer_queue_dirty;
            BitField<20, 1, u32> loop_related_dirty;
            /// Tends to also be set when embedded buffer is updated.
            BitField<21, 1, u32> play_position_dirty;
            BitField<22, 1, u32> filters_enabled_dirty;
            BitField<23, 1, u32> simple_filter_dirty;
            BitField<24, 1, u32> biquad_filter_dirty;
            BitField<25, 1, u32> gain_0_dirty;
            BitField<26, 1, u32> gain_1_dirty;
            BitField<27, 1, u32> gain_2_dirty;
            BitField<28, 1, u32> sync_count_dirty;
            BitField<29, 1, u32> reset_flag;
            BitField<30, 1, u32> embedded_buffer_dirty;
        };

        // Gain control

        /**
         * Gain is between 0.0-1.0. This determines how much will this source appear on each of the
         * 12 channels that feed into the intermediate mixers. Each of the three intermediate mixers
         * is fed two left and two right channels.
         */
        float_le gain[3][4];

        // Interpolation

        /// Multiplier for sample rate. Resampling occurs with the selected interpolation method.
        float_le rate_multiplier;

        enum class InterpolationMode : u8 {
            Polyphase = 0,
            Linear = 1,
            None = 2,
        };

        InterpolationMode interpolation_mode;
        INSERT_PADDING_BYTES(1); ///< Interpolation related

        // Filters

        /**
         * This is the simplest normalized first-order digital recursive filter.
         * The transfer function of this filter is:
         *     H(z) = b0 / (1 - a1 z^-1)
         * Note the feedbackward coefficient is negated.
         * Values are signed fixed point with 15 fractional bits.
         */
        struct SimpleFilter {
            s16_le b0;
            s16_le a1;
        };

        /**
         * This is a normalised biquad filter (second-order).
         * The transfer function of this filter is:
         *     H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 - a1 z^-1 - a2 z^-2)
         * Nintendo chose to negate the feedbackward coefficients. This differs from standard
         * notation as in: https://ccrma.stanford.edu/~jos/filters/Direct_Form_I.html
         * Values are signed fixed point with 14 fractional bits.
         */
        struct BiquadFilter {
            s16_le a2;
            s16_le a1;
            s16_le b2;
            s16_le b1;
            s16_le b0;
        };

        union {
            u16_le filters_enabled;
            BitField<0, 1, u16> simple_filter_enabled;
            BitField<1, 1, u16> biquad_filter_enabled;
        };

        SimpleFilter simple_filter;
        BiquadFilter biquad_filter;

        // Buffer Queue

        /// A buffer of audio data from the application, along with metadata about it.
        struct Buffer {
            /// Physical memory address of the start of the buffer
            u32_dsp physical_address;

            /// This is length in terms of samples.
            /// Note that in different buffer formats a sample takes up different number of bytes.
            u32_dsp length;

            /// ADPCM Predictor (4 bits) and Scale (4 bits)
            union {
                u16_le adpcm_ps;
                BitField<0, 4, u16> adpcm_scale;
                BitField<4, 4, u16> adpcm_predictor;
            };

            /// ADPCM Historical Samples (y[n-1] and y[n-2])
            u16_le adpcm_yn[2];

            /// This is non-zero when the ADPCM values above are to be updated.
            u8 adpcm_dirty;

            /// Is a looping buffer.
            u8 is_looping;

            /// This value is shown in SourceStatus::previous_buffer_id when this buffer has
            /// finished. This allows the emulated application to tell what buffer is currently
            /// playing.
            u16_le buffer_id;

            INSERT_PADDING_DSPWORDS(1);
        };

        u16_le buffers_dirty; ///< Bitmap indicating which buffers are dirty (bit i -> buffers[i])
        Buffer buffers[4];    ///< Queued Buffers

        // Playback controls

        u32_dsp loop_related;
        u8 enable;
        INSERT_PADDING_BYTES(1);
        u16_le sync_count;     ///< Application-side sync count (See also: SourceStatus::sync_count)
        u32_dsp play_position; ///< Position. (Units: number of samples)
        INSERT_PADDING_DSPWORDS(2);

        // Embedded Buffer
        // This buffer is often the first buffer to be used when initiating audio playback,
        // after which the buffer queue is used.

        u32_dsp physical_address;

        /// This is length in terms of samples.
        /// Note a sample takes up different number of bytes in different buffer formats.
        u32_dsp length;

        enum class MonoOrStereo : u16_le {
            Mono = 1,
            Stereo = 2,
        };

        enum class Format : u16_le {
            PCM8 = 0,
            PCM16 = 1,
            ADPCM = 2,
        };

        union {
            u16_le flags1_raw;
            BitField<0, 2, MonoOrStereo> mono_or_stereo;
            BitField<2, 2, Format> format;
            BitField<5, 1, u16> fade_in;
        };

        /// ADPCM Predictor (4 bit) and Scale (4 bit)
        union {
            u16_le adpcm_ps;
            BitField<0, 4, u16> adpcm_scale;
            BitField<4, 4, u16> adpcm_predictor;
        };

        /// ADPCM Historical Samples (y[n-1] and y[n-2])
        u16_le adpcm_yn[2];

        union {
            u16_le flags2_raw;
            BitField<0, 1, u16> adpcm_dirty; ///< Has the ADPCM info above been changed?
            BitField<1, 1, u16> is_looping;  ///< Is this a looping buffer?
        };

        /// Buffer id of embedded buffer (used as a buffer id in SourceStatus to reference this
        /// buffer).
        u16_le buffer_id;
    };

    Configuration config[num_sources];
};
ASSERT_DSP_STRUCT(SourceConfiguration::Configuration, 192);
ASSERT_DSP_STRUCT(SourceConfiguration::Configuration::Buffer, 20);

struct SourceStatus {
    struct Status {
        u8 is_enabled; ///< Is this channel enabled? (Doesn't have to be playing anything.)
        u8 current_buffer_id_dirty; ///< Non-zero when current_buffer_id changes
        u16_le sync_count; ///< Is set by the DSP to the value of SourceConfiguration::sync_count
        u32_dsp buffer_position;  ///< Number of samples into the current buffer
        u16_le current_buffer_id; ///< Updated when a buffer finishes playing
        u16_le last_buffer_id;    ///< Updated when all buffers in the queue finish playing
    };

    Status status[num_sources];
};
ASSERT_DSP_STRUCT(SourceStatus::Status, 12);

struct DspConfiguration {
    /// These dirty flags are set by the application when it updates the fields in this struct.
    /// The DSP clears these each audio frame.
    union {
        u32_le dirty_raw;

        BitField<6, 1, u32> aux_front_bypass_0_dirty;
        BitField<7, 1, u32> aux_front_bypass_1_dirty;
        BitField<8, 1, u32> aux_bus_enable_0_dirty;
        BitField<9, 1, u32> aux_bus_enable_1_dirty;
        BitField<10, 1, u32> delay_effect_0_dirty;
        BitField<11, 1, u32> delay_effect_1_dirty;
        BitField<12, 1, u32> reverb_effect_0_dirty;
        BitField<13, 1, u32> reverb_effect_1_dirty;

        BitField<15, 1, u32> output_buffer_count_dirty;
        BitField<16, 1, u32> master_volume_dirty;

        BitField<24, 1, u32> aux_return_volume_0_dirty;
        BitField<25, 1, u32> aux_return_volume_1_dirty;
        BitField<26, 1, u32> output_format_dirty;
        BitField<27, 1, u32> clipping_mode_dirty;
        BitField<28, 1, u32> headphones_connected_dirty;
        BitField<29, 1, u32> surround_depth_dirty;
        BitField<30, 1, u32> surround_speaker_position_dirty;
        BitField<31, 1, u32> rear_ratio_dirty;
    };

    /// The DSP has three intermediate audio mixers. This controls the volume level (0.0-1.0) for
    /// each at the final mixer.
    float_le master_volume;
    std::array<float_le, 2> aux_return_volume;

    u16_le output_buffer_count;
    INSERT_PADDING_DSPWORDS(2);

    enum class OutputFormat : u16_le {
        Mono = 0,
        Stereo = 1,
        Surround = 2,
    };

    OutputFormat output_format;

    u16_le clipping_mode;        ///< Not sure of the exact gain equation for the limiter.
    u16_le headphones_connected; ///< Application updates the DSP on headphone status.

    u16_le surround_depth;
    u16_le surround_speaker_position;
    INSERT_PADDING_DSPWORDS(1); ///< TODO: Surround sound related
    u16_le rear_ratio;
    std::array<u16_le, 2> aux_front_bypass;
    std::array<u16_le, 2> aux_bus_enable;

    /**
     * This is delay with feedback.
     * Transfer function:
     *     H(z) = a z^-N / (1 - b z^-1 + a g z^-N)
     *   where
     *     N = frame_count * samples_per_frame
     * g, a and b are fixed point with 7 fractional bits
     */
    struct DelayEffect {
        /// These dirty flags are set by the application when it updates the fields in this struct.
        /// The DSP clears these each audio frame.
        union {
            u16_le dirty_raw;
            BitField<0, 1, u16> enable_dirty;
            BitField<1, 1, u16> work_buffer_address_dirty;
            BitField<2, 1, u16> other_dirty; ///< Set when anything else has been changed
        };

        u16_le enable;
        INSERT_PADDING_DSPWORDS(1);
        u16_le outputs;
        /// The application allocates a block of memory for the DSP to use as a work buffer.
        u32_dsp work_buffer_address;
        /// Frames to delay by
        u16_le frame_count;

        // Coefficients
        s16_le g; ///< Fixed point with 7 fractional bits
        s16_le a; ///< Fixed point with 7 fractional bits
        s16_le b; ///< Fixed point with 7 fractional bits
    };

    DelayEffect delay_effect[2];

    struct ReverbEffect {
        INSERT_PADDING_DSPWORDS(26); ///< TODO
    };

    ReverbEffect reverb_effect[2];

    u16_le sync_mode;
    INSERT_PADDING_DSPWORDS(1);
    union {
        u32_le dirty2_raw;

        BitField<16, 1, u32> sync_mode_dirty;
    };
};
ASSERT_DSP_STRUCT(DspConfiguration, 196);
ASSERT_DSP_STRUCT(DspConfiguration::DelayEffect, 20);
ASSERT_DSP_STRUCT(DspConfiguration::ReverbEffect, 52);
static_assert(offsetof(DspConfiguration, sync_mode) == 0xBC);
static_assert(offsetof(DspConfiguration, dirty2_raw) == 0xC0);

struct AdpcmCoefficients {
    /// Coefficients are signed fixed point with 11 fractional bits.
    /// Each source has 16 coefficients associated with it.
    s16_le coeff[num_sources][16];
};
ASSERT_DSP_STRUCT(AdpcmCoefficients, 768);

struct DspStatus {
    u16_le unknown;
    u16_le dropped_frames;
    INSERT_PADDING_DSPWORDS(0xE);
};
ASSERT_DSP_STRUCT(DspStatus, 32);

/// Final mixed output in PCM16 stereo format, what you hear out of the speakers.
/// When the application writes to this region it has no effect.
struct FinalMixSamples {
    s16_le pcm16[samples_per_frame][2];
};
ASSERT_DSP_STRUCT(FinalMixSamples, 640);

/// DSP writes output of intermediate mixers 1 and 2 here.
/// Writes to this region by the application edits the output of the intermediate mixers.
/// This seems to be intended to allow the application to do custom effects on the ARM11.
/// Values that exceed s16 range will be clipped by the DSP after further processing.
struct IntermediateMixSamples {
    struct Samples {
        s32_le pcm32[4][samples_per_frame]; ///< Little-endian as opposed to DSP middle-endian.
    };

    Samples mix1;
    Samples mix2;
};
ASSERT_DSP_STRUCT(IntermediateMixSamples, 5120);

/// Compressor table
struct Compressor {
    INSERT_PADDING_DSPWORDS(0xD20); ///< TODO
};

/// There is no easy way to implement this in a HLE implementation.
struct DspDebug {
    INSERT_PADDING_DSPWORDS(0x130);
};
ASSERT_DSP_STRUCT(DspDebug, 0x260);

struct SharedMemory {
    /// Padding
    INSERT_PADDING_DSPWORDS(0x400);

    DspStatus dsp_status;

    DspDebug dsp_debug;

    FinalMixSamples final_samples;

    SourceStatus source_statuses;

    Compressor compressor;

    DspConfiguration dsp_configuration;

    IntermediateMixSamples intermediate_mix_samples;

    SourceConfiguration source_configurations;

    AdpcmCoefficients adpcm_coefficients;

    struct {
        INSERT_PADDING_DSPWORDS(0x100);
    } unknown10;

    struct {
        INSERT_PADDING_DSPWORDS(0xC0);
    } unknown11;

    struct {
        INSERT_PADDING_DSPWORDS(0x180);
    } unknown12;

    struct {
        INSERT_PADDING_DSPWORDS(0xA);
    } unknown13;

    struct {
        INSERT_PADDING_DSPWORDS(0x13A3);
    } unknown14;

    u16_le frame_counter;
};
ASSERT_DSP_STRUCT(SharedMemory, 0x8000);

union DspMemory {
    std::array<u8, 0x80000> raw_memory{};
    struct {
        u8 unused_0[0x50000];
        SharedMemory region_0;
        u8 unused_1[0x18000];
        SharedMemory region_1;
        u8 unused_2[0x8000];
    };
};
static_assert(offsetof(DspMemory, region_0) == region0_offset,
              "DSP region 0 is at the wrong offset");
static_assert(offsetof(DspMemory, region_1) == region1_offset,
              "DSP region 1 is at the wrong offset");

// Structures must have an offset that is a multiple of two.
static_assert(offsetof(SharedMemory, frame_counter) % 2 == 0,
              "Structures in HLE::SharedMemory must be 2-byte aligned");
static_assert(offsetof(SharedMemory, source_configurations) % 2 == 0,
              "Structures in HLE::SharedMemory must be 2-byte aligned");
static_assert(offsetof(SharedMemory, source_statuses) % 2 == 0,
              "Structures in HLE::SharedMemory must be 2-byte aligned");
static_assert(offsetof(SharedMemory, adpcm_coefficients) % 2 == 0,
              "Structures in HLE::SharedMemory must be 2-byte aligned");
static_assert(offsetof(SharedMemory, dsp_configuration) % 2 == 0,
              "Structures in HLE::SharedMemory must be 2-byte aligned");
static_assert(offsetof(SharedMemory, dsp_status) % 2 == 0,
              "Structures in HLE::SharedMemory must be 2-byte aligned");
static_assert(offsetof(SharedMemory, final_samples) % 2 == 0,
              "Structures in HLE::SharedMemory must be 2-byte aligned");
static_assert(offsetof(SharedMemory, intermediate_mix_samples) % 2 == 0,
              "Structures in HLE::SharedMemory must be 2-byte aligned");
static_assert(offsetof(SharedMemory, compressor) % 2 == 0,
              "Structures in HLE::SharedMemory must be 2-byte aligned");
static_assert(offsetof(SharedMemory, dsp_debug) % 2 == 0,
              "Structures in HLE::SharedMemory must be 2-byte aligned");
static_assert(offsetof(SharedMemory, unknown10) % 2 == 0,
              "Structures in HLE::SharedMemory must be 2-byte aligned");
static_assert(offsetof(SharedMemory, unknown11) % 2 == 0,
              "Structures in HLE::SharedMemory must be 2-byte aligned");
static_assert(offsetof(SharedMemory, unknown12) % 2 == 0,
              "Structures in HLE::SharedMemory must be 2-byte aligned");
static_assert(offsetof(SharedMemory, unknown13) % 2 == 0,
              "Structures in HLE::SharedMemory must be 2-byte aligned");
static_assert(offsetof(SharedMemory, unknown14) % 2 == 0,
              "Structures in HLE::SharedMemory must be 2-byte aligned");

#undef INSERT_PADDING_DSPWORDS
#undef ASSERT_DSP_STRUCT

} // namespace AudioCore::HLE
