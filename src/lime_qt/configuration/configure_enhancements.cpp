// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QColorDialog>
#include "common/settings.h"
#include "lime_qt/configuration/configuration_shared.h"
#include "lime_qt/configuration/configure_enhancements.h"
#include "ui_configure_enhancements.h"
#ifdef ENABLE_OPENGL
#include "video_core/renderer_opengl/post_processing_opengl.h"
#endif

ConfigureEnhancements::ConfigureEnhancements(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureEnhancements>()) {
    ui->setupUi(this);

    SetupPerGameUI();
    SetConfiguration();

    const auto graphics_api = Settings::values.graphics_api.GetValue();
    const bool res_scale_enabled = graphics_api != Settings::GraphicsAPI::Software;
    ui->resolution_factor_combobox->setEnabled(res_scale_enabled);

    connect(ui->render_3d_combobox, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int currentIndex) {
                updateShaders(static_cast<Settings::StereoRenderOption>(currentIndex));
            });

    ui->toggle_preload_textures->setEnabled(ui->toggle_custom_textures->isChecked());
    ui->toggle_async_custom_loading->setEnabled(ui->toggle_custom_textures->isChecked());
    connect(ui->toggle_custom_textures, &QCheckBox::toggled, this, [this] {
        ui->toggle_preload_textures->setEnabled(ui->toggle_custom_textures->isChecked());
        ui->toggle_async_custom_loading->setEnabled(ui->toggle_custom_textures->isChecked());
        if (!ui->toggle_preload_textures->isEnabled())
            ui->toggle_preload_textures->setChecked(false);
    });
}

ConfigureEnhancements::~ConfigureEnhancements() = default;

void ConfigureEnhancements::SetConfiguration() {

    if (!Settings::IsConfiguringGlobal()) {
        ConfigurationShared::SetPerGameSetting(ui->resolution_factor_combobox,
                                               &Settings::values.resolution_factor);
        ConfigurationShared::SetPerGameSetting(ui->texture_filter_combobox,
                                               &Settings::values.texture_filter);
        ConfigurationShared::SetHighlight(ui->widget_texture_filter,
                                          !Settings::values.texture_filter.UsingGlobal());
    } else {
        ui->resolution_factor_combobox->setCurrentIndex(
            Settings::values.resolution_factor.GetValue());
        ui->texture_filter_combobox->setCurrentIndex(
            static_cast<int>(Settings::values.texture_filter.GetValue()));
    }

    ui->render_3d_combobox->setCurrentIndex(
        static_cast<int>(Settings::values.render_3d.GetValue()));
    ui->factor_3d->setValue(Settings::values.factor_3d.GetValue());
    ui->mono_rendering_eye->setCurrentIndex(
        static_cast<int>(Settings::values.mono_render_option.GetValue()));
    updateShaders(Settings::values.render_3d.GetValue());
    ui->toggle_linear_filter->setChecked(Settings::values.filter_mode.GetValue());
    ui->toggle_dump_textures->setChecked(Settings::values.dump_textures.GetValue());
    ui->toggle_custom_textures->setChecked(Settings::values.custom_textures.GetValue());
    ui->toggle_preload_textures->setChecked(Settings::values.preload_textures.GetValue());
    ui->toggle_async_custom_loading->setChecked(Settings::values.async_custom_loading.GetValue());
}

void ConfigureEnhancements::updateShaders(Settings::StereoRenderOption stereo_option) {
    ui->shader_combobox->clear();
    ui->shader_combobox->setEnabled(true);

    if (stereo_option == Settings::StereoRenderOption::Interlaced ||
        stereo_option == Settings::StereoRenderOption::ReverseInterlaced) {
        ui->shader_combobox->addItem(QStringLiteral("horizontal (builtin)"));
        ui->shader_combobox->setCurrentIndex(0);
        ui->shader_combobox->setEnabled(false);
        return;
    }

    std::string current_shader;
    if (stereo_option == Settings::StereoRenderOption::Anaglyph) {
        ui->shader_combobox->addItem(QStringLiteral("dubois (builtin)"));
        current_shader = Settings::values.anaglyph_shader_name.GetValue();
    } else {
        ui->shader_combobox->addItem(QStringLiteral("none (builtin)"));
        current_shader = Settings::values.pp_shader_name.GetValue();
    }

    ui->shader_combobox->setCurrentIndex(0);

#ifdef ENABLE_OPENGL
    for (const auto& shader : OpenGL::GetPostProcessingShaderList(
             stereo_option == Settings::StereoRenderOption::Anaglyph)) {
        ui->shader_combobox->addItem(QString::fromStdString(shader));
        if (current_shader == shader)
            ui->shader_combobox->setCurrentIndex(ui->shader_combobox->count() - 1);
    }
#endif
}

void ConfigureEnhancements::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureEnhancements::ApplyConfiguration() {
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.resolution_factor,
                                             ui->resolution_factor_combobox);
    Settings::values.render_3d =
        static_cast<Settings::StereoRenderOption>(ui->render_3d_combobox->currentIndex());
    Settings::values.factor_3d = ui->factor_3d->value();
    Settings::values.mono_render_option =
        static_cast<Settings::MonoRenderOption>(ui->mono_rendering_eye->currentIndex());
    if (Settings::values.render_3d.GetValue() == Settings::StereoRenderOption::Anaglyph) {
        Settings::values.anaglyph_shader_name =
            ui->shader_combobox->itemText(ui->shader_combobox->currentIndex()).toStdString();
    } else if (Settings::values.render_3d.GetValue() == Settings::StereoRenderOption::Off) {
        Settings::values.pp_shader_name =
            ui->shader_combobox->itemText(ui->shader_combobox->currentIndex()).toStdString();
    }

    ConfigurationShared::ApplyPerGameSetting(&Settings::values.filter_mode,
                                             ui->toggle_linear_filter, linear_filter);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.texture_filter,
                                             ui->texture_filter_combobox);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.dump_textures,
                                             ui->toggle_dump_textures, dump_textures);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.custom_textures,
                                             ui->toggle_custom_textures, custom_textures);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.preload_textures,
                                             ui->toggle_preload_textures, preload_textures);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.async_custom_loading,
                                             ui->toggle_async_custom_loading, async_custom_loading);
}

void ConfigureEnhancements::SetupPerGameUI() {
    // Block the global settings if a game is currently running that overrides them
    if (Settings::IsConfiguringGlobal()) {
        ui->widget_resolution->setEnabled(Settings::values.resolution_factor.UsingGlobal());
        ui->widget_texture_filter->setEnabled(Settings::values.texture_filter.UsingGlobal());
        ui->toggle_linear_filter->setEnabled(Settings::values.filter_mode.UsingGlobal());
        ui->toggle_dump_textures->setEnabled(Settings::values.dump_textures.UsingGlobal());
        ui->toggle_custom_textures->setEnabled(Settings::values.custom_textures.UsingGlobal());
        ui->toggle_preload_textures->setEnabled(Settings::values.preload_textures.UsingGlobal());
        ui->toggle_async_custom_loading->setEnabled(
            Settings::values.async_custom_loading.UsingGlobal());
        return;
    }

    ui->stereo_group->setVisible(false);
    ui->widget_shader->setVisible(false);

    ConfigurationShared::SetColoredTristate(ui->toggle_linear_filter, Settings::values.filter_mode,
                                            linear_filter);
    ConfigurationShared::SetColoredTristate(ui->toggle_dump_textures,
                                            Settings::values.dump_textures, dump_textures);
    ConfigurationShared::SetColoredTristate(ui->toggle_custom_textures,
                                            Settings::values.custom_textures, custom_textures);
    ConfigurationShared::SetColoredTristate(ui->toggle_preload_textures,
                                            Settings::values.preload_textures, preload_textures);
    ConfigurationShared::SetColoredTristate(ui->toggle_async_custom_loading,
                                            Settings::values.async_custom_loading,
                                            async_custom_loading);

    ConfigurationShared::SetColoredComboBox(
        ui->resolution_factor_combobox, ui->widget_resolution,
        static_cast<int>(Settings::values.resolution_factor.GetValue(true)));

    ConfigurationShared::SetColoredComboBox(
        ui->texture_filter_combobox, ui->widget_texture_filter,
        static_cast<int>(Settings::values.texture_filter.GetValue(true)));
}
