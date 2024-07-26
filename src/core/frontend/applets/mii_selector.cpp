// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/file_util.h"
#include "common/string_util.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/file_backend.h"
#include "core/frontend/applets/mii_selector.h"
#include "core/hle/service/ptm/ptm.h"

namespace Frontend {

void MiiSelector::Finalize(u32 return_code, Mii::MiiData mii) {
    data = {return_code, mii};
}

std::vector<Mii::MiiData> LoadMiis() {
    std::vector<Mii::MiiData> miis;

    std::string nand_directory{FileUtil::GetUserPath(FileUtil::UserPath::NANDDir)};
    FileSys::ArchiveFactory_ExtSaveData extdata_archive_factory(nand_directory,
                                                                FileSys::ExtSaveDataType::Shared);

    auto archive_result = extdata_archive_factory.Open(Service::PTM::ptm_shared_extdata_id, 0);
    if (archive_result.Succeeded()) {
        auto archive = std::move(archive_result).Unwrap();

        FileSys::Path file_path = "/CFL_DB.dat";
        FileSys::Mode mode{};
        mode.read_flag.Assign(1);

        auto file_result = archive->OpenFile(file_path, mode);
        if (file_result.Succeeded()) {
            auto file = std::move(file_result).Unwrap();

            u32 saved_miis_offset = 0x8;
            // The Mii Maker has a 100 Mii limit on the 3ds
            for (int i = 0; i < 100; ++i) {
                Mii::MiiData mii;
                std::array<u8, sizeof(mii)> mii_raw;
                file->Read(saved_miis_offset, sizeof(mii), mii_raw.data());
                std::memcpy(&mii, mii_raw.data(), sizeof(mii));
                if (mii.mii_id != 0u) {
                    miis.push_back(mii);
                }
                saved_miis_offset += sizeof(mii);
            }
        }
    }

    return miis;
}

void DefaultMiiSelector::Setup(const Frontend::MiiSelectorConfig& config) {
    MiiSelector::Setup(config);
    Finalize(0, HLE::Applets::MiiSelector::GetStandardMiiResult().selected_mii_data);
}

} // namespace Frontend
