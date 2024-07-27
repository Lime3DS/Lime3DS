// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "common/common_types.h"
#include "core/loader/loader.h"

namespace Loader {

/// Loads an ELF/AXF file
class AppLoader_ELF final : public AppLoader {
public:
    AppLoader_ELF(Core::System& system_, FileUtil::IOFile&& file, std::string filename)
        : AppLoader(system_, std::move(file)), filename(std::move(filename)) {}

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

private:
    std::string filename;
};

} // namespace Loader
