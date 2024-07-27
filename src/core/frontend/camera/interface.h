// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/common_types.h"
#include "core/hle/service/cam/cam_params.h"

namespace Service::CAM {
struct Resolution;
} // namespace Service::CAM

namespace Camera {

/// An abstract class standing for a camera. All camera implementations should inherit from this.
class CameraInterface {
public:
    virtual ~CameraInterface();

    /// Starts the camera for video capturing.
    virtual void StartCapture() = 0;

    /// Stops the camera for video capturing.
    virtual void StopCapture() = 0;

    /**
     * Sets the video resolution from raw CAM service parameters.
     * For the meaning of the parameters, please refer to Service::CAM::Resolution. Note that the
     * actual camera implementation doesn't need to respect all the parameters. However, the width
     * and the height parameters must be respected and be used to determine the size of output
     * frames.
     * @param resolution The resolution parameters to set
     */
    virtual void SetResolution(const Service::CAM::Resolution& resolution) = 0;

    /**
     * Configures how received frames should be flipped by the camera.
     * @param flip Flip applying to the frame
     */
    virtual void SetFlip(Service::CAM::Flip flip) = 0;

    /**
     * Configures what effect should be applied to received frames by the camera.
     * @param effect Effect applying to the frame
     */
    virtual void SetEffect(Service::CAM::Effect effect) = 0;

    /**
     * Sets the output format of the all frames received after this function is called.
     * @param format Output format of the frame
     */
    virtual void SetFormat(Service::CAM::OutputFormat format) = 0;

    /**
     * Sets the recommended framerate of the camera.
     * @param frame_rate Recommended framerate
     */
    virtual void SetFrameRate(Service::CAM::FrameRate frame_rate) = 0;

    /**
     * Receives a frame from the camera.
     * This function should be only called between a StartCapture call and a StopCapture call.
     * @returns A std::vector<u16> containing pixels. The total size of the vector is width * height
     *     where width and height are set by a call to SetResolution.
     */
    virtual std::vector<u16> ReceiveFrame() = 0;

    /**
     * Test if the camera is opened successfully and can receive a preview frame. Only used for
     * preview. This function should be only called between a StartCapture call and a StopCapture
     * call.
     * @returns true if the camera is opened successfully and false otherwise
     */
    virtual bool IsPreviewAvailable() = 0;
};

} // namespace Camera
