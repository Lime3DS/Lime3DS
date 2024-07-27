// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <list>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include "common/thread_worker.h"
#include "video_core/custom_textures/material.h"
#include "video_core/rasterizer_interface.h"

namespace Core {
class System;
}

namespace FileUtil {
struct FSTEntry;
}

namespace VideoCore {

class SurfaceParams;

struct AsyncUpload {
    const Material* material;
    std::function<bool()> func;
};

class CustomTexManager {
public:
    explicit CustomTexManager(Core::System& system);
    ~CustomTexManager();

    /// Processes queued texture uploads
    void TickFrame();

    /// Searches the load directory assigned to program_id for any custom textures and loads them
    void FindCustomTextures();

    /// Reads the pack configuration file
    bool ReadConfig(u64 title_id, bool options_only = false);

    /// Saves the pack configuration file template to the dump directory if it doesn't exist.
    void PrepareDumping(u64 title_id);

    /// Preloads all registered custom textures
    void PreloadTextures(const std::atomic_bool& stop_run,
                         const VideoCore::DiskResourceLoadCallback& callback);

    /// Saves the provided pixel data described by params to disk as png
    void DumpTexture(const SurfaceParams& params, u32 level, std::span<u8> data, u64 data_hash);

    /// Returns the material assigned to the provided data hash
    Material* GetMaterial(u64 data_hash);

    /// Decodes the textures in material to a consumable format and uploads it.
    bool Decode(Material* material, std::function<bool()>&& upload);

    /// True when mipmap uploads should be skipped (legacy packs only)
    bool SkipMipmaps() const noexcept {
        return skip_mipmap;
    }

    /// Returns true if the pack uses the new hashing method.
    bool UseNewHash() const noexcept {
        return use_new_hash;
    }

private:
    /// Parses the custom texture filename (hash, material type, etc).
    bool ParseFilename(const FileUtil::FSTEntry& file, CustomTexture* texture);

    /// Returns a vector of all custom texture files.
    std::vector<FileUtil::FSTEntry> GetTextures(u64 title_id);

    /// Creates the thread workers.
    void CreateWorkers();

private:
    Core::System& system;
    Frontend::ImageInterface& image_interface;
    std::unordered_set<u64> dumped_textures;
    std::unordered_map<u64, std::unique_ptr<Material>> material_map;
    std::unordered_map<std::string, std::vector<u64>> path_to_hash_map;
    std::vector<std::unique_ptr<CustomTexture>> custom_textures;
    std::list<AsyncUpload> async_uploads;
    std::unique_ptr<Common::ThreadWorker> workers;
    bool textures_loaded{false};
    bool async_custom_loading{true};
    bool skip_mipmap{false};
    bool flip_png_files{true};
    bool use_new_hash{true};
};

} // namespace VideoCore
