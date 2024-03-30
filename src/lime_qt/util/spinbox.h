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

#pragma once

#include <QAbstractSpinBox>
#include <QtGlobal>

class QVariant;

/**
 * A custom spin box widget with enhanced functionality over Qt's QSpinBox
 */
class CSpinBox : public QAbstractSpinBox {
    Q_OBJECT

public:
    explicit CSpinBox(QWidget* parent = nullptr);

    void stepBy(int steps) override;
    StepEnabled stepEnabled() const override;

    void SetValue(qint64 val);

    void SetRange(qint64 min, qint64 max);

    void SetBase(int base);

    void SetPrefix(const QString& prefix);
    void SetSuffix(const QString& suffix);

    void SetNumDigits(int num_digits);

    QValidator::State validate(QString& input, int& pos) const override;

signals:
    void ValueChanged(qint64 val);

private slots:
    void OnEditingFinished();

private:
    void UpdateText();

    bool HasSign() const;

    QString TextFromValue();
    qint64 ValueFromText();

    qint64 min_value, max_value;

    qint64 value;

    QString prefix, suffix;

    int base;

    int num_digits;
};
