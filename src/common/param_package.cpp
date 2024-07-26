// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <stdexcept>
#include <utility>
#include <vector>
#include "common/logging/log.h"
#include "common/param_package.h"
#include "common/string_util.h"

namespace Common {

constexpr char KEY_VALUE_SEPARATOR = ':';
constexpr char PARAM_SEPARATOR = ',';

constexpr char ESCAPE_CHARACTER = '$';
constexpr char KEY_VALUE_SEPARATOR_ESCAPE[] = "$0";
constexpr char PARAM_SEPARATOR_ESCAPE[] = "$1";
constexpr char ESCAPE_CHARACTER_ESCAPE[] = "$2";

/// A placeholder for empty param packages to avoid empty strings
/// (they may be recognized as "not set" by some frontend libraries like qt)
constexpr char EMPTY_PLACEHOLDER[] = "[empty]";

ParamPackage::ParamPackage(const std::string& serialized) {
    if (serialized == EMPTY_PLACEHOLDER) {
        return;
    }

    const auto pairs = Common::SplitString(serialized, PARAM_SEPARATOR);
    for (const std::string& pair : pairs) {
        auto key_value = Common::SplitString(pair, KEY_VALUE_SEPARATOR);
        if (key_value.size() != 2) {
            LOG_ERROR(Common, "invalid key pair {}", pair);
            continue;
        }

        for (std::string& part : key_value) {
            part = Common::ReplaceAll(part, KEY_VALUE_SEPARATOR_ESCAPE, {KEY_VALUE_SEPARATOR});
            part = Common::ReplaceAll(part, PARAM_SEPARATOR_ESCAPE, {PARAM_SEPARATOR});
            part = Common::ReplaceAll(part, ESCAPE_CHARACTER_ESCAPE, {ESCAPE_CHARACTER});
        }

        Set(key_value[0], std::move(key_value[1]));
    }
}

ParamPackage::ParamPackage(std::initializer_list<DataType::value_type> list) : data(list) {}

std::string ParamPackage::Serialize() const {
    if (data.empty())
        return EMPTY_PLACEHOLDER;

    std::string result;

    for (const auto& pair : data) {
        std::array<std::string, 2> key_value{{pair.first, pair.second}};
        for (std::string& part : key_value) {
            part = Common::ReplaceAll(part, {ESCAPE_CHARACTER}, ESCAPE_CHARACTER_ESCAPE);
            part = Common::ReplaceAll(part, {PARAM_SEPARATOR}, PARAM_SEPARATOR_ESCAPE);
            part = Common::ReplaceAll(part, {KEY_VALUE_SEPARATOR}, KEY_VALUE_SEPARATOR_ESCAPE);
        }
        result += key_value[0] + KEY_VALUE_SEPARATOR + key_value[1] + PARAM_SEPARATOR;
    }

    result.pop_back(); // discard the trailing PARAM_SEPARATOR
    return result;
}

std::string ParamPackage::Get(const std::string& key, const std::string& default_value) const {
    auto pair = data.find(key);
    if (pair == data.end()) {
        LOG_DEBUG(Common, "key {} not found", key);
        return default_value;
    }

    return pair->second;
}

int ParamPackage::Get(const std::string& key, int default_value) const {
    auto pair = data.find(key);
    if (pair == data.end()) {
        LOG_DEBUG(Common, "key {} not found", key);
        return default_value;
    }

    try {
        return std::stoi(pair->second);
    } catch (const std::logic_error&) {
        LOG_ERROR(Common, "failed to convert {} to int", pair->second);
        return default_value;
    }
}

float ParamPackage::Get(const std::string& key, float default_value) const {
    auto pair = data.find(key);
    if (pair == data.end()) {
        LOG_DEBUG(Common, "key {} not found", key);
        return default_value;
    }

    try {
        return std::stof(pair->second);
    } catch (const std::logic_error&) {
        LOG_ERROR(Common, "failed to convert {} to float", pair->second);
        return default_value;
    }
}

void ParamPackage::Set(const std::string& key, std::string value) {
    data.insert_or_assign(key, std::move(value));
}

void ParamPackage::Set(const std::string& key, int value) {
    data.insert_or_assign(key, std::to_string(value));
}

void ParamPackage::Set(const std::string& key, float value) {
    data.insert_or_assign(key, std::to_string(value));
}

bool ParamPackage::Has(const std::string& key) const {
    return data.find(key) != data.end();
}

void ParamPackage::Erase(const std::string& key) {
    data.erase(key);
}

void ParamPackage::Clear() {
    data.clear();
}

ParamPackage::DataType::iterator ParamPackage::begin() {
    return data.begin();
}

ParamPackage::DataType::const_iterator ParamPackage::begin() const {
    return data.begin();
}

ParamPackage::DataType::iterator ParamPackage::end() {
    return data.end();
}

ParamPackage::DataType::const_iterator ParamPackage::end() const {
    return data.end();
}

} // namespace Common
