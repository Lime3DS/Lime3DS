// Copyright 2022 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <chrono>
#include <boost/crc.hpp>
#include <cryptopp/osrng.h>

#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/service/nfc/amiibo_crypto.h"
#include "core/hle/service/nfc/nfc_device.h"
#include "core/hw/aes/key.h"
#include "core/loader/loader.h"

SERVICE_CONSTRUCT_IMPL(Service::NFC::NfcDevice)

namespace Service::NFC {
template <class Archive>
void NfcDevice::serialize(Archive& ar, const unsigned int) {
    ar & tag_in_range_event;
    ar & tag_out_of_range_event;
    ar & is_initalized;
    ar & is_data_moddified;
    ar & is_app_area_open;
    ar & is_plain_amiibo;
    ar & is_write_protected;
    ar & is_tag_in_range;
    ar & allowed_protocols;
    ar & device_state;
    ar & connection_state;
    ar & communication_state;
    ar & amiibo_filename;
    ar & tag;
    ar & encrypted_tag;
}
SERIALIZE_IMPL(NfcDevice)

NfcDevice::NfcDevice(Core::System& system_) : system{system_} {
    tag_in_range_event =
        system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "NFC::tag_in_range_event");
    tag_out_of_range_event =
        system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "NFC::tag_out_range_event");

    remove_amiibo_event = system.CoreTiming().RegisterEvent(
        "remove amiibo event",
        [this](std::uintptr_t user_data, s64 cycles_late) { UnloadAmiibo(); });
}

NfcDevice::~NfcDevice() = default;

bool NfcDevice::LoadAmiibo(std::string filename) {
    FileUtil::IOFile amiibo_file(filename, "rb");

    if (!is_initalized) {
        LOG_ERROR(Service_NFC, "Not initialized");
        return false;
    }

    if (device_state != DeviceState::SearchingForTag) {
        LOG_ERROR(Service_NFC, "Game is not looking for amiibos, current state {}", device_state);
        return false;
    }

    if (!amiibo_file.IsOpen()) {
        LOG_ERROR(Service_NFC, "Could not open amiibo file \"{}\"", filename);
        return false;
    }

    if (!amiibo_file.ReadBytes(&tag.file, sizeof(tag.file))) {
        LOG_ERROR(Service_NFC, "Could not read amiibo data from file \"{}\"", filename);
        tag.file = {};
        return false;
    }

    // TODO: Filter by allowed_protocols here
    is_plain_amiibo = AmiiboCrypto::IsAmiiboValid(tag.file);
    is_write_protected = false;

    amiibo_filename = filename;
    device_state = DeviceState::TagFound;
    is_tag_in_range = true;
    tag_out_of_range_event->Clear();
    tag_in_range_event->Signal();

    RescheduleTagRemoveEvent();

    // Fallback for plain amiibos
    if (is_plain_amiibo) {
        LOG_INFO(Service_NFC, "Using plain amiibo");
        encrypted_tag.file = AmiiboCrypto::EncodedDataToNfcData(tag.file);
        return true;
    }

    // Fallback for encrypted amiibos without keys
    if (!HW::AES::NfcSecretsAvailable()) {
        LOG_INFO(Service_NFC, "Loading amiibo without keys");
        memcpy(&encrypted_tag.raw, &tag.raw, sizeof(EncryptedNTAG215File));
        tag.file = {};
        BuildAmiiboWithoutKeys();
        is_plain_amiibo = true;
        is_write_protected = true;
        return true;
    }

    LOG_INFO(Service_NFC, "Loading amiibo with keys");
    memcpy(&encrypted_tag.raw, &tag.raw, sizeof(EncryptedNTAG215File));
    tag.file = {};
    return true;
}

void NfcDevice::UnloadAmiibo() {
    is_tag_in_range = false;
    amiibo_filename = "";
    CloseAmiibo();
}

void NfcDevice::CloseAmiibo() {
    if (device_state == DeviceState::NotInitialized || device_state == DeviceState::Initialized ||
        device_state == DeviceState::SearchingForTag || device_state == DeviceState::TagRemoved) {
        return;
    }

    LOG_INFO(Service_NFC, "Amiibo removed");

    if (device_state == DeviceState::TagMounted ||
        device_state == DeviceState::TagPartiallyMounted) {
        ResetTagScanState();
    }

    device_state = DeviceState::TagRemoved;
    encrypted_tag.file = {};
    tag.file = {};
    tag_in_range_event->Clear();
    tag_out_of_range_event->Signal();
}

std::shared_ptr<Kernel::Event> NfcDevice::GetActivateEvent() const {
    return tag_in_range_event;
}

std::shared_ptr<Kernel::Event> NfcDevice::GetDeactivateEvent() const {
    return tag_out_of_range_event;
}

void NfcDevice::Initialize() {
    device_state = DeviceState::Initialized;
    encrypted_tag.file = {};
    tag.file = {};
    is_initalized = true;
}

void NfcDevice::Finalize() {
    if (device_state == DeviceState::TagMounted) {
        ResetTagScanState();
    }
    if (device_state == DeviceState::SearchingForTag || device_state == DeviceState::TagRemoved) {
        StopDetection();
    }
    device_state = DeviceState::NotInitialized;
    connection_state = ConnectionState::Success;
    communication_state = CommunicationState::Idle;
    is_initalized = false;
}

Result NfcDevice::StartCommunication() {
    const auto connection_result = CheckConnectionState();
    if (connection_result.IsError()) {
        return connection_result;
    }

    if (device_state != DeviceState::Initialized ||
        communication_state != CommunicationState::Idle) {
        return ResultInvalidOperation;
    }

    communication_state = CommunicationState::SearchingForAdapter;

    // This is a hack. This mode needs to change when the tag reader has completed the initalization
    communication_state = CommunicationState::Initialized;
    return ResultSuccess;
}

Result NfcDevice::StopCommunication() {
    const auto connection_result = CheckConnectionState();
    if (connection_result.IsError()) {
        return connection_result;
    }

    if (communication_state == CommunicationState::Idle) {
        return ResultInvalidOperation;
    }

    if (device_state == DeviceState::TagMounted ||
        device_state == DeviceState::TagPartiallyMounted) {
        ResetTagScanState();
    }

    device_state = DeviceState::Initialized;
    communication_state = CommunicationState::Idle;
    return ResultSuccess;
}

Result NfcDevice::StartDetection(TagProtocol allowed_protocol) {
    const auto connection_result = CheckConnectionState();
    if (connection_result.IsError()) {
        return connection_result;
    }

    if (device_state != DeviceState::Initialized && device_state != DeviceState::TagRemoved &&
        communication_state != CommunicationState::Initialized) {
        return ResultInvalidOperation;
    }

    // Ensure external device is active
    if (communication_state == CommunicationState::Idle) {
        StartCommunication();
    }

    device_state = DeviceState::SearchingForTag;
    allowed_protocols = allowed_protocol;

    if (is_tag_in_range) {
        LoadAmiibo(amiibo_filename);
    }

    return ResultSuccess;
}

Result NfcDevice::StopDetection() {
    const auto connection_result = CheckConnectionState();
    if (connection_result.IsError()) {
        return connection_result;
    }

    if (device_state == DeviceState::TagMounted ||
        device_state == DeviceState::TagPartiallyMounted) {
        ResetTagScanState();
    }

    if (device_state == DeviceState::TagFound || device_state == DeviceState::SearchingForTag) {
        CloseAmiibo();
    }

    if (device_state == DeviceState::TagFound || device_state == DeviceState::SearchingForTag ||
        device_state == DeviceState::TagRemoved) {
        // TODO: Stop console search mode here
        device_state = DeviceState::Initialized;
        connection_state = ConnectionState::Success;
        return ResultSuccess;
    }

    LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
    return ResultInvalidOperation;
}

Result NfcDevice::Flush() {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    // Ensure the tag will not be removed in the middle of a write
    RescheduleTagRemoveEvent();

    auto& settings = tag.file.settings;

    const auto& current_date = GetAmiiboDate();
    if (settings.write_date.raw_date != current_date.raw_date) {
        settings.write_date = current_date;
        UpdateSettingsCrc();
    }

    tag.file.write_counter++;

    if (is_write_protected) {
        LOG_ERROR(Service_NFC, "No keys available skipping write request");
        return ResultSuccess;
    }

    if (!is_plain_amiibo) {
        if (!AmiiboCrypto::EncodeAmiibo(tag.file, encrypted_tag.file)) {
            LOG_ERROR(Service_NFC, "Failed to encode data");
            return ResultOperationFailed;
        }

        if (amiibo_filename.empty()) {
            LOG_ERROR(Service_NFC, "Tried to use UpdateStoredAmiiboData on a nonexistant file.");
            return ResultOperationFailed;
        }
    }

    FileUtil::IOFile amiibo_file(amiibo_filename, "wb");
    bool write_failed = false;

    if (!amiibo_file.IsOpen()) {
        LOG_ERROR(Service_NFC, "Could not open amiibo file \"{}\"", amiibo_filename);
        write_failed = true;
    }
    if (!is_plain_amiibo) {
        if (!write_failed &&
            !amiibo_file.WriteBytes(&encrypted_tag.file, sizeof(EncryptedNTAG215File))) {
            LOG_ERROR(Service_NFC, "Could not write to amiibo file \"{}\"", amiibo_filename);
            write_failed = true;
        }
    } else {
        if (!write_failed && !amiibo_file.WriteBytes(&tag.file, sizeof(EncryptedNTAG215File))) {
            LOG_ERROR(Service_NFC, "Could not write to amiibo file \"{}\"", amiibo_filename);
            write_failed = true;
        }
    }
    amiibo_file.Close();

    if (write_failed) {
        return ResultOperationFailed;
    }

    is_data_moddified = false;

    return ResultSuccess;
}

Result NfcDevice::Mount() {
    if (device_state != DeviceState::TagFound) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    if (!AmiiboCrypto::IsAmiiboValid(encrypted_tag.file)) {
        LOG_ERROR(Service_NFC, "Not an amiibo");
        return ResultNotSupported;
    }

    // The loaded amiibo is not encrypted
    if (is_plain_amiibo) {
        device_state = DeviceState::TagMounted;
        return ResultSuccess;
    }

    if (!AmiiboCrypto::DecodeAmiibo(encrypted_tag.file, tag.file)) {
        LOG_ERROR(Service_NFC, "Can't decode amiibo {}", device_state);
        return ResultNeedFormat;
    }

    device_state = DeviceState::TagMounted;
    return ResultSuccess;
}

Result NfcDevice::MountAmiibo() {
    TagInfo tag_info{};
    const auto result = GetTagInfo(tag_info);

    if (result.IsError()) {
        return result;
    }

    if (tag_info.tag_type != PackedTagType::Type2) {
        return ResultNotSupported;
    }

    return Mount();
}

Result NfcDevice::PartiallyMount() {
    if (device_state != DeviceState::TagFound) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    if (!AmiiboCrypto::IsAmiiboValid(encrypted_tag.file)) {
        LOG_ERROR(Service_NFC, "Not an amiibo");
        return ResultNotSupported;
    }

    // The loaded amiibo is not encrypted
    if (is_plain_amiibo) {
        device_state = DeviceState::TagPartiallyMounted;
        return ResultSuccess;
    }

    if (!AmiiboCrypto::DecodeAmiibo(encrypted_tag.file, tag.file)) {
        LOG_ERROR(Service_NFC, "Can't decode amiibo {}", device_state);
        return ResultNeedFormat;
    }

    device_state = DeviceState::TagPartiallyMounted;
    return ResultSuccess;
}

Result NfcDevice::PartiallyMountAmiibo() {
    TagInfo tag_info{};
    const auto result = GetTagInfo(tag_info);

    if (result.IsError()) {
        return result;
    }

    if (tag_info.tag_type != PackedTagType::Type2) {
        return ResultNotSupported;
    }

    return PartiallyMount();
}
Result NfcDevice::ResetTagScanState() {
    if (device_state != DeviceState::TagMounted &&
        device_state != DeviceState::TagPartiallyMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    device_state = DeviceState::TagFound;
    is_app_area_open = false;

    return ResultSuccess;
}

Result NfcDevice::GetTagInfo2(TagInfo2& tag_info) const {
    tag_info = {
        .uuid_length = static_cast<u16>(encrypted_tag.file.uuid.uid.size()),
        .tag_type = PackedTagType::Type2,
        .uuid = encrypted_tag.file.uuid.uid,
        .extra_data = {}, // Used on non amiibo tags
        .protocol = TagProtocol::None,
        .extra_data2 = {}, // Used on non amiibo tags
    };

    return ResultSuccess;
}

Result NfcDevice::GetTagInfo(TagInfo& tag_info) const {
    if (device_state != DeviceState::TagFound && device_state != DeviceState::TagMounted &&
        device_state != DeviceState::TagPartiallyMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    tag_info = {
        .uuid_length = static_cast<u16>(encrypted_tag.file.uuid.uid.size()),
        .protocol = PackedTagProtocol::None,
        .tag_type = PackedTagType::Type2,
        .uuid = encrypted_tag.file.uuid.uid,
        .extra_data = {}, // Used on non amiibo tags
    };

    return ResultSuccess;
}

Result NfcDevice::GetCommonInfo(CommonInfo& common_info) const {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    const auto& settings = tag.file.settings;
    const auto& model_info_data = tag.file.model_info;

    // TODO: Validate this data
    common_info = {
        .last_write_date = settings.write_date.GetWriteDate(),
        .application_write_counter = tag.file.application_write_counter,
        .character_id = model_info_data.character_id,
        .character_variant = model_info_data.character_variant,
        .series = model_info_data.series,
        .model_number = model_info_data.model_number,
        .amiibo_type = model_info_data.amiibo_type,
        .version = tag.file.amiibo_version,
        .application_area_size = sizeof(ApplicationArea),
    };

    return ResultSuccess;
}

Result NfcDevice::GetModelInfo(ModelInfo& model_info) const {
    if (device_state != DeviceState::TagMounted &&
        device_state != DeviceState::TagPartiallyMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    const auto& model_info_data = encrypted_tag.file.user_memory.model_info;
    model_info = {
        .character_id = model_info_data.character_id,
        .character_variant = model_info_data.character_variant,
        .series = model_info_data.series,
        .model_number = model_info_data.model_number,
        .amiibo_type = model_info_data.amiibo_type,
    };

    return ResultSuccess;
}

Result NfcDevice::GetRegisterInfo(RegisterInfo& register_info) const {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    if (tag.file.settings.settings.amiibo_initialized == 0) {
        return ResultNeedRegister;
    }

    const auto& settings = tag.file.settings;

    // TODO: Validate this data
    register_info = {
        .mii_data = tag.file.owner_mii,
        .amiibo_name = settings.amiibo_name,
        .flags = static_cast<u8>(settings.settings.raw & 0xf),
        .font_region = settings.country_code_id,
        .creation_date = settings.init_date.GetWriteDate(),
    };

    return ResultSuccess;
}

Result NfcDevice::GetAdminInfo(AdminInfo& admin_info) const {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    u8 flags = static_cast<u8>(tag.file.settings.settings.raw >> 0x4);
    if (tag.file.settings.settings.amiibo_initialized == 0) {
        flags = flags & 0xfe;
    }

    u64 application_id = 0;
    u32 application_area_id = 0;
    AppAreaVersion app_area_version = AppAreaVersion::NotSet;
    if (tag.file.settings.settings.appdata_initialized != 0) {
        application_id = tag.file.application_id;
        app_area_version =
            static_cast<AppAreaVersion>(application_id >> application_id_version_offset & 0xf);

        switch (app_area_version) {
        case AppAreaVersion::Nintendo3DS:
        case AppAreaVersion::Nintendo3DSv2:
            app_area_version = AppAreaVersion::Nintendo3DS;
            break;
        case AppAreaVersion::NintendoWiiU:
            app_area_version = AppAreaVersion::NintendoWiiU;
            break;
        default:
            app_area_version = AppAreaVersion::NotSet;
            break;
        }

        // Restore application id to original value
        if (application_id >> 0x38 != 0) {
            const u8 application_byte = tag.file.application_id_byte & 0xf;
            application_id = RemoveVersionByte(application_id) |
                             (static_cast<u64>(application_byte) << application_id_version_offset);
        }

        application_area_id = tag.file.application_area_id;
    }

    // TODO: Validate this data
    admin_info = {
        .application_id = application_id,
        .application_area_id = application_area_id,
        .crc_counter = tag.file.settings.crc_counter,
        .flags = flags,
        .tag_type = PackedTagType::Type2,
        .app_area_version = app_area_version,
    };

    return ResultSuccess;
}

Result NfcDevice::DeleteRegisterInfo() {
    // This is a hack to get around a HW issue
    if (device_state == DeviceState::TagFound) {
        Mount();
    }

    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    if (tag.file.settings.settings.amiibo_initialized == 0) {
        return ResultNeedRegister;
    }

    CryptoPP::AutoSeededRandomPool rng;
    const std::size_t mii_data_size = sizeof(tag.file.owner_mii);
    std::array<CryptoPP::byte, mii_data_size> buffer{};
    rng.GenerateBlock(buffer.data(), mii_data_size);

    memcpy(&tag.file.owner_mii, buffer.data(), mii_data_size);
    memcpy(&tag.file.settings.amiibo_name, buffer.data(), sizeof(tag.file.settings.amiibo_name));
    tag.file.unknown = rng.GenerateByte();
    tag.file.unknown2[0] = rng.GenerateWord32();
    tag.file.unknown2[1] = rng.GenerateWord32();
    tag.file.register_info_crc = rng.GenerateWord32();
    tag.file.settings.init_date.raw_date = static_cast<u32>(rng.GenerateWord32());
    tag.file.settings.settings.font_region.Assign(0);
    tag.file.settings.settings.amiibo_initialized.Assign(0);
    tag.file.settings.country_code_id = 0;

    return Flush();
}

Result NfcDevice::SetRegisterInfoPrivate(const RegisterInfoPrivate& register_info) {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    auto& settings = tag.file.settings;

    if (tag.file.settings.settings.amiibo_initialized == 0) {
        settings.init_date = GetAmiiboDate();
        settings.write_date = GetAmiiboDate();
    }

    settings.amiibo_name = register_info.amiibo_name;
    tag.file.owner_mii = register_info.mii_data;
    tag.file.owner_mii.FixChecksum();
    tag.file.mii_extension = {};
    tag.file.unknown = 0;
    tag.file.unknown2 = {};
    settings.country_code_id = 0;
    settings.settings.font_region.Assign(0);
    settings.settings.amiibo_initialized.Assign(1);

    UpdateRegisterInfoCrc();

    return Flush();
}

Result NfcDevice::RestoreAmiibo() {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    // TODO: Load amiibo from backup on system
    LOG_ERROR(Service_NFC, "Not Implemented");
    return ResultSuccess;
}

Result NfcDevice::Format() {
    auto Result = DeleteApplicationArea();
    auto Result2 = DeleteRegisterInfo();

    if (Result.IsError()) {
        return Result;
    }

    if (Result2.IsError()) {
        return Result2;
    }

    return ResultSuccess;
}

Result NfcDevice::OpenApplicationArea(u32 access_id) {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    if (tag.file.settings.settings.appdata_initialized.Value() == 0) {
        LOG_WARNING(Service_NFC, "Application area is not initialized");
        return ResultNeedCreate;
    }

    if (tag.file.application_area_id != access_id) {
        LOG_WARNING(Service_NFC, "Wrong application area id");
        return ResultAccessIdMisMatch;
    }

    is_app_area_open = true;

    return ResultSuccess;
}

Result NfcDevice::GetApplicationAreaId(u32& application_area_id) const {
    application_area_id = {};

    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    if (tag.file.settings.settings.appdata_initialized.Value() == 0) {
        LOG_WARNING(Service_NFC, "Application area is not initialized");
        return ResultNeedCreate;
    }

    application_area_id = tag.file.application_area_id;

    return ResultSuccess;
}

Result NfcDevice::GetApplicationArea(std::vector<u8>& data) const {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    if (!is_app_area_open) {
        LOG_ERROR(Service_NFC, "Application area is not open");
        return ResultInvalidOperation;
    }

    if (tag.file.settings.settings.appdata_initialized.Value() == 0) {
        LOG_ERROR(Service_NFC, "Application area is not initialized");
        return ResultNeedCreate;
    }

    if (data.size() > sizeof(ApplicationArea)) {
        data.resize(sizeof(ApplicationArea));
    }

    memcpy(data.data(), tag.file.application_area.data(), data.size());

    return ResultSuccess;
}

Result NfcDevice::SetApplicationArea(std::span<const u8> data) {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    if (data.size() == 0) {
        LOG_ERROR(Service_NFC, "Data buffer is null");
        return ResultInvalidArgument;
    }

    if (data.size() > sizeof(ApplicationArea)) {
        LOG_ERROR(Service_NFC, "Wrong data size {}", data.size());
        return ResultInvalidArgumentValue;
    }

    if (!is_app_area_open) {
        LOG_ERROR(Service_NFC, "Application area is not open");
        return ResultInvalidOperation;
    }

    if (tag.file.settings.settings.appdata_initialized.Value() == 0) {
        LOG_ERROR(Service_NFC, "Application area is not initialized");
        return ResultNeedCreate;
    }

    std::memcpy(tag.file.application_area.data(), data.data(), data.size());

    // Fill remaining data with random numbers
    CryptoPP::AutoSeededRandomPool rng;
    const std::size_t data_size = sizeof(ApplicationArea) - data.size();
    std::vector<CryptoPP::byte> buffer(data_size);
    rng.GenerateBlock(buffer.data(), data_size);
    memcpy(tag.file.application_area.data() + data.size(), buffer.data(), data_size);

    if (tag.file.application_write_counter != counter_limit) {
        tag.file.application_write_counter++;
    }

    is_data_moddified = true;

    return ResultSuccess;
}

Result NfcDevice::CreateApplicationArea(u32 access_id, std::span<const u8> data) {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    if (tag.file.settings.settings.appdata_initialized.Value() != 0) {
        LOG_ERROR(Service_NFC, "Application area already exist");
        return ResultAlreadyCreated;
    }

    return RecreateApplicationArea(access_id, data);
}

Result NfcDevice::RecreateApplicationArea(u32 access_id, std::span<const u8> data) {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    if (data.size() > sizeof(ApplicationArea)) {
        LOG_ERROR(Service_NFC, "Wrong data size {}", data.size());
        return ResultInvalidArgumentValue;
    }

    if (is_app_area_open) {
        LOG_ERROR(Service_NFC, "Application area is open");
        return ResultInvalidOperation;
    }

    std::memcpy(tag.file.application_area.data(), data.data(), data.size());

    // Fill remaining data with random numbers
    CryptoPP::AutoSeededRandomPool rng;
    const std::size_t data_size = sizeof(ApplicationArea) - data.size();
    std::vector<CryptoPP::byte> buffer(data_size);
    rng.GenerateBlock(buffer.data(), data_size);
    memcpy(tag.file.application_area.data() + data.size(), buffer.data(), data_size);

    if (tag.file.application_write_counter != counter_limit) {
        tag.file.application_write_counter++;
    }

    u64 application_id{};
    if (system.GetAppLoader().ReadProgramId(application_id) == Loader::ResultStatus::Success) {
        tag.file.application_id_byte =
            static_cast<u8>(application_id >> application_id_version_offset & 0xf);
        tag.file.application_id =
            RemoveVersionByte(application_id) |
            (static_cast<u64>(AppAreaVersion::Nintendo3DSv2) << application_id_version_offset);
    }
    tag.file.settings.settings.appdata_initialized.Assign(1);
    tag.file.application_area_id = access_id;
    tag.file.unknown = {};
    tag.file.unknown2 = {};

    UpdateRegisterInfoCrc();

    return Flush();
}

Result NfcDevice::DeleteApplicationArea() {
    // This is a hack to get around a HW issue
    if (device_state == DeviceState::TagFound) {
        Mount();
    }

    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    if (tag.file.settings.settings.appdata_initialized == 0) {
        return ResultNeedCreate;
    }

    CryptoPP::AutoSeededRandomPool rng;
    constexpr std::size_t data_size = sizeof(ApplicationArea);
    std::array<CryptoPP::byte, data_size> buffer{};
    rng.GenerateBlock(buffer.data(), data_size);

    if (tag.file.application_write_counter != counter_limit) {
        tag.file.application_write_counter++;
    }

    // Reset data with random bytes
    memcpy(tag.file.application_area.data(), buffer.data(), data_size); //
    memcpy(&tag.file.application_id, buffer.data(), sizeof(u64));
    tag.file.application_area_id = rng.GenerateWord32();
    tag.file.application_id_byte = rng.GenerateByte();
    tag.file.settings.settings.appdata_initialized.Assign(0);
    tag.file.unknown = {};
    tag.file.unknown2 = {};
    is_app_area_open = false;

    UpdateRegisterInfoCrc();

    return Flush();
}

Result NfcDevice::ApplicationAreaExist(bool& has_application_area) {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        const auto connection_result = CheckConnectionState();
        if (connection_result.IsSuccess()) {
            return ResultInvalidOperation;
        }
        return connection_result;
    }

    has_application_area = tag.file.settings.settings.appdata_initialized.Value() != 0;

    return ResultSuccess;
}

constexpr u32 NfcDevice::GetApplicationAreaSize() const {
    return sizeof(ApplicationArea);
}

DeviceState NfcDevice::GetCurrentState() const {
    return device_state;
}

Result NfcDevice::GetCommunicationStatus(CommunicationState& status) const {
    if (communication_state == CommunicationState::Idle ||
        communication_state == CommunicationState::SearchingForAdapter) {
        status = communication_state;
        return ResultSuccess;
    }

    if (communication_state == CommunicationState::Initialized ||
        communication_state == CommunicationState::Active) {
        status = CommunicationState::Initialized;
        return ResultSuccess;
    }

    return ResultInvalidOperation;
}

Result NfcDevice::CheckConnectionState() const {
    if (connection_state == ConnectionState::Lost) {
        return ResultSleep;
    }

    if (connection_state == ConnectionState::NoAdapter) {
        return ResultWifiOff;
    }

    return ResultSuccess;
}

void NfcDevice::SetAmiiboName(AmiiboSettings& settings, const AmiiboName& amiibo_name) {
    // Convert from little endian to big endian
    for (std::size_t i = 0; i < amiibo_name_length; i++) {
        settings.amiibo_name[i] = static_cast<u16_be>(amiibo_name[i]);
    }
}

time_t NfcDevice::GetCurrentTime() const {
    auto& share_page = system.Kernel().GetSharedPageHandler();
    const auto console_time = share_page.GetSharedPage().date_time_1.date_time / 1000;

    // 3DS console time uses Jan 1 1900 as internal epoch,
    // so we use the seconds between 1900 and 2000 as base console time
    constexpr u64 START_DATE = 3155673600ULL;

    // 3DS system does't allow user to set a time before Jan 1 2000,
    // so we use it as an auxiliary epoch to calculate the console time.
    std::tm epoch_tm;
    epoch_tm.tm_sec = 0;
    epoch_tm.tm_min = 0;
    epoch_tm.tm_hour = 0;
    epoch_tm.tm_mday = 1;
    epoch_tm.tm_mon = 0;
    epoch_tm.tm_year = 100;
    epoch_tm.tm_isdst = 0;
    s64 epoch = std::mktime(&epoch_tm);
    return static_cast<time_t>(console_time - START_DATE + epoch);
}

AmiiboDate NfcDevice::GetAmiiboDate() const {
    auto now = GetCurrentTime();
    tm local_tm = *localtime(&now);
    AmiiboDate amiibo_date{};

    amiibo_date.SetYear(static_cast<u16>(local_tm.tm_year + 1900));
    amiibo_date.SetMonth(static_cast<u8>(local_tm.tm_mon + 1));
    amiibo_date.SetDay(static_cast<u8>(local_tm.tm_mday));

    return amiibo_date;
}

u64 NfcDevice::RemoveVersionByte(u64 application_id) const {
    return application_id & ~(0xfULL << application_id_version_offset);
}

void NfcDevice::UpdateSettingsCrc() {
    auto& settings = tag.file.settings;

    if (settings.crc_counter != counter_limit) {
        settings.crc_counter++;
    }

    // TODO: this reads data from a global, find what it is
    std::array<u8, 8> unknown_input{};
    boost::crc_32_type crc;
    crc.process_bytes(&unknown_input, sizeof(unknown_input));
    settings.crc = crc.checksum();
}

void NfcDevice::UpdateRegisterInfoCrc() {
#pragma pack(push, 1)
    struct CrcData {
        Mii::ChecksummedMiiData mii;
        u8 application_id_byte;
        u8 unknown;
        u64 mii_extension;
        std::array<u32, 0x5> unknown2;
    };
    static_assert(sizeof(CrcData) == 0x7e, "CrcData is an invalid size");
#pragma pack(pop)

    const CrcData crc_data{
        .mii = tag.file.owner_mii,
        .application_id_byte = tag.file.application_id_byte,
        .unknown = tag.file.unknown,
        .mii_extension = tag.file.mii_extension,
        .unknown2 = tag.file.unknown2,
    };

    boost::crc_32_type crc;
    crc.process_bytes(&crc_data, sizeof(CrcData));
    tag.file.register_info_crc = crc.checksum();
}

void NfcDevice::BuildAmiiboWithoutKeys() {
    auto& settings = tag.file.settings;
    const auto default_mii = HLE::Applets::MiiSelector::GetStandardMiiResult();

    tag.file = AmiiboCrypto::NfcDataToEncodedData(encrypted_tag.file);

    // Common info
    tag.file.write_counter = 0;
    tag.file.amiibo_version = 0;
    settings.write_date = GetAmiiboDate();

    // Register info
    SetAmiiboName(settings, {'c', 'i', 't', 'r', 'A', 'm', 'i', 'i', 'b', 'o'});
    settings.settings.font_region.Assign(0);
    settings.init_date = GetAmiiboDate();
    tag.file.owner_mii = default_mii.selected_mii_data;

    // Admin info
    settings.settings.amiibo_initialized.Assign(1);
    settings.settings.appdata_initialized.Assign(0);
}

void NfcDevice::RescheduleTagRemoveEvent() {
    /// The interval at which the amiibo will be removed automatically 3s
    static constexpr u64 amiibo_removal_interval = msToCycles(3 * 1000);

    system.CoreTiming().UnscheduleEvent(remove_amiibo_event, 0);

    if (device_state != DeviceState::TagFound && device_state != DeviceState::TagMounted &&
        device_state != DeviceState::TagPartiallyMounted) {
        return;
    }

    system.CoreTiming().ScheduleEvent(amiibo_removal_interval, remove_amiibo_event);
}

} // namespace Service::NFC
