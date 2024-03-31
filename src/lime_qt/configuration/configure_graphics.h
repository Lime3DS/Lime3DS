// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <span>
#include <QString>
#include <QWidget>

namespace Ui {
class ConfigureGraphics;
}

namespace ConfigurationShared {
enum class CheckState;
}

class ConfigureGraphics : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureGraphics(QString gl_renderer, std::span<const QString> physical_devices,
                               bool is_powered_on, QWidget* parent = nullptr);
    ~ConfigureGraphics() override;

    void ApplyConfiguration();
    void RetranslateUI();
    void SetConfiguration();

    void UpdateBackgroundColorButton(const QColor& color);

private:
    void SetupPerGameUI();
    void SetPhysicalDeviceComboVisibility(int index);

    ConfigurationShared::CheckState use_hw_shader;
    ConfigurationShared::CheckState shaders_accurate_mul;
    ConfigurationShared::CheckState use_disk_shader_cache;
    ConfigurationShared::CheckState use_vsync_new;
    ConfigurationShared::CheckState async_shader_compilation;
    ConfigurationShared::CheckState async_presentation;
    ConfigurationShared::CheckState spirv_shader_gen;
    std::unique_ptr<Ui::ConfigureGraphics> ui;
    QColor bg_color;
};
