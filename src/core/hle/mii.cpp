// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/crc.hpp>
#include <boost/serialization/binary_object.hpp>
#include "common/archives.h"
#include "core/hle/mii.h"

SERIALIZE_EXPORT_IMPL(Mii::MiiData)
SERIALIZE_EXPORT_IMPL(Mii::ChecksummedMiiData)

namespace Mii {
template <class Archive>
void MiiData::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::make_binary_object(this, sizeof(MiiData));
}
SERIALIZE_IMPL(MiiData)

template <class Archive>
void ChecksummedMiiData::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::make_binary_object(this, sizeof(ChecksummedMiiData));
}
SERIALIZE_IMPL(ChecksummedMiiData)

u16 ChecksummedMiiData::CalculateChecksum() {
    // Calculate the checksum of the selected Mii, see https://www.3dbrew.org/wiki/Mii#Checksum
    return boost::crc<16, 0x1021, 0, 0, false, false>(this, offsetof(ChecksummedMiiData, crc16));
}
} // namespace Mii
