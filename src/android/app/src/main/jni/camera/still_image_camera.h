// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <jni.h>
#include "common/common_types.h"
#include "core/frontend/camera/factory.h"
#include "core/frontend/camera/interface.h"
#include "core/hle/service/cam/cam.h"
#include "jni/id_cache.h"

namespace Camera::StillImage {

class Interface final : public CameraInterface {
public:
    Interface(SharedGlobalRef<jstring> path, const Service::CAM::Flip& flip);
    ~Interface();
    void StartCapture() override;
    void StopCapture() override {};
    void SetResolution(const Service::CAM::Resolution& resolution) override;
    void SetFlip(Service::CAM::Flip flip) override;
    void SetEffect(Service::CAM::Effect effect) override {};
    void SetFormat(Service::CAM::OutputFormat format) override;
    void SetFrameRate(Service::CAM::FrameRate frame_rate) override {};
    std::vector<u16> ReceiveFrame() override;
    bool IsPreviewAvailable() override;

private:
    SharedGlobalRef<jstring> path;
    Service::CAM::Resolution resolution;

    // Flipping parameters. mirror = horizontal, invert = vertical.
    bool base_mirror{};
    bool base_invert{};
    bool mirror{};
    bool invert{};

    Service::CAM::OutputFormat format;
    std::vector<u16> image; // Data fetched from the frontend
    bool opened{};          // Whether the camera was successfully opened
};

class Factory final : public CameraFactory {
public:
    std::unique_ptr<CameraInterface> Create(const std::string& config,
                                            const Service::CAM::Flip& flip) override;

private:
    /// Record the path chosen to avoid multiple prompt problem
    static SharedGlobalRef<jstring> last_path;

    friend class Interface;
};

void InitJNI(JNIEnv* env);
void CleanupJNI(JNIEnv* env);

} // namespace Camera::StillImage
