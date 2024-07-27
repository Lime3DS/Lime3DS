// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <functional>
#include "common/common_types.h"

namespace Pica {
struct OutputVertex;
}

namespace Pica {
struct DisplayTransferConfig;
struct MemoryFillConfig;
} // namespace Pica

namespace VideoCore {

enum class LoadCallbackStage {
    Prepare,
    Preload,
    Decompile,
    Build,
    Complete,
};
using DiskResourceLoadCallback = std::function<void(LoadCallbackStage, std::size_t, std::size_t)>;

class RasterizerInterface {
public:
    virtual ~RasterizerInterface() = default;

    /// Queues the primitive formed by the given vertices for rendering
    virtual void AddTriangle(const Pica::OutputVertex& v0, const Pica::OutputVertex& v1,
                             const Pica::OutputVertex& v2) = 0;

    /// Draw the current batch of triangles
    virtual void DrawTriangles() = 0;

    /// Notify rasterizer that the specified PICA register has been changed
    virtual void NotifyPicaRegisterChanged(u32 id) = 0;

    /// Notify rasterizer that all caches should be flushed to 3DS memory
    virtual void FlushAll() = 0;

    /// Notify rasterizer that any caches of the specified region should be flushed to 3DS memory
    virtual void FlushRegion(PAddr addr, u32 size) = 0;

    /// Notify rasterizer that any caches of the specified region should be invalidated
    virtual void InvalidateRegion(PAddr addr, u32 size) = 0;

    /// Notify rasterizer that any caches of the specified region should be flushed to 3DS memory
    /// and invalidated
    virtual void FlushAndInvalidateRegion(PAddr addr, u32 size) = 0;

    /// Removes as much state as possible from the rasterizer in preparation for a save/load state
    virtual void ClearAll(bool flush) = 0;

    /// Attempt to use a faster method to perform a display transfer with is_texture_copy = 0
    virtual bool AccelerateDisplayTransfer(const Pica::DisplayTransferConfig&) {
        return false;
    }

    /// Attempt to use a faster method to perform a display transfer with is_texture_copy = 1
    virtual bool AccelerateTextureCopy(const Pica::DisplayTransferConfig&) {
        return false;
    }

    /// Attempt to use a faster method to fill a region
    virtual bool AccelerateFill(const Pica::MemoryFillConfig&) {
        return false;
    }

    /// Attempt to draw using hardware shaders
    virtual bool AccelerateDrawBatch([[maybe_unused]] bool is_indexed) {
        return false;
    }

    virtual void LoadDiskResources([[maybe_unused]] const std::atomic_bool& stop_loading,
                                   [[maybe_unused]] const DiskResourceLoadCallback& callback) {}

    virtual void SyncEntireState() {}
};
} // namespace VideoCore
