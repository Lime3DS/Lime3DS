// Copyright 2020 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/icl/discrete_interval.hpp>
#include "common/common_types.h"
#include "common/logging/log.h"

namespace boost::serialization {

template <class Archive, class DomainT, ICL_COMPARE Compare>
void save(Archive& ar, const boost::icl::discrete_interval<DomainT, Compare>& obj,
          const unsigned int file_version) {
    ar << obj.lower();
    ar << obj.upper();
    ar << obj.bounds()._bits;
}

template <class Archive, class DomainT, ICL_COMPARE Compare>
void load(Archive& ar, boost::icl::discrete_interval<DomainT, Compare>& obj,
          const unsigned int file_version) {
    DomainT upper, lower;
    boost::icl::bound_type bounds;
    ar >> lower;
    ar >> upper;
    ar >> bounds;
    obj = boost::icl::discrete_interval(lower, upper, boost::icl::interval_bounds(bounds));
}

template <class Archive, class DomainT, ICL_COMPARE Compare>
void serialize(Archive& ar, boost::icl::discrete_interval<DomainT, Compare>& obj,
               const unsigned int file_version) {
    boost::serialization::split_free(ar, obj, file_version);
}

} // namespace boost::serialization
