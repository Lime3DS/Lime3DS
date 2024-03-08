// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QDirIterator>
#include "citra_qt/configuration/configure_ui.h"
#include "citra_qt/uisettings.h"
#include "ui_configure_ui.h"

ConfigureUi::ConfigureUi(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureUi>()) {
    ui->setupUi(this);
    InitializeLanguageComboBox();

    for (const auto& theme : UISettings::themes) {
        ui->theme_combobox->addItem(QString::fromUtf8(theme.first),
                                    QString::fromUtf8(theme.second));
    }

    SetConfiguration();
}

ConfigureUi::~ConfigureUi() = default;

void ConfigureUi::InitializeLanguageComboBox() {
    ui->language_combobox->addItem(tr("<System>"), QString{});
    ui->language_combobox->addItem(tr("English"), QStringLiteral("en"));
    QDirIterator it(QStringLiteral(":/languages"), QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        QString locale = it.next();
        locale.truncate(locale.lastIndexOf(QLatin1Char{'.'}));
        locale.remove(0, locale.lastIndexOf(QLatin1Char{'/'}) + 1);
        const QString lang = QLocale::languageToString(QLocale(locale).language());
        const QString country = QLocale::territoryToString(QLocale(locale).territory());
        ui->language_combobox->addItem(QStringLiteral("%1 (%2)").arg(lang, country), locale);
    }

    // Unlike other configuration changes, interface language changes need to be reflected on the
    // interface immediately. This is done by passing a signal to the main window, and then
    // retranslating when passing back.
    connect(ui->language_combobox, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &ConfigureUi::OnLanguageChanged);
}

void ConfigureUi::SetConfiguration() {
    ui->theme_combobox->setCurrentIndex(ui->theme_combobox->findData(UISettings::values.theme));
    ui->language_combobox->setCurrentIndex(
        ui->language_combobox->findData(UISettings::values.language));
    ui->icon_size_combobox->setCurrentIndex(
        static_cast<int>(UISettings::values.game_list_icon_size.GetValue()));
    ui->row_1_text_combobox->setCurrentIndex(
        static_cast<int>(UISettings::values.game_list_row_1.GetValue()));
    ui->row_2_text_combobox->setCurrentIndex(
        static_cast<int>(UISettings::values.game_list_row_2.GetValue()) + 1);
    ui->toggle_hide_no_icon->setChecked(UISettings::values.game_list_hide_no_icon.GetValue());
    ui->toggle_single_line_mode->setChecked(
        UISettings::values.game_list_single_line_mode.GetValue());
}

void ConfigureUi::ApplyConfiguration() {
    UISettings::values.theme =
        ui->theme_combobox->itemData(ui->theme_combobox->currentIndex()).toString();
    UISettings::values.game_list_icon_size =
        static_cast<UISettings::GameListIconSize>(ui->icon_size_combobox->currentIndex());
    UISettings::values.game_list_row_1 =
        static_cast<UISettings::GameListText>(ui->row_1_text_combobox->currentIndex());
    UISettings::values.game_list_row_2 =
        static_cast<UISettings::GameListText>(ui->row_2_text_combobox->currentIndex() - 1);
    UISettings::values.game_list_hide_no_icon = ui->toggle_hide_no_icon->isChecked();
    UISettings::values.game_list_single_line_mode = ui->toggle_single_line_mode->isChecked();
}

void ConfigureUi::OnLanguageChanged(int index) {
    if (index == -1)
        return;

    emit LanguageChanged(ui->language_combobox->itemData(index).toString());
}

void ConfigureUi::RetranslateUI() {
    ui->retranslateUi(this);
}
