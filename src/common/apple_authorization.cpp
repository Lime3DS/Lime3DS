// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <future>
#include <objc/message.h>
#include "common/apple_authorization.h"
#include "common/logging/log.h"

namespace AppleAuthorization {

// Bindings to Objective-C APIs

using NSString = void;
using AVMediaType = NSString*;
enum AVAuthorizationStatus : int {
    AVAuthorizationStatusNotDetermined = 0,
    AVAuthorizationStatusRestricted,
    AVAuthorizationStatusDenied,
    AVAuthorizationStatusAuthorized,
};

typedef NSString* (*send_stringWithUTF8String)(Class, SEL, const char*);
typedef AVAuthorizationStatus (*send_authorizationStatusForMediaType)(Class, SEL, AVMediaType);
typedef void (*send_requestAccessForMediaType_completionHandler)(Class, SEL, AVMediaType,
                                                                 void (^callback)(bool));

NSString* StringToNSString(const std::string_view string) {
    return reinterpret_cast<send_stringWithUTF8String>(objc_msgSend)(
        objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), string.data());
}

AVAuthorizationStatus GetAuthorizationStatus(AVMediaType media_type) {
    return reinterpret_cast<send_authorizationStatusForMediaType>(objc_msgSend)(
        objc_getClass("AVCaptureDevice"), sel_registerName("authorizationStatusForMediaType:"),
        media_type);
}

void RequestAccess(AVMediaType media_type, void (^callback)(bool)) {
    reinterpret_cast<send_requestAccessForMediaType_completionHandler>(objc_msgSend)(
        objc_getClass("AVCaptureDevice"),
        sel_registerName("requestAccessForMediaType:completionHandler:"), media_type, callback);
}

static AVMediaType AVMediaTypeAudio = StringToNSString("soun");
static AVMediaType AVMediaTypeVideo = StringToNSString("vide");

// Authorization Logic

bool CheckAuthorization(AVMediaType type, const std::string_view& type_name) {
    switch (GetAuthorizationStatus(type)) {
    case AVAuthorizationStatusNotDetermined: {
        LOG_INFO(Frontend, "Requesting {} permission.", type_name);
        __block std::promise<bool> authorization_promise;
        std::future<bool> authorization_future = authorization_promise.get_future();
        RequestAccess(type, ^(bool granted) {
          LOG_INFO(Frontend, "{} permission request result: {}", type_name, granted);
          authorization_promise.set_value(granted);
        });
        return authorization_future.get();
    }
    case AVAuthorizationStatusAuthorized:
        return true;
    case AVAuthorizationStatusDenied:
        LOG_WARNING(Frontend,
                    "{} permission has been denied and must be enabled via System Settings.",
                    type_name);
        return false;
    case AVAuthorizationStatusRestricted:
        LOG_WARNING(Frontend, "{} permission is restricted by the system.", type_name);
        return false;
    }
}

bool CheckAuthorizationForCamera() {
    return CheckAuthorization(AVMediaTypeVideo, "Camera");
}

bool CheckAuthorizationForMicrophone() {
    return CheckAuthorization(AVMediaTypeAudio, "Microphone");
}

} // namespace AppleAuthorization
