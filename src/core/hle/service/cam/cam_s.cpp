// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/cam/cam_s.h"

namespace Service::CAM {

CAM_S::CAM_S(std::shared_ptr<Module> cam) : Module::Interface(std::move(cam), "cam:s", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &CAM_S::StartCapture, "StartCapture"},
        {0x0002, &CAM_S::StopCapture, "StopCapture"},
        {0x0003, &CAM_S::IsBusy, "IsBusy"},
        {0x0004, &CAM_S::ClearBuffer, "ClearBuffer"},
        {0x0005, &CAM_S::GetVsyncInterruptEvent, "GetVsyncInterruptEvent"},
        {0x0006, &CAM_S::GetBufferErrorInterruptEvent, "GetBufferErrorInterruptEvent"},
        {0x0007, &CAM_S::SetReceiving, "SetReceiving"},
        {0x0008, &CAM_S::IsFinishedReceiving, "IsFinishedReceiving"},
        {0x0009, &CAM_S::SetTransferLines, "SetTransferLines"},
        {0x000A, &CAM_S::GetMaxLines, "GetMaxLines"},
        {0x000B, &CAM_S::SetTransferBytes, "SetTransferBytes"},
        {0x000C, &CAM_S::GetTransferBytes, "GetTransferBytes"},
        {0x000D, &CAM_S::GetMaxBytes, "GetMaxBytes"},
        {0x000E, &CAM_S::SetTrimming, "SetTrimming"},
        {0x000F, &CAM_S::IsTrimming, "IsTrimming"},
        {0x0010, &CAM_S::SetTrimmingParams, "SetTrimmingParams"},
        {0x0011, &CAM_S::GetTrimmingParams, "GetTrimmingParams"},
        {0x0012, &CAM_S::SetTrimmingParamsCenter, "SetTrimmingParamsCenter"},
        {0x0013, &CAM_S::Activate, "Activate"},
        {0x0014, &CAM_S::SwitchContext, "SwitchContext"},
        {0x0015, nullptr, "SetExposure"},
        {0x0016, nullptr, "SetWhiteBalance"},
        {0x0017, nullptr, "SetWhiteBalanceWithoutBaseUp"},
        {0x0018, nullptr, "SetSharpness"},
        {0x0019, nullptr, "SetAutoExposure"},
        {0x001A, nullptr, "IsAutoExposure"},
        {0x001B, nullptr, "SetAutoWhiteBalance"},
        {0x001C, nullptr, "IsAutoWhiteBalance"},
        {0x001D, &CAM_S::FlipImage, "FlipImage"},
        {0x001E, &CAM_S::SetDetailSize, "SetDetailSize"},
        {0x001F, &CAM_S::SetSize, "SetSize"},
        {0x0020, &CAM_S::SetFrameRate, "SetFrameRate"},
        {0x0021, nullptr, "SetPhotoMode"},
        {0x0022, &CAM_S::SetEffect, "SetEffect"},
        {0x0023, nullptr, "SetContrast"},
        {0x0024, nullptr, "SetLensCorrection"},
        {0x0025, &CAM_S::SetOutputFormat, "SetOutputFormat"},
        {0x0026, nullptr, "SetAutoExposureWindow"},
        {0x0027, nullptr, "SetAutoWhiteBalanceWindow"},
        {0x0028, nullptr, "SetNoiseFilter"},
        {0x0029, &CAM_S::SynchronizeVsyncTiming, "SynchronizeVsyncTiming"},
        {0x002A, &CAM_S::GetLatestVsyncTiming, "GetLatestVsyncTiming"},
        {0x002B, &CAM_S::GetStereoCameraCalibrationData, "GetStereoCameraCalibrationData"},
        {0x002C, nullptr, "SetStereoCameraCalibrationData"},
        {0x002D, nullptr, "WriteRegisterI2c"},
        {0x002E, nullptr, "WriteMcuVariableI2c"},
        {0x002F, nullptr, "ReadRegisterI2cExclusive"},
        {0x0030, nullptr, "ReadMcuVariableI2cExclusive"},
        {0x0031, nullptr, "SetImageQualityCalibrationData"},
        {0x0032, nullptr, "GetImageQualityCalibrationData"},
        {0x0033, &CAM_S::SetPackageParameterWithoutContext, "SetPackageParameterWithoutContext"},
        {0x0034, &CAM_S::SetPackageParameterWithContext, "SetPackageParameterWithContext"},
        {0x0035, &CAM_S::SetPackageParameterWithContextDetail, "SetPackageParameterWithContextDetail"},
        {0x0036, &CAM_S::GetSuitableY2rStandardCoefficient, "GetSuitableY2rStandardCoefficient"},
        {0x0037, nullptr, "PlayShutterSoundWithWave"},
        {0x0038, &CAM_S::PlayShutterSound, "PlayShutterSound"},
        {0x0039, &CAM_S::DriverInitialize, "DriverInitialize"},
        {0x003A, &CAM_S::DriverFinalize, "DriverFinalize"},
        {0x003B, nullptr, "GetActivatedCamera"},
        {0x003C, nullptr, "GetSleepCamera"},
        {0x003D, nullptr, "SetSleepCamera"},
        {0x003E, nullptr, "SetBrightnessSynchronization"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::CAM

SERIALIZE_EXPORT_IMPL(Service::CAM::CAM_S)
