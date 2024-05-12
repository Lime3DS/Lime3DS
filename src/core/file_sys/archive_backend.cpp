// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#include <iomanip>
#include <sstream>
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/file_sys/archive_backend.h"
#include "core/memory.h"

namespace FileSys {

Path::Path(LowPathType type, std::vector<u8> data) : type(type) {
    switch (type) {
    case LowPathType::Binary: {
        binary = std::move(data);
        break;
    }

    case LowPathType::Char: {
        string.resize(data.size() - 1); // Data is always null-terminated.
        std::memcpy(string.data(), data.data(), string.size());
        break;
    }

    case LowPathType::Wchar: {
        u16str.resize(data.size() / 2 - 1); // Data is always null-terminated.
        std::memcpy(u16str.data(), data.data(), u16str.size() * sizeof(char16_t));
        break;
    }

    default:
        break;
    }
}

std::string Path::DebugStr() const {
    switch (GetType()) {
    case LowPathType::Invalid:
    default:
        return "[Invalid]";
    case LowPathType::Empty:
        return "[Empty]";
    case LowPathType::Binary: {
        std::stringstream res;
        res << "[Binary: ";
        for (unsigned byte : binary)
            res << std::hex << std::setw(2) << std::setfill('0') << byte;
        res << ']';
        return res.str();
    }
    case LowPathType::Char:
        return "[Char: " + AsString() + ']';
    case LowPathType::Wchar:
        return "[Wchar: " + AsString() + ']';
    }
}

std::string Path::AsString() const {
    switch (GetType()) {
    case LowPathType::Char:
        return string;
    case LowPathType::Wchar:
        return Common::UTF16ToUTF8(u16str);
    case LowPathType::Empty:
        return {};
    case LowPathType::Invalid:
    case LowPathType::Binary:
    default:
        // TODO(yuriks): Add assert
        LOG_ERROR(Service_FS, "LowPathType cannot be converted to string!");
        return {};
    }
}

std::u16string Path::AsU16Str() const {
    switch (GetType()) {
    case LowPathType::Char:
        return Common::UTF8ToUTF16(string);
    case LowPathType::Wchar:
        return u16str;
    case LowPathType::Empty:
        return {};
    case LowPathType::Invalid:
    case LowPathType::Binary:
    default:
        // TODO(yuriks): Add assert
        LOG_ERROR(Service_FS, "LowPathType cannot be converted to u16string!");
        return {};
    }

    UNREACHABLE();
}

std::vector<u8> Path::AsBinary() const {
    switch (GetType()) {
    case LowPathType::Binary:
        return binary;
    case LowPathType::Char:
        return std::vector<u8>(string.begin(), string.end());
    case LowPathType::Wchar: {
        // use two u8 for each character of u16str
        std::vector<u8> to_return(u16str.size() * 2);
        for (std::size_t i = 0; i < u16str.size(); ++i) {
            u16 tmp_char = u16str.at(i);
            *reinterpret_cast<u16*>(to_return.data() + i * 2) = tmp_char;
        }
        return to_return;
    }
    case LowPathType::Empty:
        return {};
    case LowPathType::Invalid:
    default:
        // TODO(yuriks): Add assert
        LOG_ERROR(Service_FS, "LowPathType cannot be converted to binary!");
        return {};
    }
}
} // namespace FileSys
