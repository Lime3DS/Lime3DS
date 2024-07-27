// Copyright 2020 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>
#include <vector>
#include "common/common_types.h"

namespace HW::RSA {

class RsaSlot {
public:
    RsaSlot() = default;
    RsaSlot(std::vector<u8> exponent, std::vector<u8> modulus)
        : init(true), exponent(std::move(exponent)), modulus(std::move(modulus)) {}
    std::vector<u8> GetSignature(std::span<const u8> message) const;

    explicit operator bool() const {
        // TODO(B3N30): Maybe check if exponent and modulus are vailid
        return init;
    }

private:
    bool init = false;
    std::vector<u8> exponent;
    std::vector<u8> modulus;
};

void InitSlots();

RsaSlot GetSlot(std::size_t slot_id);

std::vector<u8> CreateASN1Message(std::span<const u8> data);

} // namespace HW::RSA
