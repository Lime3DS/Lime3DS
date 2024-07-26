// Copyright 2020 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/export.hpp>

using iarchive = boost::archive::binary_iarchive;
using oarchive = boost::archive::binary_oarchive;

#define SERIALIZE_IMPL(A)                                                                          \
    template void A::serialize<iarchive>(iarchive & ar, const unsigned int file_version);          \
    template void A::serialize<oarchive>(oarchive & ar, const unsigned int file_version);

#define SERIALIZE_EXPORT_IMPL(A)                                                                   \
    BOOST_CLASS_EXPORT_IMPLEMENT(A)                                                                \
    BOOST_SERIALIZATION_REGISTER_ARCHIVE(iarchive)                                                 \
    BOOST_SERIALIZATION_REGISTER_ARCHIVE(oarchive)
