// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cctype>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/frontend/applets/swkbd.h"
#include "core/hle/service/cfg/cfg.h"

namespace Frontend {

ValidationError SoftwareKeyboard::ValidateFilters(const std::string& input) const {
    if (config.filters.prevent_digit) {
        if (std::count_if(input.begin(), input.end(),
                          [](unsigned char c) { return std::isdigit(c); }) > config.max_digits) {
            return ValidationError::MaxDigitsExceeded;
        }
    }
    if (config.filters.prevent_at) {
        if (input.find('@') != std::string::npos) {
            return ValidationError::AtSignNotAllowed;
        }
    }
    if (config.filters.prevent_percent) {
        if (input.find('%') != std::string::npos) {
            return ValidationError::PercentNotAllowed;
        }
    }
    if (config.filters.prevent_backslash) {
        if (input.find('\\') != std::string::npos) {
            return ValidationError::BackslashNotAllowed;
        }
    }
    if (config.filters.prevent_profanity) {
        // TODO: check the profanity filter
        LOG_INFO(Frontend, "App requested swkbd profanity filter, but its not implemented.");
    }
    return ValidationError::None;
}

ValidationError SoftwareKeyboard::ValidateInput(const std::string& input) const {
    ValidationError error;
    if ((error = ValidateFilters(input)) != ValidationError::None) {
        return error;
    }

    // 3DS uses UTF-16 string to test string size
    std::u16string u16input = Common::UTF8ToUTF16(input);

    if (u16input.size() > config.max_text_length) {
        return ValidationError::MaxLengthExceeded;
    }

    bool is_blank =
        std::all_of(input.begin(), input.end(), [](unsigned char c) { return std::isspace(c); });
    bool is_empty = input.empty();
    switch (config.accept_mode) {
    case AcceptedInput::FixedLength:
        if (u16input.size() != config.max_text_length) {
            return ValidationError::FixedLengthRequired;
        }
        break;
    case AcceptedInput::NotEmptyAndNotBlank:
        if (is_blank) {
            return ValidationError::BlankInputNotAllowed;
        }
        if (is_empty) {
            return ValidationError::EmptyInputNotAllowed;
        }
        break;
    case AcceptedInput::NotBlank:
        if (is_blank) {
            return ValidationError::BlankInputNotAllowed;
        }
        break;
    case AcceptedInput::NotEmpty:
        if (is_empty) {
            return ValidationError::EmptyInputNotAllowed;
        }
        break;
    case AcceptedInput::Anything:
        return ValidationError::None;
    default:
        // TODO(jroweboy): What does hardware do in this case?
        LOG_CRITICAL(Frontend, "Application requested unknown validation method. Method: {}",
                     config.accept_mode);
        UNREACHABLE();
    }

    return ValidationError::None;
}

ValidationError SoftwareKeyboard::ValidateButton(u8 button) const {
    switch (config.button_config) {
    case ButtonConfig::None:
        return ValidationError::None;
    case ButtonConfig::Single:
        if (button != 0) {
            return ValidationError::ButtonOutOfRange;
        }
        break;
    case ButtonConfig::Dual:
        if (button > 1) {
            return ValidationError::ButtonOutOfRange;
        }
        break;
    case ButtonConfig::Triple:
        if (button > 2) {
            return ValidationError::ButtonOutOfRange;
        }
        break;
    default:
        UNREACHABLE();
    }
    return ValidationError::None;
}

ValidationError SoftwareKeyboard::Finalize(const std::string& text, u8 button) {
    // Skip check when OK is not pressed
    if (button == static_cast<u8>(config.button_config)) {
        ValidationError error;
        if ((error = ValidateInput(text)) != ValidationError::None) {
            return error;
        }
        if ((error = ValidateButton(button)) != ValidationError::None) {
            return error;
        }
    }
    data = {text, button};
    data_ready = true;
    return ValidationError::None;
}

bool SoftwareKeyboard::DataReady() const {
    return data_ready;
}

const KeyboardData& SoftwareKeyboard::ReceiveData() {
    data_ready = false;
    return data;
}

DefaultKeyboard::DefaultKeyboard(Core::System& system_) : system(system_) {}

void DefaultKeyboard::Execute(const Frontend::KeyboardConfig& config_) {
    SoftwareKeyboard::Execute(config_);

    auto cfg = Service::CFG::GetModule(system);
    std::string username = Common::UTF16ToUTF8(cfg->GetUsername());
    switch (this->config.button_config) {
    case ButtonConfig::None:
    case ButtonConfig::Single:
        Finalize(username, 0);
        break;
    case ButtonConfig::Dual:
        Finalize(username, 1);
        break;
    case ButtonConfig::Triple:
        Finalize(username, 2);
        break;
    default:
        UNREACHABLE();
    }
}

void DefaultKeyboard::ShowError(const std::string& error) {
    LOG_ERROR(Applet_SWKBD, "Default keyboard text is unaccepted! error: {}", error);
}

} // namespace Frontend
