// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/frontend/camera/blank_camera.h"
#include "core/hle/service/cam/cam.h"

namespace Camera {

void BlankCamera::StartCapture() {}

void BlankCamera::StopCapture() {}

void BlankCamera::SetFormat(Service::CAM::OutputFormat output_format) {
    output_rgb = output_format == Service::CAM::OutputFormat::RGB565;
}

void BlankCamera::SetResolution(const Service::CAM::Resolution& resolution) {
    width = resolution.width;
    height = resolution.height;
};

void BlankCamera::SetFlip(Service::CAM::Flip) {}

void BlankCamera::SetEffect(Service::CAM::Effect) {}

std::vector<u16> BlankCamera::ReceiveFrame() {
    // Note: 0x80008000 stands for two black pixels in YUV422
    return std::vector<u16>(width * height, output_rgb ? 0 : 0x8000);
}

bool BlankCamera::IsPreviewAvailable() {
    return true;
}

} // namespace Camera
