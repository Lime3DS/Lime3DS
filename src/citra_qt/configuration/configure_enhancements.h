// Copyright 2019-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>
#include "common/common_types.h"

namespace Settings {
enum class StereoRenderOption : u32;
}

namespace ConfigurationShared {
enum class CheckState;
}

namespace Ui {
class ConfigureEnhancements;
}

class ConfigureEnhancements : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureEnhancements(QWidget* parent = nullptr);
    ~ConfigureEnhancements();

    void ApplyConfiguration();
    void RetranslateUI();
    void SetConfiguration();

    void SetupPerGameUI();

private:
    void updateShaders(Settings::StereoRenderOption stereo_option);
    void updateTextureFilter(int index);

    std::unique_ptr<Ui::ConfigureEnhancements> ui;
    ConfigurationShared::CheckState linear_filter;
    ConfigurationShared::CheckState scaling_mode;
    ConfigurationShared::CheckState dump_textures;
    ConfigurationShared::CheckState custom_textures;
    ConfigurationShared::CheckState preload_textures;
    ConfigurationShared::CheckState async_custom_loading;
    ConfigurationShared::CheckState disable_right_eye_render;
    QColor bg_color;
};
