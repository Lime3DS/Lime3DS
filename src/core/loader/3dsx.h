// Copyright 2014 Dolphin Emulator Project / Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "common/common_types.h"
#include "core/loader/loader.h"

namespace Loader {

/// Loads an 3DSX file
class AppLoader_THREEDSX final : public AppLoader {
public:
    AppLoader_THREEDSX(Core::System& system_, FileUtil::IOFile&& file, const std::string& filename,
                       const std::string& filepath)
        : AppLoader(system_, std::move(file)), filename(filename), filepath(filepath) {}

    /**
     * Returns the type of the file
     * @param file FileUtil::IOFile open file
     * @return FileType found, or FileType::Error if this loader doesn't know it
     */
    static FileType IdentifyType(FileUtil::IOFile& file);

    FileType GetFileType() override {
        return IdentifyType(file);
    }

    ResultStatus Load(std::shared_ptr<Kernel::Process>& process) override;

    ResultStatus ReadIcon(std::vector<u8>& buffer) override;

    ResultStatus ReadRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) override;

private:
    std::string filename;
    std::string filepath;
};

} // namespace Loader
