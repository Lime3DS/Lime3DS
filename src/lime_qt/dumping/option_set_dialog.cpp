// Copyright 2020 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <unordered_map>
#include <QCheckBox>
#include <QStringList>
#include "common/logging/log.h"
#include "common/string_util.h"
#include "lime_qt/dumping/option_set_dialog.h"
#include "ui_option_set_dialog.h"

static const std::unordered_map<AVOptionType, const char*> TypeNameMap{{
    {AV_OPT_TYPE_BOOL, QT_TR_NOOP("boolean")},
    {AV_OPT_TYPE_FLAGS, QT_TR_NOOP("flags")},
    {AV_OPT_TYPE_DURATION, QT_TR_NOOP("duration")},
    {AV_OPT_TYPE_INT, QT_TR_NOOP("int")},
    {AV_OPT_TYPE_UINT64, QT_TR_NOOP("uint64")},
    {AV_OPT_TYPE_INT64, QT_TR_NOOP("int64")},
    {AV_OPT_TYPE_DOUBLE, QT_TR_NOOP("double")},
    {AV_OPT_TYPE_FLOAT, QT_TR_NOOP("float")},
    {AV_OPT_TYPE_RATIONAL, QT_TR_NOOP("rational")},
    {AV_OPT_TYPE_PIXEL_FMT, QT_TR_NOOP("pixel format")},
    {AV_OPT_TYPE_SAMPLE_FMT, QT_TR_NOOP("sample format")},
    {AV_OPT_TYPE_COLOR, QT_TR_NOOP("color")},
    {AV_OPT_TYPE_IMAGE_SIZE, QT_TR_NOOP("image size")},
    {AV_OPT_TYPE_STRING, QT_TR_NOOP("string")},
    {AV_OPT_TYPE_DICT, QT_TR_NOOP("dictionary")},
    {AV_OPT_TYPE_VIDEO_RATE, QT_TR_NOOP("video rate")},
    {AV_OPT_TYPE_CHANNEL_LAYOUT, QT_TR_NOOP("channel layout")},
}};

static const std::unordered_map<AVOptionType, const char*> TypeDescriptionMap{{
    {AV_OPT_TYPE_DURATION, QT_TR_NOOP("[&lt;hours (integer)>:][&lt;minutes (integer):]&lt;seconds "
                                      "(decimal)> e.g. 03:00.5 (3min 500ms)")},
    {AV_OPT_TYPE_RATIONAL, QT_TR_NOOP("&lt;num>/&lt;den>")},
    {AV_OPT_TYPE_COLOR, QT_TR_NOOP("0xRRGGBBAA")},
    {AV_OPT_TYPE_IMAGE_SIZE, QT_TR_NOOP("&lt;width>x&lt;height>, or preset values like 'vga'.")},
    {AV_OPT_TYPE_DICT,
     QT_TR_NOOP("Comma-splitted list of &lt;key>=&lt;value>. Do not put spaces.")},
    {AV_OPT_TYPE_VIDEO_RATE, QT_TR_NOOP("&lt;num>/&lt;den>, or preset values like 'pal'.")},
    {AV_OPT_TYPE_CHANNEL_LAYOUT, QT_TR_NOOP("Hexadecimal channel layout mask starting with '0x'.")},
}};

/// Get the preset values of an option. returns {display value, real value}
std::vector<std::pair<QString, QString>> GetPresetValues(const VideoDumper::OptionInfo& option) {
    switch (option.type) {
    case AV_OPT_TYPE_BOOL: {
        return {{QObject::tr("auto"), QStringLiteral("auto")},
                {QObject::tr("true"), QStringLiteral("true")},
                {QObject::tr("false"), QStringLiteral("false")}};
    }
    case AV_OPT_TYPE_PIXEL_FMT: {
        std::vector<std::pair<QString, QString>> out{{QObject::tr("none"), QStringLiteral("none")}};
        for (const auto& name : VideoDumper::GetPixelFormats()) {
            out.emplace_back(QString::fromUtf8(name), QString::fromUtf8(name));
        }
        return out;
    }
    case AV_OPT_TYPE_SAMPLE_FMT: {
        std::vector<std::pair<QString, QString>> out{{QObject::tr("none"), QStringLiteral("none")}};
        for (const auto& name : VideoDumper::GetSampleFormats()) {
            out.emplace_back(QString::fromUtf8(name), QString::fromUtf8(name));
        }
        return out;
    }
    case AV_OPT_TYPE_INT:
    case AV_OPT_TYPE_INT64:
    case AV_OPT_TYPE_UINT64: {
        std::vector<std::pair<QString, QString>> out;
        // Add in all named constants
        for (const auto& constant : option.named_constants) {
            out.emplace_back(QObject::tr("%1 (0x%2)")
                                 .arg(QString::fromStdString(constant.name))
                                 .arg(constant.value, 0, 16),
                             QString::fromStdString(constant.name));
        }
        return out;
    }
    default:
        return {};
    }
}

void OptionSetDialog::InitializeUI(const std::string& initial_value) {
    const QString type_name =
        TypeNameMap.count(option.type) ? tr(TypeNameMap.at(option.type)) : tr("unknown");
    ui->nameLabel->setText(tr("%1 &lt;%2> %3")
                               .arg(QString::fromStdString(option.name), type_name,
                                    QString::fromStdString(option.description)));
    if (TypeDescriptionMap.count(option.type)) {
        ui->formatLabel->setVisible(true);
        ui->formatLabel->setText(tr(TypeDescriptionMap.at(option.type)));
    }

    if (option.type == AV_OPT_TYPE_INT || option.type == AV_OPT_TYPE_INT64 ||
        option.type == AV_OPT_TYPE_UINT64 || option.type == AV_OPT_TYPE_FLOAT ||
        option.type == AV_OPT_TYPE_DOUBLE || option.type == AV_OPT_TYPE_DURATION ||
        option.type == AV_OPT_TYPE_RATIONAL) { // scalar types

        ui->formatLabel->setVisible(true);
        if (!ui->formatLabel->text().isEmpty()) {
            ui->formatLabel->text().append(QStringLiteral("\n"));
        }
        ui->formatLabel->setText(
            ui->formatLabel->text().append(tr("Range: %1 - %2").arg(option.min).arg(option.max)));
    }

    // Decide and initialize layout
    if (option.type == AV_OPT_TYPE_BOOL || option.type == AV_OPT_TYPE_PIXEL_FMT ||
        option.type == AV_OPT_TYPE_SAMPLE_FMT ||
        ((option.type == AV_OPT_TYPE_INT || option.type == AV_OPT_TYPE_INT64 ||
          option.type == AV_OPT_TYPE_UINT64) &&
         !option.named_constants.empty())) { // Use the combobox layout

        layout_type = 1;
        ui->comboBox->setVisible(true);
        ui->comboBoxHelpLabel->setVisible(true);

        QString real_initial_value = QString::fromStdString(initial_value);
        if (option.type == AV_OPT_TYPE_INT || option.type == AV_OPT_TYPE_INT64 ||
            option.type == AV_OPT_TYPE_UINT64) {

            // Get the name of the initial value
            try {
                s64 initial_value_integer = std::stoll(initial_value, nullptr, 0);
                for (const auto& constant : option.named_constants) {
                    if (constant.value == initial_value_integer) {
                        real_initial_value = QString::fromStdString(constant.name);
                        break;
                    }
                }
            } catch (...) {
                // Not convertible to integer, ignore
            }
        }

        bool found = false;
        for (const auto& [display, value] : GetPresetValues(option)) {
            ui->comboBox->addItem(display, value);
            if (value == real_initial_value) {
                found = true;
                ui->comboBox->setCurrentIndex(ui->comboBox->count() - 1);
            }
        }
        ui->comboBox->addItem(tr("custom"));

        if (!found) {
            ui->comboBox->setCurrentIndex(ui->comboBox->count() - 1);
            ui->lineEdit->setText(QString::fromStdString(initial_value));
        }

        UpdateUIDisplay();

        connect(ui->comboBox, &QComboBox::currentTextChanged, this,
                &OptionSetDialog::UpdateUIDisplay);
    } else if (option.type == AV_OPT_TYPE_FLAGS &&
               !option.named_constants.empty()) { // Use the check boxes layout

        layout_type = 2;

        for (const auto& constant : option.named_constants) {
            auto* checkBox = new QCheckBox(tr("%1 (0x%2) %3")
                                               .arg(QString::fromStdString(constant.name))
                                               .arg(constant.value, 0, 16)
                                               .arg(QString::fromStdString(constant.description)));
            checkBox->setProperty("value", static_cast<unsigned long long>(constant.value));
            checkBox->setProperty("name", QString::fromStdString(constant.name));
            ui->checkBoxLayout->addWidget(checkBox);
        }
        SetCheckBoxDefaults(initial_value);
    } else { // Use the line edit layout
        layout_type = 0;
        ui->lineEdit->setVisible(true);
        ui->lineEdit->setText(QString::fromStdString(initial_value));
    }

    adjustSize();
}

void OptionSetDialog::SetCheckBoxDefaults(const std::string& initial_value) {
    if (initial_value.size() >= 2 &&
        (initial_value.substr(0, 2) == "0x" || initial_value.substr(0, 2) == "0X")) {
        // This is a hex mask
        try {
            u64 value = std::stoull(initial_value, nullptr, 16);
            for (int i = 0; i < ui->checkBoxLayout->count(); ++i) {
                auto* checkBox = qobject_cast<QCheckBox*>(ui->checkBoxLayout->itemAt(i)->widget());
                if (checkBox) {
                    checkBox->setChecked(value & checkBox->property("value").toULongLong());
                }
            }
        } catch (...) {
            LOG_ERROR(Frontend, "Could not convert {} to number", initial_value);
        }
    } else {
        // This is a combination of constants, splitted with + or |
        const auto tmp = Common::SplitString(initial_value, '+');

        std::vector<std::string> out;
        for (const auto& str : tmp) {
            const auto tmp2 = Common::SplitString(str, '|');
            out.insert(out.end(), tmp2.begin(), tmp2.end());
        }
        for (int i = 0; i < ui->checkBoxLayout->count(); ++i) {
            auto* checkBox = qobject_cast<QCheckBox*>(ui->checkBoxLayout->itemAt(i)->widget());
            if (checkBox) {
                checkBox->setChecked(
                    std::find(out.begin(), out.end(),
                              checkBox->property("name").toString().toStdString()) != out.end());
            }
        }
    }
}

void OptionSetDialog::UpdateUIDisplay() {
    if (layout_type != 1)
        return;

    if (ui->comboBox->currentIndex() == ui->comboBox->count() - 1) { // custom
        ui->comboBoxHelpLabel->setVisible(false);
        ui->lineEdit->setVisible(true);
        adjustSize();
        return;
    }

    ui->lineEdit->setVisible(false);
    for (const auto& constant : option.named_constants) {
        if (constant.name == ui->comboBox->currentData().toString().toStdString()) {
            ui->comboBoxHelpLabel->setVisible(true);
            ui->comboBoxHelpLabel->setText(QString::fromStdString(constant.description));
            return;
        }
    }
}

std::pair<bool, std::string> OptionSetDialog::GetCurrentValue() {
    if (!is_set) {
        return {};
    }

    switch (layout_type) {
    case 0: // line edit layout
        return {true, ui->lineEdit->text().toStdString()};
    case 1: // combo box layout
        if (ui->comboBox->currentIndex() == ui->comboBox->count() - 1) {
            return {true, ui->lineEdit->text().toStdString()}; // custom
        }
        return {true, ui->comboBox->currentData().toString().toStdString()};
    case 2: { // check boxes layout
        std::string out;
        for (int i = 0; i < ui->checkBoxLayout->count(); ++i) {
            auto* checkBox = qobject_cast<QCheckBox*>(ui->checkBoxLayout->itemAt(i)->widget());
            if (checkBox && checkBox->isChecked()) {
                if (!out.empty()) {
                    out.append("+");
                }
                out.append(checkBox->property("name").toString().toStdString());
            }
        }
        if (out.empty()) {
            out = "0x0";
        }
        return {true, out};
    }
    default:
        return {};
    }
}

OptionSetDialog::OptionSetDialog(QWidget* parent, VideoDumper::OptionInfo option_,
                                 const std::string& initial_value)
    : QDialog(parent), ui(std::make_unique<Ui::OptionSetDialog>()), option(std::move(option_)) {

    ui->setupUi(this);
    InitializeUI(initial_value);

    connect(ui->unsetButton, &QPushButton::clicked, [this] {
        is_set = false;
        accept();
    });
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &OptionSetDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &OptionSetDialog::reject);
}

OptionSetDialog::~OptionSetDialog() = default;
