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
                ui->custom_layout_group->setEnabled(ui->layout_combobox->currentIndex() ==
                                                    (uint)(Settings::LayoutOption::CustomLayout));
            });

    ui->large_screen_proportion->setEnabled(
        (Settings::values.layout_option.GetValue() == Settings::LayoutOption::LargeScreen));
    connect(
        ui->layout_combobox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int currentIndex) {
            ui->large_screen_proportion->setEnabled(ui->layout_combobox->currentIndex() ==
                                                    (uint)(Settings::LayoutOption::LargeScreen));
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
    ui->custom_top_x->setValue(Settings::values.custom_top_x.GetValue());
    ui->custom_top_y->setValue(Settings::values.custom_top_y.GetValue());
    ui->custom_top_width->setValue(Settings::values.custom_top_width.GetValue());
    ui->custom_top_height->setValue(Settings::values.custom_top_height.GetValue());
    ui->custom_bottom_x->setValue(Settings::values.custom_bottom_x.GetValue());
    ui->custom_bottom_y->setValue(Settings::values.custom_bottom_y.GetValue());
    ui->custom_bottom_width->setValue(Settings::values.custom_bottom_width.GetValue());
    ui->custom_bottom_height->setValue(Settings::values.custom_bottom_height.GetValue());
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

    Settings::values.custom_top_x = ui->custom_top_x->value();
    Settings::values.custom_top_y = ui->custom_top_y->value();
    Settings::values.custom_top_width = ui->custom_top_width->value();
    Settings::values.custom_top_height = ui->custom_top_height->value();
    Settings::values.custom_bottom_x = ui->custom_bottom_x->value();
    Settings::values.custom_bottom_y = ui->custom_bottom_y->value();
    Settings::values.custom_bottom_width = ui->custom_bottom_width->value();
    Settings::values.custom_bottom_height = ui->custom_bottom_height->value();
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
