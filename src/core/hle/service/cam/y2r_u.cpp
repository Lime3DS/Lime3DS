// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstring>
#include "common/archives.h"
#include "common/common_funcs.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/cam/y2r_u.h"
#include "core/hw/y2r.h"

SERVICE_CONSTRUCT_IMPL(Service::Y2R::Y2R_U)
SERIALIZE_EXPORT_IMPL(Service::Y2R::Y2R_U)

namespace Service::Y2R {

template <class Archive>
void Y2R_U::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
    ar & completion_event;
    ar & conversion;
    ar & dithering_weight_params;
    ar & temporal_dithering_enabled;
    ar & transfer_end_interrupt_enabled;
    ar & spacial_dithering_enabled;
}

constexpr std::array<CoefficientSet, 4> standard_coefficients{{
    {{0x100, 0x166, 0xB6, 0x58, 0x1C5, -0x166F, 0x10EE, -0x1C5B}}, // ITU_Rec601
    {{0x100, 0x193, 0x77, 0x2F, 0x1DB, -0x1933, 0xA7C, -0x1D51}},  // ITU_Rec709
    {{0x12A, 0x198, 0xD0, 0x64, 0x204, -0x1BDE, 0x10F2, -0x229B}}, // ITU_Rec601_Scaling
    {{0x12A, 0x1CA, 0x88, 0x36, 0x21C, -0x1F04, 0x99C, -0x2421}},  // ITU_Rec709_Scaling
}};

Result ConversionConfiguration::SetInputLineWidth(u16 width) {
    if (width == 0 || width > 1024 || width % 8 != 0) {
        return Result(ErrorDescription::OutOfRange, ErrorModule::CAM, ErrorSummary::InvalidArgument,
                      ErrorLevel::Usage); // 0xE0E053FD
    }

    // Note: The hardware uses the register value 0 to represent a width of 1024, so for a width of
    // 1024 the `camera` module would set the value 0 here, but we don't need to emulate this
    // internal detail.
    this->input_line_width = width;
    return ResultSuccess;
}

Result ConversionConfiguration::SetInputLines(u16 lines) {
    if (lines == 0 || lines > 1024) {
        return Result(ErrorDescription::OutOfRange, ErrorModule::CAM, ErrorSummary::InvalidArgument,
                      ErrorLevel::Usage); // 0xE0E053FD
    }

    // Note: In what appears to be a bug, the `camera` module does not set the hardware register at
    // all if `lines` is 1024, so the conversion uses the last value that was set. The intention
    // was probably to set it to 0 like in SetInputLineWidth.
    if (lines != 1024) {
        this->input_lines = lines;
    }
    return ResultSuccess;
}

Result ConversionConfiguration::SetStandardCoefficient(StandardCoefficient standard_coefficient) {
    const auto index = static_cast<std::size_t>(standard_coefficient);
    if (index >= standard_coefficients.size()) {
        return Result(ErrorDescription::InvalidEnumValue, ErrorModule::CAM,
                      ErrorSummary::InvalidArgument, ErrorLevel::Usage); // 0xE0E053ED
    }

    std::memcpy(coefficients.data(), standard_coefficients[index].data(), sizeof(coefficients));
    return ResultSuccess;
}

void Y2R_U::SetInputFormat(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    conversion.input_format = rp.PopEnum<InputFormat>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R, "called input_format={}", conversion.input_format);
}

void Y2R_U::GetInputFormat(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.PushEnum(conversion.input_format);

    LOG_DEBUG(Service_Y2R, "called input_format={}", conversion.input_format);
}

void Y2R_U::SetOutputFormat(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    conversion.output_format = rp.PopEnum<OutputFormat>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R, "called output_format={}", conversion.output_format);
}

void Y2R_U::GetOutputFormat(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.PushEnum(conversion.output_format);

    LOG_DEBUG(Service_Y2R, "called output_format={}", conversion.output_format);
}

void Y2R_U::SetRotation(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    conversion.rotation = rp.PopEnum<Rotation>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R, "called rotation={}", conversion.rotation);
}

void Y2R_U::GetRotation(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.PushEnum(conversion.rotation);

    LOG_DEBUG(Service_Y2R, "called rotation={}", conversion.rotation);
}

void Y2R_U::SetBlockAlignment(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    conversion.block_alignment = rp.PopEnum<BlockAlignment>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R, "called block_alignment={}", conversion.block_alignment);
}

void Y2R_U::GetBlockAlignment(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.PushEnum(conversion.block_alignment);

    LOG_DEBUG(Service_Y2R, "called block_alignment={}", conversion.block_alignment);
}

void Y2R_U::SetSpacialDithering(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    spacial_dithering_enabled = rp.Pop<bool>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R, "(STUBBED) called");
}

void Y2R_U::GetSpacialDithering(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(spacial_dithering_enabled);

    LOG_DEBUG(Service_Y2R, "(STUBBED) called");
}

void Y2R_U::SetTemporalDithering(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    temporal_dithering_enabled = rp.Pop<bool>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R, "(STUBBED) called");
}

void Y2R_U::GetTemporalDithering(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(temporal_dithering_enabled);

    LOG_DEBUG(Service_Y2R, "(STUBBED) called");
}

void Y2R_U::SetTransferEndInterrupt(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    transfer_end_interrupt_enabled = rp.Pop<bool>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R, "(STUBBED) called");
}

void Y2R_U::GetTransferEndInterrupt(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(transfer_end_interrupt_enabled);

    LOG_DEBUG(Service_Y2R, "(STUBBED) called");
}

void Y2R_U::GetTransferEndEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(completion_event);

    LOG_DEBUG(Service_Y2R, "called");
}

void Y2R_U::SetSendingY(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    conversion.src_Y.address = rp.Pop<u32>();
    conversion.src_Y.image_size = rp.Pop<u32>();
    conversion.src_Y.transfer_unit = rp.Pop<u32>();
    conversion.src_Y.gap = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();
    // TODO (wwylele): pass process handle to y2r engine or convert VAddr to PAddr

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R,
              "called image_size=0x{:08X}, transfer_unit={}, transfer_stride={}, "
              "src_process_id={}",
              conversion.src_Y.image_size, conversion.src_Y.transfer_unit, conversion.src_Y.gap,
              process->process_id);
}

void Y2R_U::SetSendingU(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    conversion.src_U.address = rp.Pop<u32>();
    conversion.src_U.image_size = rp.Pop<u32>();
    conversion.src_U.transfer_unit = rp.Pop<u32>();
    conversion.src_U.gap = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();
    // TODO (wwylele): pass the process handle to y2r engine or convert VAddr to PAddr

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R,
              "called image_size=0x{:08X}, transfer_unit={}, transfer_stride={}, "
              "src_process_id={}",
              conversion.src_U.image_size, conversion.src_U.transfer_unit, conversion.src_U.gap,
              process->process_id);
}

void Y2R_U::SetSendingV(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    conversion.src_V.address = rp.Pop<u32>();
    conversion.src_V.image_size = rp.Pop<u32>();
    conversion.src_V.transfer_unit = rp.Pop<u32>();
    conversion.src_V.gap = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();
    // TODO (wwylele): pass the process handle to y2r engine or convert VAddr to PAddr

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R,
              "called image_size=0x{:08X}, transfer_unit={}, transfer_stride={}, "
              "src_process_id={}",
              conversion.src_V.image_size, conversion.src_V.transfer_unit, conversion.src_V.gap,
              process->process_id);
}

void Y2R_U::SetSendingYUYV(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    conversion.src_YUYV.address = rp.Pop<u32>();
    conversion.src_YUYV.image_size = rp.Pop<u32>();
    conversion.src_YUYV.transfer_unit = rp.Pop<u32>();
    conversion.src_YUYV.gap = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();
    // TODO (wwylele): pass the process handle to y2r engine or convert VAddr to PAddr

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R,
              "called image_size=0x{:08X}, transfer_unit={}, transfer_stride={}, "
              "src_process_id={}",
              conversion.src_YUYV.image_size, conversion.src_YUYV.transfer_unit,
              conversion.src_YUYV.gap, process->process_id);
}

void Y2R_U::IsFinishedSendingYuv(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push<u8>(1);

    LOG_DEBUG(Service_Y2R, "(STUBBED) called");
}

void Y2R_U::IsFinishedSendingY(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push<u8>(1);

    LOG_DEBUG(Service_Y2R, "(STUBBED) called");
}

void Y2R_U::IsFinishedSendingU(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push<u8>(1);

    LOG_DEBUG(Service_Y2R, "(STUBBED) called");
}

void Y2R_U::IsFinishedSendingV(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push<u8>(1);

    LOG_DEBUG(Service_Y2R, "(STUBBED) called");
}

void Y2R_U::SetReceiving(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    conversion.dst.address = rp.Pop<u32>();
    conversion.dst.image_size = rp.Pop<u32>();
    conversion.dst.transfer_unit = rp.Pop<u32>();
    conversion.dst.gap = rp.Pop<u32>();
    auto dst_process = rp.PopObject<Kernel::Process>();
    // TODO (wwylele): pass the process handle to y2r engine or convert VAddr to PAddr

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R,
              "called image_size=0x{:08X}, transfer_unit={}, transfer_stride={}, "
              "dst_process_id={}",
              conversion.dst.image_size, conversion.dst.transfer_unit, conversion.dst.gap,
              static_cast<u32>(dst_process->process_id));
}

void Y2R_U::IsFinishedReceiving(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push<u8>(1);

    LOG_DEBUG(Service_Y2R, "(STUBBED) called");
}

void Y2R_U::SetInputLineWidth(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 input_line_width = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(conversion.SetInputLineWidth(input_line_width));

    LOG_DEBUG(Service_Y2R, "called input_line_width={}", input_line_width);
}

void Y2R_U::GetInputLineWidth(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(conversion.input_line_width);

    LOG_DEBUG(Service_Y2R, "called input_line_width={}", conversion.input_line_width);
}

void Y2R_U::SetInputLines(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 input_lines = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(conversion.SetInputLines(input_lines));

    LOG_DEBUG(Service_Y2R, "called input_lines={}", input_lines);
}

void Y2R_U::GetInputLines(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(conversion.input_lines));

    LOG_DEBUG(Service_Y2R, "called input_lines={}", conversion.input_lines);
}

void Y2R_U::SetCoefficient(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    rp.PopRaw<CoefficientSet>(conversion.coefficients);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R, "called coefficients=[{:X}, {:X}, {:X}, {:X}, {:X}, {:X}, {:X}, {:X}]",
              conversion.coefficients[0], conversion.coefficients[1], conversion.coefficients[2],
              conversion.coefficients[3], conversion.coefficients[4], conversion.coefficients[5],
              conversion.coefficients[6], conversion.coefficients[7]);
}

void Y2R_U::GetCoefficient(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(ResultSuccess);
    rb.PushRaw(conversion.coefficients);

    LOG_DEBUG(Service_Y2R, "called");
}

void Y2R_U::SetStandardCoefficient(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 index = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(conversion.SetStandardCoefficient(static_cast<StandardCoefficient>(index)));

    LOG_DEBUG(Service_Y2R, "called standard_coefficient={}", index);
}

void Y2R_U::GetStandardCoefficient(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 index = rp.Pop<u32>();

    if (index < standard_coefficients.size()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
        rb.Push(ResultSuccess);
        rb.PushRaw(standard_coefficients[index]);

        LOG_DEBUG(Service_Y2R, "called standard_coefficient={} ", index);
    } else {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(Result(ErrorDescription::InvalidEnumValue, ErrorModule::CAM,
                       ErrorSummary::InvalidArgument, ErrorLevel::Usage));

        LOG_ERROR(Service_Y2R, "called standard_coefficient={}  The argument is invalid!", index);
    }
}

void Y2R_U::SetAlpha(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    conversion.alpha = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R, "called alpha={}", conversion.alpha);
}

void Y2R_U::GetAlpha(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(conversion.alpha);

    LOG_DEBUG(Service_Y2R, "called alpha={}", conversion.alpha);
}

void Y2R_U::SetDitheringWeightParams(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    rp.PopRaw(dithering_weight_params);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R, "called");
}

void Y2R_U::GetDitheringWeightParams(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(9, 0);
    rb.Push(ResultSuccess);
    rb.PushRaw(dithering_weight_params);

    LOG_DEBUG(Service_Y2R, "called");
}

void Y2R_U::StartConversion(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    // dst_image_size would seem to be perfect for this, but it doesn't include the gap :(
    u32 total_output_size =
        conversion.input_lines * (conversion.dst.transfer_unit + conversion.dst.gap);
    system.Memory().RasterizerFlushVirtualRegion(conversion.dst.address, total_output_size,
                                                 Memory::FlushMode::FlushAndInvalidate);

    HW::Y2R::PerformConversion(system.Memory(), conversion);

    completion_event->Signal();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R, "called");
}

void Y2R_U::StopConversion(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R, "called");
}

void Y2R_U::IsBusyConversion(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push<u8>(0); // StartConversion always finishes immediately

    LOG_DEBUG(Service_Y2R, "called");
}

void Y2R_U::SetPackageParameter(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    auto params = rp.PopRaw<ConversionParameters>();

    conversion.input_format = params.input_format;
    conversion.output_format = params.output_format;
    conversion.rotation = params.rotation;
    conversion.block_alignment = params.block_alignment;

    Result result = conversion.SetInputLineWidth(params.input_line_width);

    if (result.IsError())
        goto cleanup;

    result = conversion.SetInputLines(params.input_lines);

    if (result.IsError())
        goto cleanup;

    result = conversion.SetStandardCoefficient(params.standard_coefficient);

    if (result.IsError())
        goto cleanup;

    conversion.padding = params.padding;
    conversion.alpha = params.alpha;

cleanup:
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);

    LOG_DEBUG(Service_Y2R,
              "called input_format={} output_format={} rotation={} block_alignment={} "
              "input_line_width={} input_lines={} standard_coefficient={} reserved={} alpha={:X}",
              params.input_format, params.output_format, params.rotation, params.block_alignment,
              params.input_line_width, params.input_lines, params.standard_coefficient,
              params.padding, params.alpha);
}

void Y2R_U::PingProcess(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push<u8>(0);

    LOG_DEBUG(Service_Y2R, "(STUBBED) called");
}

void Y2R_U::DriverInitialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    conversion.input_format = InputFormat::YUV422_Indiv8;
    conversion.output_format = OutputFormat::RGBA8;
    conversion.rotation = Rotation::None;
    conversion.block_alignment = BlockAlignment::Linear;
    conversion.coefficients.fill(0);
    conversion.SetInputLineWidth(1024);
    conversion.SetInputLines(1024);
    conversion.alpha = 0;

    ConversionBuffer zero_buffer = {};
    conversion.src_Y = zero_buffer;
    conversion.src_U = zero_buffer;
    conversion.src_V = zero_buffer;
    conversion.dst = zero_buffer;

    completion_event->Clear();

    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R, "called");
}

void Y2R_U::DriverFinalize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_Y2R, "called");
}

void Y2R_U::GetPackageParameter(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(4, 0);
    rb.Push(ResultSuccess);
    rb.PushRaw(conversion);

    LOG_DEBUG(Service_Y2R, "called");
}

Y2R_U::Y2R_U(Core::System& system) : ServiceFramework("y2r:u", 1), system(system) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &Y2R_U::SetInputFormat, "SetInputFormat"},
        {0x0002, &Y2R_U::GetInputFormat, "GetInputFormat"},
        {0x0003, &Y2R_U::SetOutputFormat, "SetOutputFormat"},
        {0x0004, &Y2R_U::GetOutputFormat, "GetOutputFormat"},
        {0x0005, &Y2R_U::SetRotation, "SetRotation"},
        {0x0006, &Y2R_U::GetRotation, "GetRotation"},
        {0x0007, &Y2R_U::SetBlockAlignment, "SetBlockAlignment"},
        {0x0008, &Y2R_U::GetBlockAlignment, "GetBlockAlignment"},
        {0x0009, &Y2R_U::SetSpacialDithering, "SetSpacialDithering"},
        {0x000A, &Y2R_U::GetSpacialDithering, "GetSpacialDithering"},
        {0x000B, &Y2R_U::SetTemporalDithering, "SetTemporalDithering"},
        {0x000C, &Y2R_U::GetTemporalDithering, "GetTemporalDithering"},
        {0x000D, &Y2R_U::SetTransferEndInterrupt, "SetTransferEndInterrupt"},
        {0x000E, &Y2R_U::GetTransferEndInterrupt, "GetTransferEndInterrupt"},
        {0x000F, &Y2R_U::GetTransferEndEvent, "GetTransferEndEvent"},
        {0x0010, &Y2R_U::SetSendingY, "SetSendingY"},
        {0x0011, &Y2R_U::SetSendingU, "SetSendingU"},
        {0x0012, &Y2R_U::SetSendingV, "SetSendingV"},
        {0x0013, &Y2R_U::SetSendingYUYV, "SetSendingYUYV"},
        {0x0014, &Y2R_U::IsFinishedSendingYuv, "IsFinishedSendingYuv"},
        {0x0015, &Y2R_U::IsFinishedSendingY, "IsFinishedSendingY"},
        {0x0016, &Y2R_U::IsFinishedSendingU, "IsFinishedSendingU"},
        {0x0017, &Y2R_U::IsFinishedSendingV, "IsFinishedSendingV"},
        {0x0018, &Y2R_U::SetReceiving, "SetReceiving"},
        {0x0019, &Y2R_U::IsFinishedReceiving, "IsFinishedReceiving"},
        {0x001A, &Y2R_U::SetInputLineWidth, "SetInputLineWidth"},
        {0x001B, &Y2R_U::GetInputLineWidth, "GetInputLineWidth"},
        {0x001C, &Y2R_U::SetInputLines, "SetInputLines"},
        {0x001D, &Y2R_U::GetInputLines, "GetInputLines"},
        {0x001E, &Y2R_U::SetCoefficient, "SetCoefficient"},
        {0x001F, &Y2R_U::GetCoefficient, "GetCoefficient"},
        {0x0020, &Y2R_U::SetStandardCoefficient, "SetStandardCoefficient"},
        {0x0021, &Y2R_U::GetStandardCoefficient, "GetStandardCoefficient"},
        {0x0022, &Y2R_U::SetAlpha, "SetAlpha"},
        {0x0023, &Y2R_U::GetAlpha, "GetAlpha"},
        {0x0024, &Y2R_U::SetDitheringWeightParams, "SetDitheringWeightParams"},
        {0x0025, &Y2R_U::GetDitheringWeightParams, "GetDitheringWeightParams"},
        {0x0026, &Y2R_U::StartConversion, "StartConversion"},
        {0x0027, &Y2R_U::StopConversion, "StopConversion"},
        {0x0028, &Y2R_U::IsBusyConversion, "IsBusyConversion"},
        {0x0029, &Y2R_U::SetPackageParameter, "SetPackageParameter"},
        {0x002A, &Y2R_U::PingProcess, "PingProcess"},
        {0x002B, &Y2R_U::DriverInitialize, "DriverInitialize"},
        {0x002C, &Y2R_U::DriverFinalize, "DriverFinalize"},
        {0x002D, &Y2R_U::GetPackageParameter, "GetPackageParameter"},
        // clang-format on
    };
    RegisterHandlers(functions);

    completion_event = system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "Y2R:Completed");
}

Y2R_U::~Y2R_U() = default;

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<Y2R_U>(system)->InstallAsService(service_manager);
}

} // namespace Service::Y2R
