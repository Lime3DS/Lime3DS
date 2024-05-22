// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "tuple"

#include "common/common_types.h"
#include "core/hle/result.h"
#include "core/hle/service/fs/archive.h"

namespace FileSys {
class SecureValueBackend : NonCopyable {
public:
    virtual ~SecureValueBackend() {};

    virtual Result ObsoletedSetSaveDataSecureValue(u32 unique_id, u8 title_variation,
                                                   u32 secure_value_slot, u64 secure_value) = 0;

    virtual ResultVal<std::tuple<bool, u64>> ObsoletedGetSaveDataSecureValue(
        u32 unique_id, u8 title_variation, u32 secure_value_slot) = 0;

    virtual Result ControlSecureSave(u32 action, u8* input, size_t input_size, u8* output,
                                     size_t output_size) = 0;

    virtual Result SetThisSaveDataSecureValue(u32 secure_value_slot, u64 secure_value) = 0;

    virtual ResultVal<std::tuple<bool, bool, u64>> GetThisSaveDataSecureValue(
        u32 secure_value_slot) = 0;

    virtual bool BackendIsSlow() {
        return false;
    }

protected:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {}
    friend class boost::serialization::access;
};

class DefaultSecureValueBackend : public SecureValueBackend {
public:
    Result ObsoletedSetSaveDataSecureValue(u32 unique_id, u8 title_variation, u32 secure_value_slot,
                                           u64 secure_value) override;

    ResultVal<std::tuple<bool, u64>> ObsoletedGetSaveDataSecureValue(
        u32 unique_id, u8 title_variation, u32 secure_value_slot) override;

    Result ControlSecureSave(u32 action, u8* input, size_t input_size, u8* output,
                             size_t output_size) override;

    Result SetThisSaveDataSecureValue(u32 secure_value_slot, u64 secure_value) override;

    ResultVal<std::tuple<bool, bool, u64>> GetThisSaveDataSecureValue(
        u32 secure_value_slot) override;

protected:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};
} // namespace FileSys

BOOST_CLASS_EXPORT_KEY(FileSys::DefaultSecureValueBackend)