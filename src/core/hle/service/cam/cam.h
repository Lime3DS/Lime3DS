// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <deque>
#include <future>
#include <memory>
#include <vector>
#include <boost/serialization/array.hpp>
#include <boost/serialization/deque.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <boost/serialization/version.hpp>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/global.h"
#include "core/hle/result.h"
#include "core/hle/service/cam/cam_params.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Camera {
class CameraInterface;
}

namespace Core {
struct TimingEventType;
}

namespace Kernel {
class Process;
}

namespace Service::CAM {

/// Stereo camera calibration data.
struct StereoCameraCalibrationData {
    u8 isValidRotationXY; ///< Bool indicating whether the X and Y rotation data is valid.
    INSERT_PADDING_BYTES(3);
    float_le scale;        ///< Scale to match the left camera image with the right.
    float_le rotationZ;    ///< Z axis rotation to match the left camera image with the right.
    float_le translationX; ///< X axis translation to match the left camera image with the right.
    float_le translationY; ///< Y axis translation to match the left camera image with the right.
    float_le rotationX;    ///< X axis rotation to match the left camera image with the right.
    float_le rotationY;    ///< Y axis rotation to match the left camera image with the right.
    float_le angleOfViewRight; ///< Right camera angle of view.
    float_le angleOfViewLeft;  ///< Left camera angle of view.
    float_le distanceToChart;  ///< Distance between cameras and measurement chart.
    float_le distanceCameras;  ///< Distance between left and right cameras.
    s16_le imageWidth;         ///< Image width.
    s16_le imageHeight;        ///< Image height.
    INSERT_PADDING_BYTES(16);
};
static_assert(sizeof(StereoCameraCalibrationData) == 64,
              "StereoCameraCalibrationData structure size is wrong");

/**
 * Resolution parameters for the camera.
 * The native resolution of 3DS camera is 640 * 480. The captured image will be cropped in the
 * region [crop_x0, crop_x1] * [crop_y0, crop_y1], and then scaled to size width * height as the
 * output image. Note that all cropping coordinates are inclusive.
 */
struct Resolution {
    u16 width;
    u16 height;
    u16 crop_x0;
    u16 crop_y0;
    u16 crop_x1;
    u16 crop_y1;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & width;
        ar & height;
        ar & crop_x0;
        ar & crop_y0;
        ar & crop_x1;
        ar & crop_y1;
    }
    friend class boost::serialization::access;
};

struct PackageParameterWithoutContext {
    u8 camera_select;
    s8 exposure;
    WhiteBalance white_balance;
    s8 sharpness;
    bool auto_exposure;
    bool auto_white_balance;
    FrameRate frame_rate;
    PhotoMode photo_mode;
    Contrast contrast;
    LensCorrection lens_correction;
    bool noise_filter;
    u8 padding;
    s16 auto_exposure_window_x;
    s16 auto_exposure_window_y;
    s16 auto_exposure_window_width;
    s16 auto_exposure_window_height;
    s16 auto_white_balance_window_x;
    s16 auto_white_balance_window_y;
    s16 auto_white_balance_window_width;
    s16 auto_white_balance_window_height;
    INSERT_PADDING_WORDS(4);
};

static_assert(sizeof(PackageParameterWithoutContext) == 44,
              "PackageParameterCameraWithoutContext structure size is wrong");

struct PackageParameterWithContext {
    u8 camera_select;
    u8 context_select;
    Flip flip;
    Effect effect;
    Size size;
    INSERT_PADDING_BYTES(3);
    INSERT_PADDING_WORDS(3);

    Resolution GetResolution() const;
};

static_assert(sizeof(PackageParameterWithContext) == 20,
              "PackageParameterWithContext structure size is wrong");

struct PackageParameterWithContextDetail {
    u8 camera_select;
    u8 context_select;
    Flip flip;
    Effect effect;
    Resolution resolution;
    INSERT_PADDING_WORDS(3);

    Resolution GetResolution() const {
        return resolution;
    }
};

static_assert(sizeof(PackageParameterWithContextDetail) == 28,
              "PackageParameterWithContextDetail structure size is wrong");

class Module final {
public:
    explicit Module(Core::System& system);
    ~Module();
    void ReloadCameraDevices();

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> cam, const char* name, u32 max_session);
        ~Interface();

        std::shared_ptr<Module> GetModule() const;

    protected:
        /**
         * Starts capturing at the selected port.
         *  Inputs:
         *      0: 0x00010040
         *      1: u8 selected port
         *  Outputs:
         *      0: 0x00010040
         *      1: Result
         */
        void StartCapture(Kernel::HLERequestContext& ctx);

        /**
         * Stops capturing from the selected port.
         *  Inputs:
         *      0: 0x00020040
         *      1: u8 selected port
         *  Outputs:
         *      0: 0x00020040
         *      1: Result
         */
        void StopCapture(Kernel::HLERequestContext& ctx);

        /**
         * Gets whether the selected port is currently capturing.
         *  Inputs:
         *      0: 0x00030040
         *      1: u8 selected port
         *  Outputs:
         *      0: 0x00030080
         *      1: Result
         *      2: 0 if not capturing, 1 if capturing
         */
        void IsBusy(Kernel::HLERequestContext& ctx);

        /**
         * Clears the buffer of selected ports.
         *  Inputs:
         *      0: 0x00040040
         *      1: u8 selected port
         *  Outputs:
         *      0: 0x00040040
         *      2: Result
         */
        void ClearBuffer(Kernel::HLERequestContext& ctx);

        /**
         * Unknown
         *  Inputs:
         *      0: 0x00050040
         *      1: u8 selected port
         *  Outputs:
         *      0: 0x00050042
         *      1: Result
         *      2: Descriptor: Handle
         *      3: Event handle
         */
        void GetVsyncInterruptEvent(Kernel::HLERequestContext& ctx);

        /**
         * Unknown
         *  Inputs:
         *      0: 0x00060040
         *      1: u8 selected port
         *  Outputs:
         *      0: 0x00060042
         *      1: Result
         *      2: Descriptor: Handle
         *      3: Event handle
         */
        void GetBufferErrorInterruptEvent(Kernel::HLERequestContext& ctx);

        /**
         * Sets the target buffer to receive a frame of image data and starts the transfer. Each
         * camera port has its own event to signal the end of the transfer.
         *
         *  Inputs:
         *      0: 0x00070102
         *      1: Destination address in calling process
         *      2: u8 selected port
         *      3: Image size (in bytes)
         *      4: u16 Transfer unit size (in bytes)
         *      5: Descriptor: Handle
         *      6: Handle to destination process
         *  Outputs:
         *      0: 0x00070042
         *      1: Result
         *      2: Descriptor: Handle
         *      3: Handle to event signalled when transfer finishes
         */
        void SetReceiving(Kernel::HLERequestContext& ctx);

        /**
         * Gets whether the selected port finished receiving a frame.
         *  Inputs:
         *      0: 0x00080040
         *      1: u8 selected port
         *  Outputs:
         *      0: 0x00080080
         *      1: Result
         *      2: 0 if not finished, 1 if finished
         */
        void IsFinishedReceiving(Kernel::HLERequestContext& ctx);

        /**
         * Sets the number of lines the buffer contains.
         *  Inputs:
         *      0: 0x00090100
         *      1: u8 selected port
         *      2: u16 Number of lines to transfer
         *      3: u16 Width
         *      4: u16 Height
         *  Outputs:
         *      0: 0x00090040
         *      1: Result
         * @todo figure out how the "buffer" actually works.
         */
        void SetTransferLines(Kernel::HLERequestContext& ctx);

        /**
         * Gets the maximum number of lines that fit in the buffer
         *  Inputs:
         *      0: 0x000A0080
         *      1: u16 Width
         *      2: u16 Height
         *  Outputs:
         *      0: 0x000A0080
         *      1: Result
         *      2: Maximum number of lines that fit in the buffer
         * @todo figure out how the "buffer" actually works.
         */
        void GetMaxLines(Kernel::HLERequestContext& ctx);

        /**
         * Sets the number of bytes the buffer contains.
         *  Inputs:
         *      0: 0x000B0100
         *      1: u8 selected port
         *      2: u16 Number of bytes to transfer
         *      3: u16 Width
         *      4: u16 Height
         *  Outputs:
         *      0: 0x000B0040
         *      1: Result
         * @todo figure out how the "buffer" actually works.
         */
        void SetTransferBytes(Kernel::HLERequestContext& ctx);

        /**
         * Gets the number of bytes to the buffer contains.
         *  Inputs:
         *      0: 0x000C0040
         *      1: u8 selected port
         *  Outputs:
         *      0: 0x000C0080
         *      1: Result
         *      2: The number of bytes the buffer contains
         * @todo figure out how the "buffer" actually works.
         */
        void GetTransferBytes(Kernel::HLERequestContext& ctx);

        /**
         * Gets the maximum number of bytes that fit in the buffer.
         *  Inputs:
         *      0: 0x000D0080
         *      1: u16 Width
         *      2: u16 Height
         *  Outputs:
         *      0: 0x000D0080
         *      1: Result
         *      2: Maximum number of bytes that fit in the buffer
         * @todo figure out how the "buffer" actually works.
         */
        void GetMaxBytes(Kernel::HLERequestContext& ctx);

        /**
         * Enables or disables trimming.
         *  Inputs:
         *      0: 0x000E0080
         *      1: u8 selected port
         *      2: u8 bool Enable trimming if true
         *  Outputs:
         *      0: 0x000E0040
         *      1: Result
         */
        void SetTrimming(Kernel::HLERequestContext& ctx);

        /**
         * Gets whether trimming is enabled.
         *  Inputs:
         *      0: 0x000F0040
         *      1: u8 selected port
         *  Outputs:
         *      0: 0x000F0080
         *      1: Result
         *      2: u8 bool Enable trimming if true
         */
        void IsTrimming(Kernel::HLERequestContext& ctx);

        /**
         * Sets the position to trim.
         *  Inputs:
         *      0: 0x00100140
         *      1: u8 selected port
         *      2: x start
         *      3: y start
         *      4: x end (exclusive)
         *      5: y end (exclusive)
         *  Outputs:
         *      0: 0x00100040
         *      1: Result
         */
        void SetTrimmingParams(Kernel::HLERequestContext& ctx);

        /**
         * Gets the position to trim.
         *  Inputs:
         *      0: 0x00110040
         *      1: u8 selected port
         *
         *  Outputs:
         *      0: 0x00110140
         *      1: Result
         *      2: x start
         *      3: y start
         *      4: x end (exclusive)
         *      5: y end (exclusive)
         */
        void GetTrimmingParams(Kernel::HLERequestContext& ctx);

        /**
         * Sets the position to trim by giving the width and height. The trimming window is always
         * at the center.
         *  Inputs:
         *      0: 0x00120140
         *      1: u8 selected port
         *      2: s16 Trim width
         *      3: s16 Trim height
         *      4: s16 Camera width
         *      5: s16 Camera height
         *  Outputs:
         *      0: 0x00120040
         *      1: Result
         */
        void SetTrimmingParamsCenter(Kernel::HLERequestContext& ctx);

        /**
         * Selects up to two physical cameras to enable.
         *  Inputs:
         *      0: 0x00130040
         *      1: u8 selected camera
         *  Outputs:
         *      0: 0x00130040
         *      1: Result
         */
        void Activate(Kernel::HLERequestContext& ctx);

        /**
         * Switches the context of camera settings.
         *  Inputs:
         *      0: 0x00140080
         *      1: u8 selected camera
         *      2: u8 selected context
         *  Outputs:
         *      0: 0x00140040
         *      1: Result
         */
        void SwitchContext(Kernel::HLERequestContext& ctx);

        /**
         * Sets flipping of images
         *  Inputs:
         *      0: 0x001D00C0
         *      1: u8 selected camera
         *      2: u8 Type of flipping to perform (`Flip` enum)
         *      3: u8 selected context
         *  Outputs:
         *      0: 0x001D0040
         *      1: Result
         */
        void FlipImage(Kernel::HLERequestContext& ctx);

        /**
         * Sets camera resolution from custom parameters. For more details see the Resolution
         * struct.
         *  Inputs:
         *      0: 0x001E0200
         *      1: u8 selected camera
         *      2: width
         *      3: height
         *      4: crop x0
         *      5: crop y0
         *      6: crop x1
         *      7: crop y1
         *      8: u8 selected context
         *  Outputs:
         *      0: 0x001E0040
         *      1: Result
         */
        void SetDetailSize(Kernel::HLERequestContext& ctx);

        /**
         * Sets camera resolution from preset resolution parameters.
         *  Inputs:
         *      0: 0x001F00C0
         *      1: u8 selected camera
         *      2: u8 Camera frame resolution (`Size` enum)
         *      3: u8 selected context
         *  Outputs:
         *      0: 0x001F0040
         *      1: Result
         */
        void SetSize(Kernel::HLERequestContext& ctx);

        /**
         * Sets camera framerate.
         *  Inputs:
         *      0: 0x00200080
         *      1: u8 selected camera
         *      2: u8 Camera framerate (`FrameRate` enum)
         *  Outputs:
         *      0: 0x00200040
         *      1: Result
         */
        void SetFrameRate(Kernel::HLERequestContext& ctx);

        /**
         * Sets effect on the output image
         *  Inputs:
         *      0: 0x002200C0
         *      1: u8 selected camera
         *      2: u8 image effect (`Effect` enum)
         *      3: u8 selected context
         *  Outputs:
         *      0: 0x00220040
         *      1: Result
         */
        void SetEffect(Kernel::HLERequestContext& ctx);

        /**
         * Sets format of the output image
         *  Inputs:
         *      0: 0x002500C0
         *      1: u8 selected camera
         *      2: u8 image format (`OutputFormat` enum)
         *      3: u8 selected context
         *  Outputs:
         *      0: 0x00250040
         *      1: Result
         */
        void SetOutputFormat(Kernel::HLERequestContext& ctx);

        /**
         * Synchronizes the V-Sync timing of two cameras.
         *  Inputs:
         *      0: 0x00290080
         *      1: u8 selected camera 1
         *      2: u8 selected camera 2
         *  Outputs:
         *      0: 0x00280040
         *      1: Result
         */
        void SynchronizeVsyncTiming(Kernel::HLERequestContext& ctx);

        /**
         * Gets the vsync timing record of the specified camera for the specified number of signals.
         *  Inputs:
         *      0: 0x002A0080
         *      1: Port
         *      2: Number of timings to get
         *      64: ((PastTimings * 8) << 14) | 2
         *      65: s64* TimingsOutput
         *  Outputs:
         *      0: 0x002A0042
         *      1: Result
         *      2-3: Output static buffer
         */
        void GetLatestVsyncTiming(Kernel::HLERequestContext& ctx);

        /**
         * Returns calibration data relating the outside cameras to each other, for use in AR
         * applications.
         *
         *  Inputs:
         *      0: 0x002B0000
         *  Outputs:
         *      0: 0x002B0440
         *      1: Result
         *      2-17: `StereoCameraCalibrationData` structure with calibration values
         */
        void GetStereoCameraCalibrationData(Kernel::HLERequestContext& ctx);

        /**
         * Batch-configures context-free settings.
         *
         *  Inputs:
         *      0: 0x003302C0
         *      1-7: struct PachageParameterWithoutContext
         *      8-11: unused
         *  Outputs:
         *      0: 0x00330040
         *      1: Result
         */
        void SetPackageParameterWithoutContext(Kernel::HLERequestContext& ctx);

        /**
         * Batch-configures context-related settings with preset resolution parameters.
         *
         *  Inputs:
         *      0: 0x00340140
         *      1-2: struct PackageParameterWithContext
         *      3-5: unused
         *  Outputs:
         *      0: 0x00340040
         *      1: Result
         */
        void SetPackageParameterWithContext(Kernel::HLERequestContext& ctx);

        /**
         * Batch-configures context-related settings with custom resolution parameters
         *
         *  Inputs:
         *      0: 0x003501C0
         *      1-4: struct PackageParameterWithContextDetail
         *      5-7: unused
         *  Outputs:
         *      0: 0x00350040
         *      1: Result
         */
        void SetPackageParameterWithContextDetail(Kernel::HLERequestContext& ctx);

        /**
         * Unknown
         *  Inputs:
         *      0: 0x00360000
         *  Outputs:
         *      0: 0x00360080
         *      1: Result
         *      2: ?
         */
        void GetSuitableY2rStandardCoefficient(Kernel::HLERequestContext& ctx);

        /**
         * Unknown
         *  Inputs:
         *      0: 0x00380040
         *      1: u8 Sound ID
         *  Outputs:
         *      0: 0x00380040
         *      1: Result
         */
        void PlayShutterSound(Kernel::HLERequestContext& ctx);

        /**
         * Initializes the camera driver. Must be called before using other functions.
         *  Inputs:
         *      0: 0x00390000
         *  Outputs:
         *      0: 0x00390040
         *      1: Result
         */
        void DriverInitialize(Kernel::HLERequestContext& ctx);

        /**
         * Shuts down the camera driver.
         *  Inputs:
         *      0: 0x003A0000
         *  Outputs:
         *      0: 0x003A0040
         *      1: Result
         */
        void DriverFinalize(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> cam;
    };

private:
    void CompletionEventCallBack(u64 port_id, s64);
    void VsyncInterruptEventCallBack(u64 port_id, s64 cycles_late);

    // Starts a receiving process on the specified port. This can only be called when is_busy = true
    // and is_receiving = false.
    void StartReceiving(int port_id);

    // Cancels any ongoing receiving processes at the specified port. This is used by functions that
    // stop capturing.
    // TODO: what is the exact behaviour on real 3DS when stopping capture during an ongoing
    //       process? Will the completion event still be signaled?
    void CancelReceiving(int port_id);

    // Activates the specified port with the specfied camera.
    void ActivatePort(int port_id, int camera_id);

    template <typename PackageParameterType>
    Result SetPackageParameter(const PackageParameterType& package);

    struct ContextConfig {
        Flip flip{Flip::NoFlip};
        Effect effect{Effect::NoEffect};
        OutputFormat format{OutputFormat::YUV422};
        Resolution resolution = {0, 0, 0, 0, 0, 0};

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar & flip;
            ar & effect;
            ar & format;
            ar & resolution;
        }
        friend class boost::serialization::access;
    };

    struct CameraConfig {
        std::unique_ptr<Camera::CameraInterface> impl;
        std::array<ContextConfig, 2> contexts;
        int current_context{0};
        FrameRate frame_rate{FrameRate::Rate_15};

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int file_version) {
            ar & contexts;
            ar & current_context;
            ar & frame_rate;
        }
        friend class boost::serialization::access;
    };

    struct PortConfig {
        int camera_id{0};

        bool is_active{false}; // set when the port is activated by an Activate call.

        // set if SetReceiving is called when is_busy = false. When StartCapture is called then,
        // this will trigger a receiving process and reset itself.
        bool is_pending_receiving{false};

        // set when StartCapture is called and reset when StopCapture is called.
        bool is_busy{false};

        bool is_receiving{false}; // set when there is an ongoing receiving process.

        bool is_trimming{false};
        u16 x0{0}; // x-coordinate of starting position for trimming
        u16 y0{0}; // y-coordinate of starting position for trimming
        u16 x1{0}; // x-coordinate of ending position for trimming
        u16 y1{0}; // y-coordinate of ending position for trimming

        u16 transfer_bytes{256};

        std::shared_ptr<Kernel::Event> completion_event;
        std::shared_ptr<Kernel::Event> buffer_error_interrupt_event;
        std::shared_ptr<Kernel::Event> vsync_interrupt_event;

        std::deque<s64> vsync_timings;

        std::future<std::vector<u16>> capture_result; // will hold the received frame.
        Kernel::Process* dest_process{nullptr};
        VAddr dest{0};    // the destination address of the receiving process
        u32 dest_size{0}; // the destination size of the receiving process

        void Clear();

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar & camera_id;
            ar & is_active;
            ar & is_pending_receiving;
            ar & is_busy;
            ar & is_receiving;
            ar & is_trimming;
            ar & x0;
            ar & y0;
            ar & x1;
            ar & y1;
            ar & transfer_bytes;
            ar & completion_event;
            ar & buffer_error_interrupt_event;
            ar & vsync_interrupt_event;
            ar & vsync_timings;
            // Ignore capture_result. In-progress captures might be affected but this is OK.
            ar & dest_process;
            ar & dest;
            ar & dest_size;
        }
        friend class boost::serialization::access;
    };

    void LoadCameraImplementation(CameraConfig& camera, int camera_id);

    Core::System& system;
    bool initialized{};
    std::array<CameraConfig, NumCameras> cameras;
    std::array<PortConfig, 2> ports;
    Core::TimingEventType* completion_event_callback;
    Core::TimingEventType* vsync_interrupt_event_callback;
    std::atomic<bool> is_camera_reload_pending{false};

    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version);
    friend class boost::serialization::access;
};

std::shared_ptr<Module> GetModule(Core::System& system);

void InstallInterfaces(Core::System& system);

} // namespace Service::CAM

SERVICE_CONSTRUCT(Service::CAM::Module)
BOOST_CLASS_VERSION(Service::CAM::Module, 1)
BOOST_CLASS_VERSION(Service::CAM::Module::CameraConfig, 1)
