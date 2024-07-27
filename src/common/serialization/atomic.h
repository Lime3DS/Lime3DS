// Copyright 2020 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <boost/serialization/split_free.hpp>

namespace boost::serialization {

template <class Archive, class T>
void serialize(Archive& ar, std::atomic<T>& value, const unsigned int file_version) {
    boost::serialization::split_free(ar, value, file_version);
}

template <class Archive, class T>
void save(Archive& ar, const std::atomic<T>& value, const unsigned int file_version) {
    ar << value.load();
}

template <class Archive, class T>
void load(Archive& ar, std::atomic<T>& value, const unsigned int file_version) {
    T tmp;
    ar >> tmp;
    value.store(tmp);
}

} // namespace boost::serialization
