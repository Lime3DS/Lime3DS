// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "tuple"

#include "common/common_types.h"
#include "core/file_sys/secure_value_backend.h"
#include "core/hle/result.h"
#include "core/hle/service/fs/archive.h"
#include "network/artic_base/artic_base_client.h"

namespace FileSys {
class ArticSecureValueBackend : public SecureValueBackend {
public:
    ArticSecureValueBackend(const std::shared_ptr<Network::ArticBase::Client>& _client)
        : client(_client) {}

    Result ObsoletedSetSaveDataSecureValue(u32 unique_id, u8 title_variation, u32 secure_value_slot,
                                           u64 secure_value) override;

    ResultVal<std::tuple<bool, u64>> ObsoletedGetSaveDataSecureValue(
        u32 unique_id, u8 title_variation, u32 secure_value_slot) override;

    Result ControlSecureSave(u32 action, u8* input, size_t input_size, u8* output,
                             size_t output_size) override;

    Result SetThisSaveDataSecureValue(u32 secure_value_slot, u64 secure_value) override;

    ResultVal<std::tuple<bool, bool, u64>> GetThisSaveDataSecureValue(
        u32 secure_value_slot) override;

    bool BackendIsSlow() override {
        return true;
    }

protected:
    ArticSecureValueBackend() = default;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<SecureValueBackend>(*this);
    }
    friend class boost::serialization::access;

private:
    std::shared_ptr<Network::ArticBase::Client> client;
};
} // namespace FileSys

BOOST_CLASS_EXPORT_KEY(FileSys::ArticSecureValueBackend)