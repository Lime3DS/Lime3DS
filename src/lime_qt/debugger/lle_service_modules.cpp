// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QCheckBox>
#include <QLabel>
#include <QLayout>
#include <QScrollArea>
#include "common/settings.h"
#include "lime_qt/debugger/lle_service_modules.h"

LLEServiceModulesWidget::LLEServiceModulesWidget(QWidget* parent)
    : QDockWidget(tr("Toggle LLE Service Modules"), parent) {
    setObjectName(QStringLiteral("LLEServiceModulesWidget"));
    QScrollArea* scroll_area = new QScrollArea;
    QLayout* scroll_layout = new QVBoxLayout;
    for (const auto& service_module : Settings::values.lle_modules) {
        QCheckBox* check_box =
            new QCheckBox(QString::fromStdString(service_module.first), scroll_area);
        check_box->setChecked(service_module.second);
        connect(check_box, &QCheckBox::toggled, [check_box] {
            Settings::values.lle_modules.find(check_box->text().toStdString())->second =
                check_box->isChecked();
        });
        scroll_layout->addWidget(check_box);
    }
    QWidget* scroll_area_contents = new QWidget;
    scroll_area_contents->setLayout(scroll_layout);
    scroll_area->setWidget(scroll_area_contents);
    setWidget(scroll_area);
}

LLEServiceModulesWidget::~LLEServiceModulesWidget() = default;
