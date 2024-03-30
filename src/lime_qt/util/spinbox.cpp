// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Copyright 2014 Tony Wasserka
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the owner nor the names of its contributors may
//       be used to endorse or promote products derived from this software
//       without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <cstdlib>
#include <QLineEdit>
#include <QRegularExpression>
#include "common/assert.h"
#include "lime_qt/util/spinbox.h"

CSpinBox::CSpinBox(QWidget* parent)
    : QAbstractSpinBox(parent), min_value(-100), max_value(100), value(0), base(10), num_digits(0) {
    // TODO: Might be nice to not immediately call the slot.
    //       Think of an address that is being replaced by a different one, in which case a lot
    //       invalid intermediate addresses would be read from during editing.
    connect(lineEdit(), &QLineEdit::textEdited, this, &CSpinBox::OnEditingFinished);

    UpdateText();
}

void CSpinBox::SetValue(qint64 val) {
    auto old_value = value;
    value = std::max(std::min(val, max_value), min_value);

    if (old_value != value) {
        UpdateText();
        emit ValueChanged(value);
    }
}

void CSpinBox::SetRange(qint64 min, qint64 max) {
    min_value = min;
    max_value = max;

    SetValue(value);
    UpdateText();
}

void CSpinBox::stepBy(int steps) {
    auto new_value = value;
    // Scale number of steps by the currently selected digit
    // TODO: Move this code elsewhere and enable it.
    // TODO: Support for num_digits==0, too
    // TODO: Support base!=16, too
    // TODO: Make the cursor not jump back to the end of the line...
    /*if (base == 16 && num_digits > 0) {
        int digit = num_digits - (lineEdit()->cursorPosition() - prefix.length()) - 1;
        digit = std::max(0, std::min(digit, num_digits - 1));
        steps <<= digit * 4;
    }*/

    // Increment "new_value" by "steps", and perform annoying overflow checks, too.
    constexpr qint64 qint64_min = std::numeric_limits<qint64>::min();
    constexpr qint64 qint64_max = std::numeric_limits<qint64>::max();
    if (steps < 0 && new_value < qint64_min - steps) {
        new_value = qint64_min;
    } else if (steps > 0 && new_value > qint64_max - steps) {
        new_value = qint64_max;
    } else {
        new_value += steps;
    }

    SetValue(new_value);
    UpdateText();
}

QAbstractSpinBox::StepEnabled CSpinBox::stepEnabled() const {
    StepEnabled ret = StepNone;

    if (value > min_value)
        ret |= StepDownEnabled;

    if (value < max_value)
        ret |= StepUpEnabled;

    return ret;
}

void CSpinBox::SetBase(int base) {
    this->base = base;

    UpdateText();
}

void CSpinBox::SetNumDigits(int num_digits) {
    this->num_digits = num_digits;

    UpdateText();
}

void CSpinBox::SetPrefix(const QString& prefix) {
    this->prefix = prefix;

    UpdateText();
}

void CSpinBox::SetSuffix(const QString& suffix) {
    this->suffix = suffix;

    UpdateText();
}

static QString StringToInputMask(const QString& input) {
    QString mask = input;

    // ... replace any special characters by their escaped counterparts ...
    mask.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    mask.replace(QLatin1Char{'A'}, QStringLiteral("\\A"));
    mask.replace(QLatin1Char{'a'}, QStringLiteral("\\a"));
    mask.replace(QLatin1Char{'N'}, QStringLiteral("\\N"));
    mask.replace(QLatin1Char{'n'}, QStringLiteral("\\n"));
    mask.replace(QLatin1Char{'X'}, QStringLiteral("\\X"));
    mask.replace(QLatin1Char{'x'}, QStringLiteral("\\x"));
    mask.replace(QLatin1Char{'9'}, QStringLiteral("\\9"));
    mask.replace(QLatin1Char{'0'}, QStringLiteral("\\0"));
    mask.replace(QLatin1Char{'D'}, QStringLiteral("\\D"));
    mask.replace(QLatin1Char{'d'}, QStringLiteral("\\d"));
    mask.replace(QLatin1Char{'#'}, QStringLiteral("\\#"));
    mask.replace(QLatin1Char{'H'}, QStringLiteral("\\H"));
    mask.replace(QLatin1Char{'h'}, QStringLiteral("\\h"));
    mask.replace(QLatin1Char{'B'}, QStringLiteral("\\B"));
    mask.replace(QLatin1Char{'b'}, QStringLiteral("\\b"));
    mask.replace(QLatin1Char{'>'}, QStringLiteral("\\>"));
    mask.replace(QLatin1Char{'<'}, QStringLiteral("\\<"));
    mask.replace(QLatin1Char{'!'}, QStringLiteral("\\!"));

    return mask;
}

void CSpinBox::UpdateText() {
    // If a fixed number of digits is used, we put the line edit in insertion mode by setting an
    // input mask.
    QString mask;
    if (num_digits != 0) {
        mask += StringToInputMask(prefix);

        // For base 10 and negative range, demand a single sign character
        if (HasSign())
            mask.append(QLatin1Char{'X'}); // identified as "-" or "+" in the validator

        // Uppercase digits greater than 9.
        mask.append(QLatin1Char{'>'});

        // Match num_digits digits
        // Digits irrelevant to the chosen number base are filtered in the validator
        mask.append(QStringLiteral("H").repeated(std::max(num_digits, 1)));

        // Switch off case conversion
        mask.append(QLatin1Char{'!'});

        mask.append(StringToInputMask(suffix));
    }
    lineEdit()->setInputMask(mask);

    // Set new text without changing the cursor position. This will cause the cursor to briefly
    // appear at the end of the line and then to jump back to its original position. That's
    // a bit ugly, but better than having setText() move the cursor permanently all the time.
    int cursor_position = lineEdit()->cursorPosition();
    lineEdit()->setText(TextFromValue());
    lineEdit()->setCursorPosition(cursor_position);
}

QString CSpinBox::TextFromValue() {
    return prefix +
           (HasSign() ? ((value < 0) ? QStringLiteral("-") : QStringLiteral("+")) : QString{}) +
           QStringLiteral("%1").arg(std::abs(value), num_digits, base, QLatin1Char('0')).toUpper() +
           suffix;
}

qint64 CSpinBox::ValueFromText() {
    qsizetype strpos = prefix.length();

    QString num_string = text().mid(strpos, text().length() - strpos - suffix.length());
    return num_string.toLongLong(nullptr, base);
}

bool CSpinBox::HasSign() const {
    return base == 10 && min_value < 0;
}

void CSpinBox::OnEditingFinished() {
    // Only update for valid input
    QString input = lineEdit()->text();
    int pos = 0;
    if (QValidator::Acceptable == validate(input, pos))
        SetValue(ValueFromText());
}

QValidator::State CSpinBox::validate(QString& input, int& pos) const {
    if (!prefix.isEmpty() && input.left(prefix.length()) != prefix)
        return QValidator::Invalid;

    qsizetype strpos = prefix.length();

    // Empty "numbers" allowed as intermediate values
    if (strpos >= input.length() - HasSign() - suffix.length())
        return QValidator::Intermediate;

    DEBUG_ASSERT(base <= 10 || base == 16);
    QString regexp;

    // Demand sign character for negative ranges
    if (HasSign())
        regexp.append(QStringLiteral("[+\\-]"));

    // Match digits corresponding to the chosen number base.
    regexp.append(QStringLiteral("[0-%1").arg(std::min(base, 9)));
    if (base == 16) {
        regexp.append(QStringLiteral("a-fA-F"));
    }
    regexp.append(QLatin1Char(']'));

    // Specify number of digits
    if (num_digits > 0) {
        regexp.append(QStringLiteral("{%1}").arg(num_digits));
    } else {
        regexp.append(QLatin1Char{'+'});
    }

    // Match string
    QRegularExpression num_regexp(QRegularExpression::anchoredPattern(regexp));
    qsizetype num_pos = strpos;
    QString sub_input = input.mid(strpos, input.length() - strpos - suffix.length());

    auto match = num_regexp.match(sub_input);
    if (!match.hasMatch())
        return QValidator::Invalid;

    sub_input = sub_input.left(match.capturedLength());
    bool ok;
    qint64 val = sub_input.toLongLong(&ok, base);

    if (!ok)
        return QValidator::Invalid;

    // Outside boundaries => don't accept
    if (val < min_value || val > max_value)
        return QValidator::Invalid;

    // Make sure we are actually at the end of this string...
    strpos += match.capturedLength();

    if (!suffix.isEmpty() && input.mid(strpos) != suffix) {
        return QValidator::Invalid;
    } else {
        strpos += suffix.length();
    }

    if (strpos != input.length())
        return QValidator::Invalid;

    // At this point we can say for sure that the input is fine. Let's fix it up a bit though
    input.replace(num_pos, sub_input.length(), sub_input.toUpper());

    return QValidator::Acceptable;
}
