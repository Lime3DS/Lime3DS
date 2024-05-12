// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "secure_value_backend.h"

SERIALIZE_EXPORT_IMPL(FileSys::DefaultSecureValueBackend)

namespace FileSys {

Result DefaultSecureValueBackend::ObsoletedSetSaveDataSecureValue(u32 unique_id, u8 title_variation,
                                                                  u32 secure_value_slot,
                                                                  u64 secure_value) {

    // TODO: Generate and Save the Secure Value

    LOG_WARNING(Service_FS,
                "(STUBBED) called, value=0x{:016x} secure_value_slot=0x{:08X} "
                "unqiue_id=0x{:08X} title_variation=0x{:02X}",
                secure_value, secure_value_slot, unique_id, title_variation);

    return ResultSuccess;
}

ResultVal<std::tuple<bool, u64>> DefaultSecureValueBackend::ObsoletedGetSaveDataSecureValue(
    u32 unique_id, u8 title_variation, u32 secure_value_slot) {

    // TODO: Implement Secure Value Lookup & Generation

    LOG_WARNING(Service_FS,
                "(STUBBED) called, secure_value_slot=0x{:08X} "
                "unqiue_id=0x{:08X} title_variation=0x{:02X}",
                secure_value_slot, unique_id, title_variation);

    return std::make_tuple<bool, u64>(false, 0);
}

Result DefaultSecureValueBackend::ControlSecureSave(u32 action, u8* input, size_t input_size,
                                                    u8* output, size_t output_size) {

    LOG_WARNING(Service_FS,
                "(STUBBED) called, action=0x{:08X} "
                "input_size=0x{:016X} output_size=0x{:016X}",
                action, input_size, output_size);

    return ResultSuccess;
}

Result DefaultSecureValueBackend::SetThisSaveDataSecureValue(u32 secure_value_slot,
                                                             u64 secure_value) {
    // TODO: Generate and Save the Secure Value

    LOG_WARNING(Service_FS, "(STUBBED) called, secure_value=0x{:016x} secure_value_slot=0x{:08X}",
                secure_value, secure_value_slot);

    return ResultSuccess;
}

ResultVal<std::tuple<bool, bool, u64>> DefaultSecureValueBackend::GetThisSaveDataSecureValue(
    u32 secure_value_slot) {

    // TODO: Implement Secure Value Lookup & Generation

    LOG_WARNING(Service_FS, "(STUBBED) called secure_value_slot=0x{:08X}", secure_value_slot);

    return std::make_tuple<bool, bool, u64>(false, true, 0);
}

template <class Archive>
void FileSys::DefaultSecureValueBackend::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<SecureValueBackend>(*this);
}
} // namespace FileSys