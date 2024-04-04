// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <android/bitmap.h>
#include <libyuv.h>
#include "common/logging/log.h"
#include "core/frontend/camera/blank_camera.h"
#include "jni/camera/still_image_camera.h"

static jclass s_still_image_camera_helper_class;
static jmethodID s_open_file_picker;
static jmethodID s_load_image_from_file;

namespace Camera::StillImage {

void InitJNI(JNIEnv* env) {
    s_still_image_camera_helper_class = reinterpret_cast<jclass>(env->NewGlobalRef(
        env->FindClass("io/github/lime3ds/android/camera/StillImageCameraHelper")));
    s_open_file_picker = env->GetStaticMethodID(s_still_image_camera_helper_class, "OpenFilePicker",
                                                "()Ljava/lang/String;");
    s_load_image_from_file =
        env->GetStaticMethodID(s_still_image_camera_helper_class, "LoadImageFromFile",
                               "(Ljava/lang/String;II)Landroid/graphics/Bitmap;");
}

void CleanupJNI(JNIEnv* env) {
    env->DeleteGlobalRef(s_still_image_camera_helper_class);
}

Interface::Interface(SharedGlobalRef<jstring> path_, const Service::CAM::Flip& flip)
    : path(std::move(path_)) {
    mirror = base_mirror =
        flip == Service::CAM::Flip::Horizontal || flip == Service::CAM::Flip::Reverse;
    invert = base_invert =
        flip == Service::CAM::Flip::Vertical || flip == Service::CAM::Flip::Reverse;
}

Interface::~Interface() {
    Factory::last_path.reset();
}

void Interface::StartCapture() {
    JNIEnv* env = IDCache::GetEnvForThread();
    jobject bitmap =
        env->CallStaticObjectMethod(s_still_image_camera_helper_class, s_load_image_from_file,
                                    path.get(), resolution.width, resolution.height);
    if (bitmap == nullptr) {
        LOG_ERROR(Frontend, "Could not load image from file");
        opened = false;
        return;
    }

    int ret;

#define BITMAP_CALL(func)                                                                          \
    ret = AndroidBitmap_##func;                                                                    \
    if (ret != ANDROID_BITMAP_RESULT_SUCCESS) {                                                    \
        LOG_ERROR(Frontend, #func " failed with code {}", ret);                                    \
        opened = false;                                                                            \
        return;                                                                                    \
    }

    AndroidBitmapInfo info;
    BITMAP_CALL(getInfo(env, bitmap, &info));
    ASSERT_MSG(info.format == AndroidBitmapFormat::ANDROID_BITMAP_FORMAT_RGBA_8888,
               "Bitmap format was incorrect");
    ASSERT_MSG(info.width == resolution.width && info.height == resolution.height,
               "Bitmap resolution was incorrect");

    void* raw_data;
    BITMAP_CALL(lockPixels(env, bitmap, &raw_data));
    std::vector<u8> data(info.height * info.stride);
    libyuv::ABGRToARGB(reinterpret_cast<u8*>(raw_data), info.stride, data.data(), info.stride,
                       info.width, info.height);
    BITMAP_CALL(unlockPixels(env, bitmap));

    if (mirror) {
        std::vector<u8> mirrored(data.size());
        libyuv::ARGBMirror(data.data(), info.stride, mirrored.data(), info.stride, info.width,
                           info.height);
        data.swap(mirrored);
    }

    image.resize(info.height * info.width);
    if (format == Service::CAM::OutputFormat::RGB565) {
        libyuv::ARGBToRGB565(data.data(), info.stride, reinterpret_cast<u8*>(image.data()),
                             info.width * 2, info.width, invert ? -info.height : info.height);
    } else {
        libyuv::ARGBToYUY2(data.data(), info.stride, reinterpret_cast<u8*>(image.data()),
                           info.width * 2, info.width, invert ? -info.height : info.height);
    }
    opened = true;

#undef BITMAP_CALL
}

void Interface::SetResolution(const Service::CAM::Resolution& resolution_) {
    resolution = resolution_;
}

void Interface::SetFlip(Service::CAM::Flip flip) {
    mirror = base_mirror ^
             (flip == Service::CAM::Flip::Horizontal || flip == Service::CAM::Flip::Reverse);
    invert =
        base_invert ^ (flip == Service::CAM::Flip::Vertical || flip == Service::CAM::Flip::Reverse);
}

void Interface::SetFormat(Service::CAM::OutputFormat format_) {
    format = format_;
}

std::vector<u16> Interface::ReceiveFrame() {
    return image;
}

bool Interface::IsPreviewAvailable() {
    return opened;
}

SharedGlobalRef<jstring> Factory::last_path{};

std::unique_ptr<CameraInterface> Factory::Create(const std::string& config,
                                                 const Service::CAM::Flip& flip) {

    JNIEnv* env = IDCache::GetEnvForThread();
    if (last_path != nullptr) {
        return std::make_unique<Interface>(last_path, flip);
    }

    // Open file picker to get the string
    jstring path = reinterpret_cast<jstring>(
        env->CallStaticObjectMethod(s_still_image_camera_helper_class, s_open_file_picker));
    if (path == nullptr) {
        return std::make_unique<Camera::BlankCamera>();
    } else {
        auto shared_path = NewSharedGlobalRef(path);
        last_path = shared_path;
        return std::make_unique<Interface>(std::move(shared_path), flip);
    }
}

} // namespace Camera::StillImage
