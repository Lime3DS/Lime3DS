// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

// Configuration memory stores various hardware/kernel configuration settings. This memory page is
// read-only for ARM11 processes. I'm guessing this would normally be written to by the firmware/
// bootrom. Because we're not emulating this, and essentially just "stubbing" the functionality, I'm
// putting this as a subset of HLE for now.

#include <boost/serialization/export.hpp>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/memory_ref.h"
#include "common/swap.h"
#include "core/memory.h"

namespace ConfigMem {

struct ConfigMemDef {
    u8 kernel_unk;                       // 0
    u8 kernel_version_rev;               // 1
    u8 kernel_version_min;               // 2
    u8 kernel_version_maj;               // 3
    u32_le update_flag;                  // 4
    u64_le ns_tid;                       // 8
    u32_le sys_core_ver;                 // 10
    u8 unit_info;                        // 14
    u8 boot_firm;                        // 15
    u8 prev_firm;                        // 16
    INSERT_PADDING_BYTES(0x1);           // 17
    u32_le ctr_sdk_ver;                  // 18
    INSERT_PADDING_BYTES(0x30 - 0x1C);   // 1C
    u32_le app_mem_type;                 // 30
    INSERT_PADDING_BYTES(0x40 - 0x34);   // 34
    u32_le app_mem_alloc;                // 40
    u32_le sys_mem_alloc;                // 44
    u32_le base_mem_alloc;               // 48
    INSERT_PADDING_BYTES(0x60 - 0x4C);   // 4C
    u8 firm_unk;                         // 60
    u8 firm_version_rev;                 // 61
    u8 firm_version_min;                 // 62
    u8 firm_version_maj;                 // 63
    u32_le firm_sys_core_ver;            // 64
    u32_le firm_ctr_sdk_ver;             // 68
    INSERT_PADDING_BYTES(0x1000 - 0x6C); // 6C
};
static_assert(sizeof(ConfigMemDef) == Memory::CONFIG_MEMORY_SIZE,
              "Config Memory structure size is wrong");

class Handler : public BackingMem {
public:
    Handler();
    ConfigMemDef& GetConfigMem();

    u8* GetPtr() override {
        return reinterpret_cast<u8*>(&config_mem);
    }

    const u8* GetPtr() const override {
        return reinterpret_cast<const u8*>(&config_mem);
    }

    std::size_t GetSize() const override {
        return sizeof(config_mem);
    }

private:
    ConfigMemDef config_mem;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

} // namespace ConfigMem

BOOST_CLASS_EXPORT_KEY(ConfigMem::Handler)
