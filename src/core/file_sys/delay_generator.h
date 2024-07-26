// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include "common/common_types.h"

#define SERIALIZE_DELAY_GENERATOR                                                                  \
private:                                                                                           \
    template <class Archive>                                                                       \
    void serialize(Archive& ar, const unsigned int) {                                              \
        ar& boost::serialization::base_object<DelayGenerator>(*this);                              \
    }                                                                                              \
    friend class boost::serialization::access;

namespace FileSys {

class DelayGenerator {
public:
    virtual ~DelayGenerator();
    virtual u64 GetReadDelayNs(std::size_t length) = 0;
    virtual u64 GetOpenDelayNs() = 0;

    // TODO (B3N30): Add getter for all other file/directory io operations
private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {}
    friend class boost::serialization::access;
};

class DefaultDelayGenerator : public DelayGenerator {
public:
    u64 GetReadDelayNs(std::size_t length) override;
    u64 GetOpenDelayNs() override;

    SERIALIZE_DELAY_GENERATOR
};

} // namespace FileSys

BOOST_CLASS_EXPORT_KEY(FileSys::DefaultDelayGenerator);
