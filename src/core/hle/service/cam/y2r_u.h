// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>
#include <boost/serialization/array.hpp>
#include "common/common_types.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class Event;
}

namespace Service::Y2R {

enum class InputFormat : u8 {
    /// 8-bit input, with YUV components in separate planes and 4:2:2 subsampling.
    YUV422_Indiv8 = 0,
    /// 8-bit input, with YUV components in separate planes and 4:2:0 subsampling.
    YUV420_Indiv8 = 1,

    /// 16-bit input (only LSB used), with YUV components in separate planes and 4:2:2 subsampling.
    YUV422_Indiv16 = 2,
    /// 16-bit input (only LSB used), with YUV components in separate planes and 4:2:0 subsampling.
    YUV420_Indiv16 = 3,

    /// 8-bit input, with a single interleaved stream in YUYV format and 4:2:2 subsampling.
    YUYV422_Interleaved = 4,
};

enum class OutputFormat : u8 {
    RGBA8 = 0,
    RGB8 = 1,
    RGB5A1 = 2,
    RGB565 = 3,
};

enum class Rotation : u8 {
    None = 0,
    Clockwise_90 = 1,
    Clockwise_180 = 2,
    Clockwise_270 = 3,
};

enum class BlockAlignment : u8 {
    /// Image is output in linear format suitable for use as a framebuffer.
    Linear = 0,
    /// Image is output in tiled PICA format, suitable for use as a texture.
    Block8x8 = 1,
};

enum class StandardCoefficient : u8 {
    /// ITU Rec. BT.601 primaries, with PC ranges.
    ITU_Rec601 = 0,
    /// ITU Rec. BT.709 primaries, with PC ranges.
    ITU_Rec709 = 1,
    /// ITU Rec. BT.601 primaries, with TV ranges.
    ITU_Rec601_Scaling = 2,
    /// ITU Rec. BT.709 primaries, with TV ranges.
    ITU_Rec709_Scaling = 3,
};

/**
 * A set of coefficients configuring the RGB to YUV conversion. Coefficients 0-4 are unsigned 2.8
 * fixed pointer numbers representing entries on the conversion matrix, while coefficient 5-7 are
 * signed 11.5 fixed point numbers added as offsets to the RGB result.
 *
 * The overall conversion process formula is:
 * ```
 * R = trunc((c_0 * Y           + c_1 * V) + c_5 + 0.75)
 * G = trunc((c_0 * Y - c_3 * U - c_2 * V) + c_6 + 0.75)
 * B = trunc((c_0 * Y + c_4 * U          ) + c_7 + 0.75)
 * ```
 */
using CoefficientSet = std::array<s16, 8>;

struct ConversionBuffer {
    /// Current reading/writing address of this buffer.
    VAddr address;
    /// Remaining amount of bytes to be DMAed, does not include the inter-trasfer gap.
    u32 image_size;
    /// Size of a single DMA transfer.
    u16 transfer_unit;
    /// Amount of bytes to be skipped between copying each `transfer_unit` bytes.
    u16 gap;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & address;
        ar & image_size;
        ar & transfer_unit;
        ar & gap;
    }
    friend class boost::serialization::access;
};

struct ConversionConfiguration {
    InputFormat input_format;
    OutputFormat output_format;
    Rotation rotation;
    BlockAlignment block_alignment;
    u16 input_line_width;
    u16 input_lines;
    CoefficientSet coefficients;
    u8 padding;
    u16 alpha;

    /// Input parameters for the Y (luma) plane
    ConversionBuffer src_Y, src_U, src_V, src_YUYV;
    /// Output parameters for the conversion results
    ConversionBuffer dst;

    Result SetInputLineWidth(u16 width);
    Result SetInputLines(u16 lines);
    Result SetStandardCoefficient(StandardCoefficient standard_coefficient);

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & input_format;
        ar & output_format;
        ar & rotation;
        ar & block_alignment;
        ar & input_line_width;
        ar & input_lines;
        ar & coefficients;
        ar & padding;
        ar & alpha;
        ar & src_Y;
        ar & src_U;
        ar & src_V;
        ar & src_YUYV;
        ar & dst;
    }
    friend class boost::serialization::access;
};

struct DitheringWeightParams {
    u16 w0_xEven_yEven;
    u16 w0_xOdd_yEven;
    u16 w0_xEven_yOdd;
    u16 w0_xOdd_yOdd;
    u16 w1_xEven_yEven;
    u16 w1_xOdd_yEven;
    u16 w1_xEven_yOdd;
    u16 w1_xOdd_yOdd;
    u16 w2_xEven_yEven;
    u16 w2_xOdd_yEven;
    u16 w2_xEven_yOdd;
    u16 w2_xOdd_yOdd;
    u16 w3_xEven_yEven;
    u16 w3_xOdd_yEven;
    u16 w3_xEven_yOdd;
    u16 w3_xOdd_yOdd;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & w0_xEven_yEven;
        ar & w0_xOdd_yEven;
        ar & w0_xEven_yOdd;
        ar & w0_xOdd_yOdd;
        ar & w1_xEven_yEven;
        ar & w1_xOdd_yEven;
        ar & w1_xEven_yOdd;
        ar & w1_xOdd_yOdd;
        ar & w2_xEven_yEven;
        ar & w2_xOdd_yEven;
        ar & w2_xEven_yOdd;
        ar & w2_xOdd_yOdd;
        ar & w3_xEven_yEven;
        ar & w3_xOdd_yEven;
        ar & w3_xEven_yOdd;
        ar & w3_xOdd_yOdd;
    }
    friend class boost::serialization::access;
};

struct ConversionParameters {
    InputFormat input_format;
    OutputFormat output_format;
    Rotation rotation;
    BlockAlignment block_alignment;
    u16 input_line_width;
    u16 input_lines;
    StandardCoefficient standard_coefficient;
    u8 padding;
    u16 alpha;
};
static_assert(sizeof(ConversionParameters) == 12, "ConversionParameters struct has incorrect size");

class Y2R_U final : public ServiceFramework<Y2R_U> {
public:
    explicit Y2R_U(Core::System& system);
    ~Y2R_U() override;

private:
    void SetInputFormat(Kernel::HLERequestContext& ctx);
    void GetInputFormat(Kernel::HLERequestContext& ctx);
    void SetOutputFormat(Kernel::HLERequestContext& ctx);
    void GetOutputFormat(Kernel::HLERequestContext& ctx);
    void SetRotation(Kernel::HLERequestContext& ctx);
    void GetRotation(Kernel::HLERequestContext& ctx);
    void SetBlockAlignment(Kernel::HLERequestContext& ctx);
    void GetBlockAlignment(Kernel::HLERequestContext& ctx);

    /**
     * Y2R_U::SetSpacialDithering service function
     *  Inputs:
     *      1 : u8, 0 = Disabled, 1 = Enabled
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetSpacialDithering(Kernel::HLERequestContext& ctx);

    /**
     * Y2R_U::GetSpacialDithering service function
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : u8, 0 = Disabled, 1 = Enabled
     */
    void GetSpacialDithering(Kernel::HLERequestContext& ctx);

    /**
     * Y2R_U::SetTemporalDithering service function
     *  Inputs:
     *      1 : u8, 0 = Disabled, 1 = Enabled
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetTemporalDithering(Kernel::HLERequestContext& ctx);

    /**
     * Y2R_U::GetTemporalDithering service function
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : u8, 0 = Disabled, 1 = Enabled
     */
    void GetTemporalDithering(Kernel::HLERequestContext& ctx);

    /**
     * Y2R_U::SetTransferEndInterrupt service function
     *  Inputs:
     *      1 : u8, 0 = Disabled, 1 = Enabled
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetTransferEndInterrupt(Kernel::HLERequestContext& ctx);

    /**
     * Y2R_U::GetTransferEndInterrupt service function
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : u8, 0 = Disabled, 1 = Enabled
     */
    void GetTransferEndInterrupt(Kernel::HLERequestContext& ctx);

    /**
     * Y2R_U::GetTransferEndEvent service function
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      3 : The handle of the completion event
     */
    void GetTransferEndEvent(Kernel::HLERequestContext& ctx);

    void SetSendingY(Kernel::HLERequestContext& ctx);
    void SetSendingU(Kernel::HLERequestContext& ctx);
    void SetSendingV(Kernel::HLERequestContext& ctx);
    void SetSendingYUYV(Kernel::HLERequestContext& ctx);

    /**
     * Y2R::IsFinishedSendingYuv service function
     * Output:
     *       1 : Result of the function, 0 on success, otherwise error code
     *       2 : u8, 0 = Not Finished, 1 = Finished
     */
    void IsFinishedSendingYuv(Kernel::HLERequestContext& ctx);

    /**
     * Y2R::IsFinishedSendingY service function
     * Output:
     *       1 : Result of the function, 0 on success, otherwise error code
     *       2 : u8, 0 = Not Finished, 1 = Finished
     */
    void IsFinishedSendingY(Kernel::HLERequestContext& ctx);

    /**
     * Y2R::IsFinishedSendingU service function
     * Output:
     *       1 : Result of the function, 0 on success, otherwise error code
     *       2 : u8, 0 = Not Finished, 1 = Finished
     */
    void IsFinishedSendingU(Kernel::HLERequestContext& ctx);

    /**
     * Y2R::IsFinishedSendingV service function
     * Output:
     *       1 : Result of the function, 0 on success, otherwise error code
     *       2 : u8, 0 = Not Finished, 1 = Finished
     */
    void IsFinishedSendingV(Kernel::HLERequestContext& ctx);

    void SetReceiving(Kernel::HLERequestContext& ctx);

    /**
     * Y2R::IsFinishedReceiving service function
     * Output:
     *       1 : Result of the function, 0 on success, otherwise error code
     *       2 : u8, 0 = Not Finished, 1 = Finished
     */
    void IsFinishedReceiving(Kernel::HLERequestContext& ctx);

    void SetInputLineWidth(Kernel::HLERequestContext& ctx);
    void GetInputLineWidth(Kernel::HLERequestContext& ctx);
    void SetInputLines(Kernel::HLERequestContext& ctx);
    void GetInputLines(Kernel::HLERequestContext& ctx);
    void SetCoefficient(Kernel::HLERequestContext& ctx);
    void GetCoefficient(Kernel::HLERequestContext& ctx);
    void SetStandardCoefficient(Kernel::HLERequestContext& ctx);
    void GetStandardCoefficient(Kernel::HLERequestContext& ctx);
    void SetAlpha(Kernel::HLERequestContext& ctx);
    void GetAlpha(Kernel::HLERequestContext& ctx);
    void SetDitheringWeightParams(Kernel::HLERequestContext& ctx);
    void GetDitheringWeightParams(Kernel::HLERequestContext& ctx);
    void StartConversion(Kernel::HLERequestContext& ctx);
    void StopConversion(Kernel::HLERequestContext& ctx);
    void IsBusyConversion(Kernel::HLERequestContext& ctx);

    /**
     * Y2R_U::SetPackageParameter service function
     */
    void SetPackageParameter(Kernel::HLERequestContext& ctx);

    void PingProcess(Kernel::HLERequestContext& ctx);
    void DriverInitialize(Kernel::HLERequestContext& ctx);
    void DriverFinalize(Kernel::HLERequestContext& ctx);
    void GetPackageParameter(Kernel::HLERequestContext& ctx);

    Core::System& system;

    std::shared_ptr<Kernel::Event> completion_event;
    ConversionConfiguration conversion{};
    DitheringWeightParams dithering_weight_params{};
    bool temporal_dithering_enabled = false;
    bool transfer_end_interrupt_enabled = false;
    bool spacial_dithering_enabled = false;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::Y2R

SERVICE_CONSTRUCT(Service::Y2R::Y2R_U)
BOOST_CLASS_EXPORT_KEY(Service::Y2R::Y2R_U)
