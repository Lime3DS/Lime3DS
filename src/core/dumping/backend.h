// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include "audio_core/audio_types.h"
#include "common/common_types.h"
#include "core/frontend/framebuffer_layout.h"

namespace VideoDumper {
/**
 * Frame dump data for a single screen
 * data is in RGB888 format, left to right then top to bottom
 */
class VideoFrame {
public:
    std::size_t width;
    std::size_t height;
    u32 stride;
    std::vector<u8> data;

    VideoFrame(std::size_t width_ = 0, std::size_t height_ = 0, u8* data_ = nullptr);
};

class Backend {
public:
    virtual ~Backend();
    virtual bool StartDumping(const std::string& path, const Layout::FramebufferLayout& layout) = 0;
    virtual void AddVideoFrame(VideoFrame frame) = 0;
    virtual void AddAudioFrame(AudioCore::StereoFrame16 frame) = 0;
    virtual void AddAudioSample(const std::array<s16, 2>& sample) = 0;
    virtual void StopDumping() = 0;
    virtual bool IsDumping() const = 0;
    virtual Layout::FramebufferLayout GetLayout() const = 0;
};

class NullBackend : public Backend {
public:
    ~NullBackend() override;
    bool StartDumping(const std::string& /*path*/,
                      const Layout::FramebufferLayout& /*layout*/) override {
        return false;
    }
    void AddVideoFrame(VideoFrame /*frame*/) override {}
    void AddAudioFrame(AudioCore::StereoFrame16 /*frame*/) override {}
    void AddAudioSample(const std::array<s16, 2>& /*sample*/) override {}
    void StopDumping() override {}
    bool IsDumping() const override {
        return false;
    }
    Layout::FramebufferLayout GetLayout() const override {
        return Layout::FramebufferLayout{};
    }
};
} // namespace VideoDumper
