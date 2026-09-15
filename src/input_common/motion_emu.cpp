// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>
#include <tuple>
#include <cmath>
#include "common/math_util.h"
#include "common/quaternion.h"
#include "common/thread.h"
#include "common/vector_math.h"
#include "input_common/motion_emu.h"


namespace InputCommon {

// Implementation class of the motion emulation device
class MotionEmuDevice {
public:
    MotionEmuDevice(int update_millisecond, float sensitivity, float tilt_clamp)
        : update_millisecond(update_millisecond),
          update_duration(std::chrono::duration_cast<std::chrono::steady_clock::duration>(
              std::chrono::milliseconds(update_millisecond))),
          sensitivity(sensitivity), tilt_clamp(tilt_clamp),
          motion_emu_thread(&MotionEmuDevice::MotionEmuThread, this) {}

    ~MotionEmuDevice() {
        if (motion_emu_thread.joinable()) {
            shutdown_event.Set();
            motion_emu_thread.join();
        }
    }

    void BeginTilt(int x, int y) {
        mouse_origin = Common::MakeVec(x, y);
        look_direction = Common::MakeVec(0.0f, 0.0f);
        is_tilting = true;
        is_looking = false;
    }

    void BeginLook(int x, int y) {
        mouse_origin = Common::MakeVec(x, y);
        previous_look_direction = look_direction;
        is_looking = true;
    }

    void Tilt(int x, int y) {
        auto mouse_move = Common::MakeVec(x, y) - mouse_origin;
        if (is_tilting) {
            std::lock_guard guard{tilt_mutex};
            if (mouse_move.x == 0 && mouse_move.y == 0) {
                tilt_angle = 0;
            } else {
                tilt_direction = mouse_move.Cast<float>();
                tilt_angle = std::clamp(tilt_direction.Normalize() * sensitivity, 0.0f,
                                        Common::PI * this->tilt_clamp / 180.0f);
            }
        }
    }

    void Look(int x, int y) {
        auto mouse_move = Common::MakeVec(x, y) - mouse_origin + previous_look_direction.Cast<int>();
        if (is_looking) {
            std::lock_guard guard{tilt_mutex};
            if (!(mouse_move.x == 0 && mouse_move.y == 0)) {
                look_direction = mouse_move.Cast<float>();
            }
        }
    }

    void EndTilt() {
        std::lock_guard guard{tilt_mutex};
        tilt_angle = 0;
        is_tilting = false;
    }

    void EndLook() {
        is_looking = false;
    }

    std::tuple<Common::Vec3<float>, Common::Vec3<float>> GetStatus() {
        std::lock_guard guard{status_mutex};
        return status;
    }

private:
    const int update_millisecond;
    const std::chrono::steady_clock::duration update_duration;
    const float sensitivity;

    Common::Vec2<int> mouse_origin;

    std::mutex tilt_mutex;
    Common::Vec2<float> tilt_direction;
    Common::Vec2<float> look_direction;
    Common::Vec2<float> previous_look_direction = Common::MakeVec(0.0f,0.0f);
    float tilt_angle = 0;
    float tilt_clamp = 90;

    bool is_tilting = false;
    bool is_looking = false;

    Common::Event shutdown_event;

    std::tuple<Common::Vec3<float>, Common::Vec3<float>> status;
    std::mutex status_mutex;

    // Note: always keep the thread declaration at the end so that other objects are initialized
    // before this!
    std::thread motion_emu_thread;

    void MotionEmuThread() {
        auto update_time = std::chrono::steady_clock::now();
        Common::Quaternion<float> q = {{0.0f, 0.0f, 0.0f}, 1.0f};
        Common::Quaternion<float> old_q;

        while (!shutdown_event.WaitUntil(update_time)) {
            update_time += update_duration;
            old_q = q;

            {
                std::lock_guard guard{tilt_mutex};

                if (is_tilting) {
                    // Find the quaternion describing current 3DS tilting
                    q = Common::MakeQuaternion(Common::MakeVec(-tilt_direction.y, 0.0f, tilt_direction.x), tilt_angle);
                }else {
                    // Find the quaternion describing current 3DS looking
                    float p = -1.0f * ((Common::PI * 0.25f) + Common::PI * (-look_direction.y * sensitivity * 100.0f) / 360.0f);
                    float y = Common::PI * (-look_direction.x * sensitivity * 100.0f) / 360.0f;
                    float qw = std::cos(p) * std::cos(y);
                    float qx = std::sin(p) * std::cos(y);
                    float qy = std::cos(p) * std::sin(y);
                    float qz = -1.0f * std::sin(p) * std::sin(y);

                    q = {Common::MakeVec(qx, qy, qz), qw};
                }
            }

            auto inv_q = q.Inverse();

            // Set the gravity vector in world space
            auto gravity = Common::MakeVec(0.0f, -1.0f, 0.0f);

            // Find the angular rate vector in world space
            auto angular_rate = ((q - old_q) * inv_q).xyz * 2;
            angular_rate *= 1000 / update_millisecond / Common::PI * 180;

            // Transform the two vectors from world space to 3DS space
            gravity = QuaternionRotate(inv_q, gravity);
            angular_rate = QuaternionRotate(inv_q, angular_rate);

            // Update the sensor state
            {
                std::lock_guard guard{status_mutex};
                status = std::make_tuple(gravity, angular_rate);
            }
        }
    }
};

// Interface wrapper held by input receiver as a unique_ptr. It holds the implementation class as
// a shared_ptr, which is also observed by the factory class as a weak_ptr. In this way the factory
// can forward all the inputs to the implementation only when it is valid.
class MotionEmuDeviceWrapper : public Input::MotionDevice {
public:
    MotionEmuDeviceWrapper(int update_millisecond, float sensitivity, float tilt_clamp) {
        device = std::make_shared<MotionEmuDevice>(update_millisecond, sensitivity, tilt_clamp);
    }

    std::tuple<Common::Vec3<float>, Common::Vec3<float>> GetStatus() const override {
        return device->GetStatus();
    }

    std::shared_ptr<MotionEmuDevice> device;
};

std::unique_ptr<Input::MotionDevice> MotionEmu::Create(const Common::ParamPackage& params) {
    int update_period = params.Get("update_period", 100);
    float sensitivity = params.Get("sensitivity", 0.01f);
    float tilt_clamp = params.Get("tilt_clamp", 90.0f);
    auto device_wrapper =
        std::make_unique<MotionEmuDeviceWrapper>(update_period, sensitivity, tilt_clamp);
    // Previously created device is disconnected here. Having two motion devices for 3DS is not
    // expected.
    current_device = device_wrapper->device;
    return std::move(device_wrapper);
}

void MotionEmu::BeginTilt(int x, int y) {
    if (auto ptr = current_device.lock()) {
        ptr->BeginTilt(x, y);
    }
}

void MotionEmu::BeginLook(int x, int y) {
    if (auto ptr = current_device.lock()) {
        ptr->BeginLook(x, y);
    }
}

void MotionEmu::Tilt(int x, int y) {
    if (auto ptr = current_device.lock()) {
        ptr->Tilt(x, y);
    }
}

void MotionEmu::Look(int x, int y) {
    if (auto ptr = current_device.lock()) {
        ptr->Look(x, y);
    }
}

void MotionEmu::EndTilt() {
    if (auto ptr = current_device.lock()) {
        ptr->EndTilt();
    }
}

void MotionEmu::EndLook() {
    if (auto ptr = current_device.lock()) {
        ptr->EndLook();
    }
}

} // namespace InputCommon
