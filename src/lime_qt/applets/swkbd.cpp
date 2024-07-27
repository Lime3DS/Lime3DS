// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <unordered_map>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QString>
#include <QVBoxLayout>
#include "lime_qt/applets/swkbd.h"

QtKeyboardValidator::QtKeyboardValidator(QtKeyboard* keyboard_) : keyboard(keyboard_) {}

QtKeyboardValidator::State QtKeyboardValidator::validate(QString& input, int& pos) const {
    if (keyboard->ValidateFilters(input.toStdString()) == Frontend::ValidationError::None) {
        if (input.size() > keyboard->config.max_text_length)
            return State::Invalid;
        return State::Acceptable;
    } else {
        return State::Invalid;
    }
}

QtKeyboardDialog::QtKeyboardDialog(QWidget* parent, QtKeyboard* keyboard_)
    : QDialog(parent), keyboard(keyboard_) {
    using namespace Frontend;
    const auto config = keyboard->config;
    layout = new QVBoxLayout;
    label = new QLabel(QString::fromStdString(config.hint_text));
    line_edit = new QLineEdit;
    line_edit->setValidator(new QtKeyboardValidator(keyboard));
    buttons = new QDialogButtonBox;
    // Initialize buttons
    switch (config.button_config) {
    case ButtonConfig::Triple:
        buttons->addButton(config.button_text[1].empty()
                               ? tr(SWKBD_BUTTON_FORGOT)
                               : QString::fromStdString(config.button_text[1]),
                           QDialogButtonBox::ButtonRole::HelpRole);
        [[fallthrough]];
    case ButtonConfig::Dual:
        buttons->addButton(config.button_text[0].empty()
                               ? tr(SWKBD_BUTTON_CANCEL)
                               : QString::fromStdString(config.button_text[0]),
                           QDialogButtonBox::ButtonRole::RejectRole);
        [[fallthrough]];
    case ButtonConfig::Single:
        buttons->addButton(config.button_text[2].empty()
                               ? tr(SWKBD_BUTTON_OKAY)
                               : QString::fromStdString(config.button_text[2]),
                           QDialogButtonBox::ButtonRole::AcceptRole);
        break;
    case ButtonConfig::None:
        break;
    }
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { Submit(); });
    connect(buttons, &QDialogButtonBox::rejected, this, [this] {
        button = QtKeyboard::cancel_id;
        accept();
    });
    connect(buttons, &QDialogButtonBox::helpRequested, this, [this] {
        button = QtKeyboard::forgot_id;
        accept();
    });
    layout->addWidget(label);
    layout->addWidget(line_edit);
    layout->addWidget(buttons);
    setLayout(layout);
}

void QtKeyboardDialog::Submit() {
    auto error = keyboard->ValidateInput(line_edit->text().toStdString());
    if (error != Frontend::ValidationError::None) {
        HandleValidationError(error);
    } else {
        button = keyboard->ok_id;
        text = line_edit->text();
        accept();
    }
}

void QtKeyboardDialog::HandleValidationError(Frontend::ValidationError error) {
    using namespace Frontend;
    const std::unordered_map<ValidationError, QString> VALIDATION_ERROR_MESSAGES = {
        {ValidationError::FixedLengthRequired,
         tr("Text length is not correct (should be %1 characters)")
             .arg(keyboard->config.max_text_length)},
        {ValidationError::MaxLengthExceeded,
         tr("Text is too long (should be no more than %1 characters)")
             .arg(keyboard->config.max_text_length)},
        {ValidationError::BlankInputNotAllowed, tr("Blank input is not allowed")},
        {ValidationError::EmptyInputNotAllowed, tr("Empty input is not allowed")},
    };
    QMessageBox::critical(this, tr("Validation error"), VALIDATION_ERROR_MESSAGES.at(error));
}

QtKeyboard::QtKeyboard(QWidget& parent_) : parent(parent_) {}

void QtKeyboard::Execute(const Frontend::KeyboardConfig& config) {
    SoftwareKeyboard::Execute(config);
    if (this->config.button_config != Frontend::ButtonConfig::None) {
        ok_id = static_cast<u8>(this->config.button_config);
    }
    QMetaObject::invokeMethod(this, "OpenInputDialog", Qt::BlockingQueuedConnection);
    Finalize(result_text, result_button);
}

void QtKeyboard::ShowError(const std::string& error) {
    QString message = QString::fromStdString(error);
    QMetaObject::invokeMethod(this, "ShowErrorDialog", Qt::BlockingQueuedConnection,
                              Q_ARG(QString, message));
}

void QtKeyboard::OpenInputDialog() {
    QtKeyboardDialog dialog(&parent, this);
    dialog.setWindowFlags(dialog.windowFlags() &
                          ~(Qt::WindowCloseButtonHint | Qt::WindowContextHelpButtonHint));
    dialog.setWindowModality(Qt::WindowModal);
    dialog.exec();

    result_text = dialog.text.toStdString();
    result_button = dialog.button;
    LOG_INFO(Frontend, "SWKBD input dialog finished, text={}, button={}", result_text,
             result_button);
}

void QtKeyboard::ShowErrorDialog(QString message) {
    QMessageBox::critical(&parent, tr("Software Keyboard"), message);
}
