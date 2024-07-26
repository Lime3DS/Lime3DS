// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include "common/alignment.h"
#include "core/file_sys/cia_common.h"
#include "core/file_sys/ticket.h"
#include "core/hw/aes/key.h"
#include "core/loader/loader.h"

namespace FileSys {

Loader::ResultStatus Ticket::Load(std::span<const u8> file_data, std::size_t offset) {
    std::size_t total_size = static_cast<std::size_t>(file_data.size() - offset);
    if (total_size < sizeof(u32))
        return Loader::ResultStatus::Error;

    std::memcpy(&signature_type, &file_data[offset], sizeof(u32));

    // Signature lengths are variable, and the body follows the signature
    u32 signature_size = GetSignatureSize(signature_type);
    if (signature_size == 0) {
        return Loader::ResultStatus::Error;
    }

    // The ticket body start position is rounded to the nearest 0x40 after the signature
    std::size_t body_start = Common::AlignUp(signature_size + sizeof(u32), 0x40);
    std::size_t body_end = body_start + sizeof(Body);

    if (total_size < body_end)
        return Loader::ResultStatus::Error;

    // Read signature + ticket body
    ticket_signature.resize(signature_size);
    std::memcpy(ticket_signature.data(), &file_data[offset + sizeof(u32)], signature_size);
    std::memcpy(&ticket_body, &file_data[offset + body_start], sizeof(Body));

    return Loader::ResultStatus::Success;
}

std::optional<std::array<u8, 16>> Ticket::GetTitleKey() const {
    HW::AES::InitKeys();
    std::array<u8, 16> ctr{};
    std::memcpy(ctr.data(), &ticket_body.title_id, sizeof(u64));
    HW::AES::SelectCommonKeyIndex(ticket_body.common_key_index);
    if (!HW::AES::IsNormalKeyAvailable(HW::AES::KeySlotID::TicketCommonKey)) {
        LOG_ERROR(Service_FS, "CommonKey {} missing", ticket_body.common_key_index);
        return {};
    }
    auto key = HW::AES::GetNormalKey(HW::AES::KeySlotID::TicketCommonKey);
    auto title_key = ticket_body.title_key;
    CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption{key.data(), key.size(), ctr.data()}.ProcessData(
        title_key.data(), title_key.data(), title_key.size());
    return title_key;
}

} // namespace FileSys
