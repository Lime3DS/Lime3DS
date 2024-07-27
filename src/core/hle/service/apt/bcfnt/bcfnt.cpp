// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/apt/bcfnt/bcfnt.h"
#include "core/hle/service/service.h"

namespace Service::APT::BCFNT {

void RelocateSharedFont(std::shared_ptr<Kernel::SharedMemory> shared_font, VAddr new_address) {
    static const u32 SharedFontStartOffset = 0x80;
    const u8* cfnt_ptr = shared_font->GetPointer(SharedFontStartOffset);

    CFNT cfnt;
    std::memcpy(&cfnt, cfnt_ptr, sizeof(cfnt));

    u32 assumed_cmap_offset = 0;
    u32 assumed_cwdh_offset = 0;
    u32 assumed_tglp_offset = 0;
    u32 first_cmap_offset = 0;
    u32 first_cwdh_offset = 0;
    u32 first_tglp_offset = 0;

    // First discover the location of sections so that the rebase offset can be auto-detected
    u32 current_offset = SharedFontStartOffset + cfnt.header_size;
    for (unsigned block = 0; block < cfnt.num_blocks; ++block) {
        const u8* data = shared_font->GetPointer(current_offset);

        SectionHeader section_header;
        std::memcpy(&section_header, data, sizeof(section_header));

        if (first_cmap_offset == 0 && std::memcmp(section_header.magic, "CMAP", 4) == 0) {
            first_cmap_offset = current_offset;
        } else if (first_cwdh_offset == 0 && std::memcmp(section_header.magic, "CWDH", 4) == 0) {
            first_cwdh_offset = current_offset;
        } else if (first_tglp_offset == 0 && std::memcmp(section_header.magic, "TGLP", 4) == 0) {
            first_tglp_offset = current_offset;
        } else if (std::memcmp(section_header.magic, "FINF", 4) == 0) {
            BCFNT::FINF finf;
            std::memcpy(&finf, data, sizeof(finf));

            assumed_cmap_offset = finf.cmap_offset - sizeof(SectionHeader);
            assumed_cwdh_offset = finf.cwdh_offset - sizeof(SectionHeader);
            assumed_tglp_offset = finf.tglp_offset - sizeof(SectionHeader);
        }

        current_offset += section_header.section_size;
    }

    u32 previous_base = assumed_cmap_offset - first_cmap_offset;
    ASSERT(previous_base == assumed_cwdh_offset - first_cwdh_offset);
    ASSERT(previous_base == assumed_tglp_offset - first_tglp_offset);

    u32 offset = new_address - previous_base;

    // Reset pointer back to start of sections and do the actual rebase
    current_offset = SharedFontStartOffset + cfnt.header_size;
    for (unsigned block = 0; block < cfnt.num_blocks; ++block) {
        u8* data = shared_font->GetPointer(current_offset);

        SectionHeader section_header;
        std::memcpy(&section_header, data, sizeof(section_header));

        if (std::memcmp(section_header.magic, "FINF", 4) == 0) {
            BCFNT::FINF finf;
            std::memcpy(&finf, data, sizeof(finf));

            // Relocate the offsets in the FINF section
            finf.cmap_offset += offset;
            finf.cwdh_offset += offset;
            finf.tglp_offset += offset;

            std::memcpy(data, &finf, sizeof(finf));
        } else if (std::memcmp(section_header.magic, "CMAP", 4) == 0) {
            BCFNT::CMAP cmap;
            std::memcpy(&cmap, data, sizeof(cmap));

            // Relocate the offsets in the CMAP section
            if (cmap.next_cmap_offset != 0)
                cmap.next_cmap_offset += offset;

            std::memcpy(data, &cmap, sizeof(cmap));
        } else if (std::memcmp(section_header.magic, "CWDH", 4) == 0) {
            BCFNT::CWDH cwdh;
            std::memcpy(&cwdh, data, sizeof(cwdh));

            // Relocate the offsets in the CWDH section
            if (cwdh.next_cwdh_offset != 0)
                cwdh.next_cwdh_offset += offset;

            std::memcpy(data, &cwdh, sizeof(cwdh));
        } else if (std::memcmp(section_header.magic, "TGLP", 4) == 0) {
            BCFNT::TGLP tglp;
            std::memcpy(&tglp, data, sizeof(tglp));

            // Relocate the offsets in the TGLP section
            tglp.sheet_data_offset += offset;

            std::memcpy(data, &tglp, sizeof(tglp));
        }

        current_offset += section_header.section_size;
    }
}

} // namespace Service::APT::BCFNT
