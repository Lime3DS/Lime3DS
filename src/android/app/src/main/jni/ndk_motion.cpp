// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <thread>

#include <android/sensor.h>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/vector_math.h"
#include "jni/ndk_motion.h"

namespace InputManager {

namespace {
using Common::Vec3;
}

class NDKMotion final : public Input::MotionDevice {
    std::chrono::microseconds update_period;

    ASensorManager* sensor_manager = nullptr;
    ALooper* looper = nullptr;
    ASensorEventQueue* event_queue;

    mutable std::atomic<Vec3<float>> acceleration{};
    mutable std::atomic<Vec3<float>> rotation{};
    static_assert(decltype(acceleration)::is_always_lock_free, "vectors are not lock free");
    std::thread poll_thread;
    std::atomic<bool> stop_polling = false;

    static Vec3<float> TransformAxes(Vec3<float> in) {
        // 3DS   Y+            Phone     Z+
        // on    |             laying    |
        // table |             in        |
        //       |_______ X-   portrait  |_______ X+
        //      /              mode     /
        //     /                       /
        //    Z-                      Y-
        Vec3<float> out;
        out.y = in.z;
        // rotations are 90 degrees counter-clockwise from portrait
        switch (screen_rotation) {
        case 0:
            out.x = -in.x;
            out.z = in.y;
            break;
        case 1:
            out.x = in.y;
            out.z = in.x;
            break;
        case 2:
            out.x = in.x;
            out.z = -in.y;
            break;
        case 3:
            out.x = -in.y;
            out.z = -in.x;
            break;
        default:
            UNREACHABLE();
        }
        return out;
    }

    void Construct() {
        sensor_manager = ASensorManager_getInstanceForPackage("io.github.lime3ds.android");
        looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
        if (!sensor_manager || !looper) {
            LOG_CRITICAL(Input, "Could not retrieve sensor manager");
            return;
        }
        event_queue = ASensorManager_createEventQueue(sensor_manager, looper, 0, nullptr, nullptr);
        if (!event_queue) {
            LOG_ERROR(Input, "Could not create sensor event queue");
            return;
        }

        EnableSensors();
    }

    void Destruct() {
        ASensorManager_destroyEventQueue(sensor_manager, event_queue);
    }

    void Update() const {
        ALooper_pollOnce(0, nullptr, nullptr, nullptr);
        ASensorEvent event{};
        std::optional<Vec3<float>> new_accel{}, new_rot{};
        while (ASensorEventQueue_getEvents(event_queue, &event, 1) > 0) {
            if (event.type == ASENSOR_TYPE_ACCELEROMETER) {
                new_accel.emplace(event.vector.x, event.vector.y, event.vector.z);
            } else if (event.type == ASENSOR_TYPE_GYROSCOPE) {
                new_rot.emplace(event.vector.x, event.vector.y, event.vector.z);
            }
            // occasionally the queue has ASENSOR_TYPE_ADDITIONAL_INFO events
            // but so far there is no reason to handle them
        }
        if (new_accel) {
            // convert from m/(s^2) to g and invert
            acceleration = TransformAxes(*new_accel) / -ASENSOR_STANDARD_GRAVITY;
        }
        if (new_rot) {
            // convert from rad/s to deg/s
            rotation = TransformAxes(*new_rot) * 180.0f / static_cast<float>(M_PI);
        }
    }

public:
    NDKMotion(std::chrono::microseconds update_period_, bool asynchronous = false)
        : update_period(update_period_) {
        if (asynchronous) {
            poll_thread = std::thread([this] {
                Construct();
                auto start = std::chrono::high_resolution_clock::now();
                while (!stop_polling) {
                    Update();
                    std::this_thread::sleep_until(start += update_period);
                }
                Destruct();
            });
        } else {
            Construct();
        }
    }

    ~NDKMotion() {
        if (std::thread::id{} == poll_thread.get_id()) {
            Destruct();
        } else {
            stop_polling = true;
            poll_thread.join();
        }
    }

    std::tuple<Vec3<float>, Vec3<float>> GetStatus() const override {
        if (std::thread::id{} == poll_thread.get_id()) {
            Update();
        }
        return {acceleration, rotation};
    }

    void EnableSensors() {
        const auto init_sensor = [this](int sensor_type) {
            ASensorRef sensor = ASensorManager_getDefaultSensor(sensor_manager, sensor_type);
            if (!sensor) {
                LOG_ERROR(Input, "Could not find sensor of type {}", sensor_type);
                return;
            }
            int error = ASensorEventQueue_registerSensor(
                event_queue, sensor,
                std::max(ASensor_getMinDelay(sensor), static_cast<int>(update_period.count())), 0);
            if (error < 0)
                LOG_ERROR(Input, "Registering sensor returned error code {}", error);
        };

        LOG_TRACE(Input, "Enabling sensors..");
        init_sensor(ASENSOR_TYPE_ACCELEROMETER);
        init_sensor(ASENSOR_TYPE_GYROSCOPE);
    }

    void DisableSensors() {
        const auto disable_sensor = [this](int sensor_type) {
            ASensorRef sensor = ASensorManager_getDefaultSensor(sensor_manager, sensor_type);
            if (!sensor) {
                LOG_ERROR(Input, "Could not find sensor of type {}", sensor_type);
                return;
            }
            int error = ASensorEventQueue_disableSensor(event_queue, sensor);
            if (error < 0)
                LOG_ERROR(Input, "Disabling sensor returned error code {}", error);
        };

        LOG_TRACE(Input, "Disabling sensors..");
        disable_sensor(ASENSOR_TYPE_ACCELEROMETER);
        disable_sensor(ASENSOR_TYPE_GYROSCOPE);
    }
};

std::unique_ptr<Input::MotionDevice> NDKMotionFactory::Create(const Common::ParamPackage& params) {
    std::chrono::milliseconds update_period{params.Get("update_period", 4)};
    std::unique_ptr<NDKMotion> ndk_motion = std::make_unique<NDKMotion>(update_period);
    ndk_motion_device = ndk_motion.get();
    return std::move(ndk_motion);
}

void NDKMotionFactory::EnableSensors() {
    if (ndk_motion_device)
        ndk_motion_device->EnableSensors();
}

void NDKMotionFactory::DisableSensors() {
    if (ndk_motion_device)
        ndk_motion_device->DisableSensors();
}

} // namespace InputManager
