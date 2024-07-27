// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>

#include <boost/locale/encoding_utf.hpp>

#include "common/common_paths.h"
#include "common/logging/log.h"
#include "common/string_util.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace Common {

/// Make a char lowercase
char ToLower(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

/// Make a char uppercase
char ToUpper(char c) {
    return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

/// Make a string lowercase
std::string ToLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return str;
}

/// Make a string uppercase
std::string ToUpper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return str;
}

// Turns "  hej " into "hej". Also handles tabs.
std::string StripSpaces(const std::string& str) {
    const std::size_t s = str.find_first_not_of(" \t\r\n");

    if (str.npos != s)
        return str.substr(s, str.find_last_not_of(" \t\r\n") - s + 1);
    else
        return "";
}

// "\"hello\"" is turned to "hello"
// This one assumes that the string has already been space stripped in both
// ends, as done by StripSpaces above, for example.
std::string StripQuotes(const std::string& s) {
    if (s.size() && '\"' == s[0] && '\"' == *s.rbegin())
        return s.substr(1, s.size() - 2);
    else
        return s;
}

std::string StringFromBool(bool value) {
    return value ? "True" : "False";
}

bool SplitPath(const std::string& full_path, std::string* _pPath, std::string* _pFilename,
               std::string* _pExtension) {
    if (full_path.empty())
        return false;

    std::size_t dir_end = full_path.find_last_of("/"
// windows needs the : included for something like just "C:" to be considered a directory
#ifdef _WIN32
                                                 ":"
#endif
    );
    if (std::string::npos == dir_end)
        dir_end = 0;
    else
        dir_end += 1;

    std::size_t fname_end = full_path.rfind('.');
    if (fname_end < dir_end || std::string::npos == fname_end)
        fname_end = full_path.size();

    if (_pPath)
        *_pPath = full_path.substr(0, dir_end);

    if (_pFilename)
        *_pFilename = full_path.substr(dir_end, fname_end - dir_end);

    if (_pExtension)
        *_pExtension = full_path.substr(fname_end);

    return true;
}

void BuildCompleteFilename(std::string& _CompleteFilename, const std::string& _Path,
                           const std::string& _Filename) {
    _CompleteFilename = _Path;

    // check for seperator
    if (DIR_SEP_CHR != *_CompleteFilename.rbegin())
        _CompleteFilename += DIR_SEP_CHR;

    // add the filename
    _CompleteFilename += _Filename;
}

std::vector<std::string> SplitString(const std::string& str, const char delim) {
    std::istringstream iss(str);
    std::vector<std::string> output(1);

    while (std::getline(iss, *output.rbegin(), delim)) {
        output.emplace_back();
    }

    output.pop_back();
    return output;
}

std::string TabsToSpaces(int tab_size, std::string in) {
    std::size_t i = 0;

    while ((i = in.find('\t')) != std::string::npos) {
        in.replace(i, 1, tab_size, ' ');
    }

    return in;
}

bool EndsWith(const std::string& value, const std::string& ending) {
    if (ending.size() > value.size())
        return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

std::string ReplaceAll(std::string result, const std::string& src, const std::string& dest) {
    std::size_t pos = 0;

    if (src == dest)
        return result;

    while ((pos = result.find(src, pos)) != std::string::npos) {
        result.replace(pos, src.size(), dest);
        pos += dest.length();
    }

    return result;
}

std::string UTF16ToUTF8(std::u16string_view input) {
    return boost::locale::conv::utf_to_utf<char>(input.data(), input.data() + input.size());
}

std::u16string UTF8ToUTF16(std::string_view input) {
    return boost::locale::conv::utf_to_utf<char16_t>(input.data(), input.data() + input.size());
}

#ifdef _WIN32
static std::wstring CPToUTF16(u32 code_page, const std::string& input) {
    const auto size =
        MultiByteToWideChar(code_page, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);

    if (size == 0) {
        return {};
    }

    std::wstring output(size, L'\0');

    if (size != MultiByteToWideChar(code_page, 0, input.data(), static_cast<int>(input.size()),
                                    &output[0], static_cast<int>(output.size()))) {
        output.clear();
    }

    return output;
}

std::string UTF16ToUTF8(const std::wstring& input) {
    const auto size = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
                                          nullptr, 0, nullptr, nullptr);
    if (size == 0) {
        return "";
    }

    std::string output(size, '\0');

    if (size != WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
                                    &output[0], static_cast<int>(output.size()), nullptr,
                                    nullptr)) {
        output.clear();
    }

    return output;
}

std::wstring UTF8ToUTF16W(const std::string& input) {
    return CPToUTF16(CP_UTF8, input);
}

#endif

std::string StringFromFixedZeroTerminatedBuffer(const char* buffer, std::size_t max_len) {
    std::size_t len = 0;
    while (len < max_len && buffer[len] != '\0')
        ++len;

    return std::string(buffer, len);
}
} // namespace Common
