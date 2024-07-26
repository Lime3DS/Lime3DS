// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QDialogButtonBox>
#include <QKeySequenceEdit>
#include <QVBoxLayout>
#include "lime_qt/util/sequence_dialog/sequence_dialog.h"

SequenceDialog::SequenceDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Enter a hotkey"));

    key_sequence = new QKeySequenceEdit;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    key_sequence->setMaximumSequenceLength(1);
    key_sequence->setFinishingKeyCombinations({});
#endif

    auto* const buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->setCenterButtons(true);

    auto* const layout = new QVBoxLayout(this);
    layout->addWidget(key_sequence);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

SequenceDialog::~SequenceDialog() = default;

QKeySequence SequenceDialog::GetSequence() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return key_sequence->keySequence();
#else
    // Only the first key is returned. The other 3, if present, are ignored.
    return QKeySequence(key_sequence->keySequence()[0]);
#endif
}

bool SequenceDialog::focusNextPrevChild(bool next) {
    return false;
}

void SequenceDialog::closeEvent(QCloseEvent*) {
    reject();
}
