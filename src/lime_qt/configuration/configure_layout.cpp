// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QColorDialog>
#include "common/settings.h"
#include "lime_qt/configuration/configuration_shared.h"
#include "lime_qt/configuration/configure_layout.h"
#include "ui_configure_layout.h"
#ifdef ENABLE_OPENGL
#include "video_core/renderer_opengl/post_processing_opengl.h"
#endif

ConfigureLayout::ConfigureLayout(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureLayout>()) {
    ui->setupUi(this);

    SetupPerGameUI();
    SetConfiguration();

    ui->layout_group->setEnabled(!Settings::values.custom_layout);

    ui->custom_layout_group->setEnabled(
        (Settings::values.layout_option.GetValue() == Settings::LayoutOption::CustomLayout));
    connect(ui->layout_combobox,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this](int currentIndex) {
                ui->custom_layout_group->setEnabled(ui->layout_combobox->currentIndex() == 5);
            });

    connect(ui->bg_button, &QPushButton::clicked, this, [this] {
        const QColor new_bg_color = QColorDialog::getColor(bg_color);
        if (!new_bg_color.isValid()) {
            return;
        }
        bg_color = new_bg_color;
        QPixmap pixmap(ui->bg_button->size());
        pixmap.fill(bg_color);
        const QIcon color_icon(pixmap);
        ui->bg_button->setIcon(color_icon);
    });
}

ConfigureLayout::~ConfigureLayout() = default;

void ConfigureLayout::SetConfiguration() {

    if (!Settings::IsConfiguringGlobal()) {
        ConfigurationShared::SetPerGameSetting(ui->layout_combobox,
                                               &Settings::values.layout_option);
    } else {
        ui->layout_combobox->setCurrentIndex(
            static_cast<int>(Settings::values.layout_option.GetValue()));
    }

    ui->toggle_swap_screen->setChecked(Settings::values.swap_screen.GetValue());
    ui->toggle_upright_screen->setChecked(Settings::values.upright_screen.GetValue());
    ui->large_screen_proportion->setValue(Settings::values.large_screen_proportion.GetValue());
    ui->custom_top_left->setValue(Settings::values.custom_top_left.GetValue());
    ui->custom_top_top->setValue(Settings::values.custom_top_top.GetValue());
    ui->custom_top_right->setValue(Settings::values.custom_top_right.GetValue());
    ui->custom_top_bottom->setValue(Settings::values.custom_top_bottom.GetValue());
    ui->custom_bottom_left->setValue(Settings::values.custom_bottom_left.GetValue());
    ui->custom_bottom_top->setValue(Settings::values.custom_bottom_top.GetValue());
    ui->custom_bottom_right->setValue(Settings::values.custom_bottom_right.GetValue());
    ui->custom_bottom_bottom->setValue(Settings::values.custom_bottom_bottom.GetValue());
    ui->custom_second_layer_opacity->setValue(
        Settings::values.custom_second_layer_opacity.GetValue());
    bg_color =
        QColor::fromRgbF(Settings::values.bg_red.GetValue(), Settings::values.bg_green.GetValue(),
                         Settings::values.bg_blue.GetValue());
    QPixmap pixmap(ui->bg_button->size());
    pixmap.fill(bg_color);
    const QIcon color_icon(pixmap);
    ui->bg_button->setIcon(color_icon);
}

void ConfigureLayout::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureLayout::ApplyConfiguration() {
    Settings::values.large_screen_proportion = ui->large_screen_proportion->value();

    Settings::values.custom_top_left = ui->custom_top_left->value();
    Settings::values.custom_top_top = ui->custom_top_top->value();
    Settings::values.custom_top_right = ui->custom_top_right->value();
    Settings::values.custom_top_bottom = ui->custom_top_bottom->value();
    Settings::values.custom_bottom_left = ui->custom_bottom_left->value();
    Settings::values.custom_bottom_top = ui->custom_bottom_top->value();
    Settings::values.custom_bottom_right = ui->custom_bottom_right->value();
    Settings::values.custom_bottom_bottom = ui->custom_bottom_bottom->value();
    Settings::values.custom_second_layer_opacity = ui->custom_second_layer_opacity->value();

    ConfigurationShared::ApplyPerGameSetting(&Settings::values.layout_option, ui->layout_combobox);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.swap_screen, ui->toggle_swap_screen,
                                             swap_screen);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.upright_screen,
                                             ui->toggle_upright_screen, upright_screen);

    Settings::values.bg_red = static_cast<float>(bg_color.redF());
    Settings::values.bg_green = static_cast<float>(bg_color.greenF());
    Settings::values.bg_blue = static_cast<float>(bg_color.blueF());
}

void ConfigureLayout::SetupPerGameUI() {
    // Block the global settings if a game is currently running that overrides them
    if (Settings::IsConfiguringGlobal()) {
        ui->toggle_swap_screen->setEnabled(Settings::values.swap_screen.UsingGlobal());
        ui->toggle_upright_screen->setEnabled(Settings::values.upright_screen.UsingGlobal());
        return;
    }

    ui->bg_color_group->setVisible(false);

    ConfigurationShared::SetColoredTristate(ui->toggle_swap_screen, Settings::values.swap_screen,
                                            swap_screen);
    ConfigurationShared::SetColoredTristate(ui->toggle_upright_screen,
                                            Settings::values.upright_screen, upright_screen);

    ConfigurationShared::SetColoredComboBox(
        ui->layout_combobox, ui->widget_layout,
        static_cast<int>(Settings::values.layout_option.GetValue(true)));
}
