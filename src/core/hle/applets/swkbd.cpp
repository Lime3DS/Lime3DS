// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <string>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/applets/swkbd.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"
#include "core/hle/service/gsp/gsp.h"
#include "core/hle/service/hid/hid.h"
#include "core/memory.h"

namespace HLE::Applets {

Result SoftwareKeyboard::ReceiveParameterImpl(Service::APT::MessageParameter const& parameter) {
    switch (parameter.signal) {
    case Service::APT::SignalType::Request: {
        // The LibAppJustStarted message contains a buffer with the size of the framebuffer shared
        // memory.
        // Create the SharedMemory that will hold the framebuffer data
        Service::APT::CaptureBufferInfo capture_info;
        ASSERT(sizeof(capture_info) == parameter.buffer.size());

        std::memcpy(&capture_info, parameter.buffer.data(), sizeof(capture_info));

        using Kernel::MemoryPermission;
        // Create a SharedMemory that directly points to this heap block.
        framebuffer_memory = system.Kernel().CreateSharedMemoryForApplet(
            0, capture_info.size, MemoryPermission::ReadWrite, MemoryPermission::ReadWrite,
            "SoftwareKeyboard Memory");

        // Send the response message with the newly created SharedMemory
        SendParameter({
            .sender_id = id,
            .destination_id = parent,
            .signal = Service::APT::SignalType::Response,
            .object = framebuffer_memory,
        });

        return ResultSuccess;
    }

    case Service::APT::SignalType::Message: {
        // Callback result
        ASSERT_MSG(parameter.buffer.size() == sizeof(config),
                   "The size of the parameter (SoftwareKeyboardConfig) is wrong");

        std::memcpy(&config, parameter.buffer.data(), parameter.buffer.size());

        switch (config.callback_result) {
        case SoftwareKeyboardCallbackResult::OK:
            // Finish execution
            Finalize();
            return ResultSuccess;

        case SoftwareKeyboardCallbackResult::Close:
            // Let the frontend display error and quit
            frontend_applet->ShowError(Common::UTF16BufferToUTF8(config.callback_msg));
            config.return_code = SoftwareKeyboardResult::BannedInput;
            config.text_offset = config.text_length = 0;
            Finalize();
            return ResultSuccess;

        case SoftwareKeyboardCallbackResult::Continue:
            // Let the frontend display error and get input again
            // The input will be sent for validation again on next Update().
            frontend_applet->ShowError(Common::UTF16BufferToUTF8(config.callback_msg));
            frontend_applet->Execute(ToFrontendConfig(config));
            return ResultSuccess;

        default:
            UNREACHABLE();
        }
    }

    default: {
        LOG_ERROR(Service_APT, "unsupported signal {}", parameter.signal);
        UNIMPLEMENTED();
        // TODO(Subv): Find the right error code
        return ResultUnknown;
    }
    }
}

Result SoftwareKeyboard::Start(Service::APT::MessageParameter const& parameter) {
    ASSERT_MSG(parameter.buffer.size() == sizeof(config),
               "The size of the parameter (SoftwareKeyboardConfig) is wrong");

    std::memcpy(&config, parameter.buffer.data(), parameter.buffer.size());
    text_memory = std::static_pointer_cast<Kernel::SharedMemory, Kernel::Object>(parameter.object);

    DrawScreenKeyboard();

    using namespace Frontend;
    frontend_applet = system.GetSoftwareKeyboard();
    ASSERT(frontend_applet);

    frontend_applet->Execute(ToFrontendConfig(config));

    return ResultSuccess;
}

void SoftwareKeyboard::Update() {
    if (!frontend_applet->DataReady())
        return;

    using namespace Frontend;
    const KeyboardData& data = frontend_applet->ReceiveData();
    std::u16string text = Common::UTF8ToUTF16(data.text);
    // Include a null terminator
    std::memcpy(text_memory->GetPointer(), text.c_str(), (text.length() + 1) * sizeof(char16_t));
    switch (config.num_buttons_m1) {
    case SoftwareKeyboardButtonConfig::SingleButton:
        config.return_code = SoftwareKeyboardResult::D0Click;
        break;
    case SoftwareKeyboardButtonConfig::DualButton:
        if (data.button == 0)
            config.return_code = SoftwareKeyboardResult::D1Click0;
        else
            config.return_code = SoftwareKeyboardResult::D1Click1;
        break;
    case SoftwareKeyboardButtonConfig::TripleButton:
        if (data.button == 0)
            config.return_code = SoftwareKeyboardResult::D2Click0;
        else if (data.button == 1)
            config.return_code = SoftwareKeyboardResult::D2Click1;
        else
            config.return_code = SoftwareKeyboardResult::D2Click2;
        break;
    case SoftwareKeyboardButtonConfig::NoButton:
        // TODO: find out what is actually returned
        config.return_code = SoftwareKeyboardResult::None;
        break;
    default:
        LOG_CRITICAL(Applet_SWKBD, "Unknown button config {}", config.num_buttons_m1);
        UNREACHABLE();
    }

    config.text_length = static_cast<u16>(text.size());
    config.text_offset = 0;

    if (config.filter_flags & HLE::Applets::SoftwareKeyboardFilter::Callback) {
        std::vector<u8> buffer(sizeof(SoftwareKeyboardConfig));
        std::memcpy(buffer.data(), &config, buffer.size());

        // Send the message to invoke callback
        SendParameter({
            .sender_id = id,
            .destination_id = parent,
            .signal = Service::APT::SignalType::Message,
            .buffer = buffer,
        });
    } else {
        Finalize();
    }
}

void SoftwareKeyboard::DrawScreenKeyboard() {
    // TODO(Subv): Draw the HLE keyboard, for now just do nothing
}

Result SoftwareKeyboard::Finalize() {
    std::vector<u8> buffer(sizeof(SoftwareKeyboardConfig));
    std::memcpy(buffer.data(), &config, buffer.size());
    CloseApplet(nullptr, buffer);
    text_memory = nullptr;
    return ResultSuccess;
}

Frontend::KeyboardConfig SoftwareKeyboard::ToFrontendConfig(
    const SoftwareKeyboardConfig& config) const {
    using namespace Frontend;
    KeyboardConfig frontend_config;
    frontend_config.button_config =
        static_cast<ButtonConfig>(static_cast<u32>(config.num_buttons_m1));
    frontend_config.accept_mode = static_cast<AcceptedInput>(static_cast<u32>(config.valid_input));
    frontend_config.multiline_mode = config.multiline;
    frontend_config.max_text_length = config.max_text_length;
    frontend_config.max_digits = config.max_digits;
    frontend_config.hint_text = Common::UTF16BufferToUTF8(config.hint_text);
    for (const auto& text : config.button_text) {
        frontend_config.button_text.push_back(Common::UTF16BufferToUTF8(text));
    }
    frontend_config.filters.prevent_digit =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::Digits);
    frontend_config.filters.prevent_at =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::At);
    frontend_config.filters.prevent_percent =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::Percent);
    frontend_config.filters.prevent_backslash =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::Backslash);
    frontend_config.filters.prevent_profanity =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::Profanity);
    frontend_config.filters.enable_callback =
        static_cast<bool>(config.filter_flags & SoftwareKeyboardFilter::Callback);
    return frontend_config;
}
} // namespace HLE::Applets
