// Copyright 2020 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/serialization/serialization.hpp>

/// Allows classes to define `save_construct` and `load_construct` methods for serialization
/// This is used where we don't call the default constructor during deserialization, as a shortcut
/// instead of using load_construct_data directly
class construct_access {
public:
    template <class Archive, class T>
    static void save_construct(Archive& ar, const T* t, const unsigned int file_version) {
        t->save_construct(ar, file_version);
    }
    template <class Archive, class T>
    static void load_construct(Archive& ar, T* t, const unsigned int file_version) {
        T::load_construct(ar, t, file_version);
    }
};

#define BOOST_SERIALIZATION_CONSTRUCT(T)                                                           \
    namespace boost::serialization {                                                               \
    template <class Archive>                                                                       \
    void save_construct_data(Archive& ar, const T* t, const unsigned int file_version) {           \
        construct_access::save_construct(ar, t, file_version);                                     \
    }                                                                                              \
    template <class Archive>                                                                       \
    void load_construct_data(Archive& ar, T* t, const unsigned int file_version) {                 \
        construct_access::load_construct(ar, t, file_version);                                     \
    }                                                                                              \
    }
