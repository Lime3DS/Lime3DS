// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/cam/cam_u.h"

namespace Service::CAM {

CAM_U::CAM_U(std::shared_ptr<Module> cam) : Module::Interface(std::move(cam), "cam:u", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &CAM_U::StartCapture, "StartCapture"},
        {0x0002, &CAM_U::StopCapture, "StopCapture"},
        {0x0003, &CAM_U::IsBusy, "IsBusy"},
        {0x0004, &CAM_U::ClearBuffer, "ClearBuffer"},
        {0x0005, &CAM_U::GetVsyncInterruptEvent, "GetVsyncInterruptEvent"},
        {0x0006, &CAM_U::GetBufferErrorInterruptEvent, "GetBufferErrorInterruptEvent"},
        {0x0007, &CAM_U::SetReceiving, "SetReceiving"},
        {0x0008, &CAM_U::IsFinishedReceiving, "IsFinishedReceiving"},
        {0x0009, &CAM_U::SetTransferLines, "SetTransferLines"},
        {0x000A, &CAM_U::GetMaxLines, "GetMaxLines"},
        {0x000B, &CAM_U::SetTransferBytes, "SetTransferBytes"},
        {0x000C, &CAM_U::GetTransferBytes, "GetTransferBytes"},
        {0x000D, &CAM_U::GetMaxBytes, "GetMaxBytes"},
        {0x000E, &CAM_U::SetTrimming, "SetTrimming"},
        {0x000F, &CAM_U::IsTrimming, "IsTrimming"},
        {0x0010, &CAM_U::SetTrimmingParams, "SetTrimmingParams"},
        {0x0011, &CAM_U::GetTrimmingParams, "GetTrimmingParams"},
        {0x0012, &CAM_U::SetTrimmingParamsCenter, "SetTrimmingParamsCenter"},
        {0x0013, &CAM_U::Activate, "Activate"},
        {0x0014, &CAM_U::SwitchContext, "SwitchContext"},
        {0x0015, nullptr, "SetExposure"},
        {0x0016, nullptr, "SetWhiteBalance"},
        {0x0017, nullptr, "SetWhiteBalanceWithoutBaseUp"},
        {0x0018, nullptr, "SetSharpness"},
        {0x0019, nullptr, "SetAutoExposure"},
        {0x001A, nullptr, "IsAutoExposure"},
        {0x001B, nullptr, "SetAutoWhiteBalance"},
        {0x001C, nullptr, "IsAutoWhiteBalance"},
        {0x001D, &CAM_U::FlipImage, "FlipImage"},
        {0x001E, &CAM_U::SetDetailSize, "SetDetailSize"},
        {0x001F, &CAM_U::SetSize, "SetSize"},
        {0x0020, &CAM_U::SetFrameRate, "SetFrameRate"},
        {0x0021, nullptr, "SetPhotoMode"},
        {0x0022, &CAM_U::SetEffect, "SetEffect"},
        {0x0023, nullptr, "SetContrast"},
        {0x0024, nullptr, "SetLensCorrection"},
        {0x0025, &CAM_U::SetOutputFormat, "SetOutputFormat"},
        {0x0026, nullptr, "SetAutoExposureWindow"},
        {0x0027, nullptr, "SetAutoWhiteBalanceWindow"},
        {0x0028, nullptr, "SetNoiseFilter"},
        {0x0029, &CAM_U::SynchronizeVsyncTiming, "SynchronizeVsyncTiming"},
        {0x002A, &CAM_U::GetLatestVsyncTiming, "GetLatestVsyncTiming"},
        {0x002B, &CAM_U::GetStereoCameraCalibrationData, "GetStereoCameraCalibrationData"},
        {0x002C, nullptr, "SetStereoCameraCalibrationData"},
        {0x002D, nullptr, "WriteRegisterI2c"},
        {0x002E, nullptr, "WriteMcuVariableI2c"},
        {0x002F, nullptr, "ReadRegisterI2cExclusive"},
        {0x0030, nullptr, "ReadMcuVariableI2cExclusive"},
        {0x0031, nullptr, "SetImageQualityCalibrationData"},
        {0x0032, nullptr, "GetImageQualityCalibrationData"},
        {0x0033, &CAM_U::SetPackageParameterWithoutContext, "SetPackageParameterWithoutContext"},
        {0x0034, &CAM_U::SetPackageParameterWithContext, "SetPackageParameterWithContext"},
        {0x0035, &CAM_U::SetPackageParameterWithContextDetail, "SetPackageParameterWithContextDetail"},
        {0x0036, &CAM_U::GetSuitableY2rStandardCoefficient, "GetSuitableY2rStandardCoefficient"},
        {0x0037, nullptr, "PlayShutterSoundWithWave"},
        {0x0038, &CAM_U::PlayShutterSound, "PlayShutterSound"},
        {0x0039, &CAM_U::DriverInitialize, "DriverInitialize"},
        {0x003A, &CAM_U::DriverFinalize, "DriverFinalize"},
        {0x003B, nullptr, "GetActivatedCamera"},
        {0x003C, nullptr, "GetSleepCamera"},
        {0x003D, nullptr, "SetSleepCamera"},
        {0x003E, nullptr, "SetBrightnessSynchronization"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::CAM

SERIALIZE_EXPORT_IMPL(Service::CAM::CAM_U)
