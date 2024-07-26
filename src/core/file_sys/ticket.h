// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"

namespace Loader {
enum class ResultStatus;
}

namespace FileSys {
class Ticket {
public:
#pragma pack(push, 1)
    struct Body {
        std::array<u8, 0x40> issuer;
        std::array<u8, 0x3C> ecc_public_key;
        u8 version;
        u8 ca_crl_version;
        u8 signer_crl_version;
        std::array<u8, 0x10> title_key;
        INSERT_PADDING_BYTES(1);
        u64_be ticket_id;
        u32_be console_id;
        u64_be title_id;
        INSERT_PADDING_BYTES(2);
        u16_be ticket_title_version;
        INSERT_PADDING_BYTES(8);
        u8 license_type;
        u8 common_key_index;
        INSERT_PADDING_BYTES(0x2A);
        u32_be eshop_account_id;
        INSERT_PADDING_BYTES(1);
        u8 audit;
        INSERT_PADDING_BYTES(0x42);
        std::array<u8, 0x40> limits;
        std::array<u8, 0xAC> content_index;
    };
    static_assert(sizeof(Body) == 0x210, "Ticket body structure size is wrong");
#pragma pack(pop)

    Loader::ResultStatus Load(std::span<const u8> file_data, std::size_t offset = 0);
    std::optional<std::array<u8, 16>> GetTitleKey() const;
    u64 GetTitleID() const {
        return ticket_body.title_id;
    }

private:
    Body ticket_body;
    u32_be signature_type;
    std::vector<u8> ticket_signature;
};
} // namespace FileSys
