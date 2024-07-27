// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QAction>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QString>
#include <QTimer>
#include "common/common_types.h"
#include "common/microprofile.h"
#include "lime_qt/debugger/profiler.h"
#include "lime_qt/util/util.h"

// Include the implementation of the UI in this file. This isn't in microprofile.cpp because the
// non-Qt frontends don't need it (and don't implement the UI drawing hooks either).
#if MICROPROFILE_ENABLED
#define MICROPROFILEUI_IMPL 1
#include "common/microprofileui.h"

class MicroProfileWidget : public QWidget {
public:
    MicroProfileWidget(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    /// This timer is used to redraw the widget's contents continuously. To save resources, it only
    /// runs while the widget is visible.
    QTimer update_timer;
    /// Scale the coordinate system appropriately when dpi != 96.
    qreal x_scale = 1.0, y_scale = 1.0;
};

#endif

MicroProfileDialog::MicroProfileDialog(QWidget* parent) : QWidget(parent, Qt::Dialog) {
    setObjectName(QStringLiteral("MicroProfile"));
    setWindowTitle(tr("MicroProfile"));
    resize(1000, 600);
    // Enable the maximize button
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

#if MICROPROFILE_ENABLED

    MicroProfileWidget* widget = new MicroProfileWidget(this);

    QLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(widget);
    setLayout(layout);

    // Configure focus so that widget is focusable and the dialog automatically forwards focus to
    // it.
    setFocusProxy(widget);
    widget->setFocusPolicy(Qt::StrongFocus);
    widget->setFocus();
#endif
}

QAction* MicroProfileDialog::toggleViewAction() {
    if (toggle_view_action == nullptr) {
        toggle_view_action = new QAction(windowTitle(), this);
        toggle_view_action->setCheckable(true);
        toggle_view_action->setChecked(isVisible());
        connect(toggle_view_action, &QAction::toggled, this, &MicroProfileDialog::setVisible);
    }

    return toggle_view_action;
}

void MicroProfileDialog::showEvent(QShowEvent* event) {
    if (toggle_view_action) {
        toggle_view_action->setChecked(isVisible());
    }
    QWidget::showEvent(event);
}

void MicroProfileDialog::hideEvent(QHideEvent* event) {
    if (toggle_view_action) {
        toggle_view_action->setChecked(isVisible());
    }
    QWidget::hideEvent(event);
}

#if MICROPROFILE_ENABLED

/// There's no way to pass a user pointer to MicroProfile, so this variable is used to make the
/// QPainter available inside the drawing callbacks.
static QPainter* mp_painter = nullptr;

MicroProfileWidget::MicroProfileWidget(QWidget* parent) : QWidget(parent) {
    // Send mouse motion events even when not dragging.
    setMouseTracking(true);

    MicroProfileSetDisplayMode(1); // Timers screen
    MicroProfileInitUI();

    connect(&update_timer, &QTimer::timeout, this, qOverload<>(&MicroProfileWidget::update));
}

void MicroProfileWidget::paintEvent([[maybe_unused]] QPaintEvent* event) {
    QPainter painter(this);

    // The units used by Microprofile for drawing are based in pixels on a 96 dpi display.
    x_scale = qreal(painter.device()->logicalDpiX()) / 96.0;
    y_scale = qreal(painter.device()->logicalDpiY()) / 96.0;
    painter.scale(x_scale, y_scale);

    painter.setBackground(Qt::black);
    painter.eraseRect(rect());

    QFont font = GetMonospaceFont();
    font.setPixelSize(MICROPROFILE_TEXT_HEIGHT);
    painter.setFont(font);

    mp_painter = &painter;
    MicroProfileDraw(rect().width() / x_scale, rect().height() / y_scale);
    mp_painter = nullptr;
}

void MicroProfileWidget::showEvent(QShowEvent* event) {
    update_timer.start(15); // ~60 Hz
    QWidget::showEvent(event);
}

void MicroProfileWidget::hideEvent(QHideEvent* event) {
    update_timer.stop();
    QWidget::hideEvent(event);
}

void MicroProfileWidget::mouseMoveEvent(QMouseEvent* event) {
    const auto point = event->position().toPoint();
    MicroProfileMousePosition(point.x() / x_scale, point.y() / y_scale, 0);
    event->accept();
}

void MicroProfileWidget::mousePressEvent(QMouseEvent* event) {
    const auto point = event->position().toPoint();
    MicroProfileMousePosition(point.x() / x_scale, point.y() / y_scale, 0);
    MicroProfileMouseButton(event->buttons() & Qt::LeftButton, event->buttons() & Qt::RightButton);
    event->accept();
}

void MicroProfileWidget::mouseReleaseEvent(QMouseEvent* event) {
    const auto point = event->position().toPoint();
    MicroProfileMousePosition(point.x() / x_scale, point.y() / y_scale, 0);
    MicroProfileMouseButton(event->buttons() & Qt::LeftButton, event->buttons() & Qt::RightButton);
    event->accept();
}

void MicroProfileWidget::wheelEvent(QWheelEvent* event) {
    const auto point = event->position().toPoint();
    MicroProfileMousePosition(point.x() / x_scale, point.y() / y_scale,
                              event->angleDelta().y() / 120);
    event->accept();
}

void MicroProfileWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Control) {
        // Inform MicroProfile that the user is holding Ctrl.
        MicroProfileModKey(1);
    }
    QWidget::keyPressEvent(event);
}

void MicroProfileWidget::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Control) {
        MicroProfileModKey(0);
    }
    QWidget::keyReleaseEvent(event);
}

// These functions are called by MicroProfileDraw to draw the interface elements on the screen.

void MicroProfileDrawText(int x, int y, u32 hex_color, const char* text, u32 text_length) {
    // hex_color does not include an alpha, so it must be assumed to be 255
    mp_painter->setPen(QColor::fromRgb(hex_color));

    // It's impossible to draw a string using a monospaced font with a fixed width per cell in a
    // way that's reliable across different platforms and fonts as far as I (yuriks) can tell, so
    // draw each character individually in order to precisely control the text advance.
    for (u32 i = 0; i < text_length; ++i) {
        // Position the text baseline 1 pixel above the bottom of the text cell, this gives nice
        // vertical alignment of text for a wide range of tested fonts.
        mp_painter->drawText(x, y + MICROPROFILE_TEXT_HEIGHT - 2, QString{QLatin1Char{text[i]}});
        x += MICROPROFILE_TEXT_WIDTH + 1;
    }
}

void MicroProfileDrawBox(int left, int top, int right, int bottom, u32 hex_color,
                         MicroProfileBoxType type) {
    QColor color = QColor::fromRgba(hex_color);
    QBrush brush = color;
    if (type == MicroProfileBoxTypeBar) {
        QLinearGradient gradient(left, top, left, bottom);
        gradient.setColorAt(0.f, color.lighter(125));
        gradient.setColorAt(1.f, color.darker(125));
        brush = gradient;
    }
    mp_painter->fillRect(left, top, right - left, bottom - top, brush);
}

void MicroProfileDrawLine2D(u32 vertices_length, float* vertices, u32 hex_color) {
    // Temporary vector used to convert between the float array and QPointF. Marked static to reuse
    // the allocation across calls.
    static std::vector<QPointF> point_buf;

    point_buf.reserve(vertices_length);
    for (u32 i = 0; i < vertices_length; ++i) {
        point_buf.emplace_back(vertices[i * 2 + 0], vertices[i * 2 + 1]);
    }

    // hex_color does not include an alpha, so it must be assumed to be 255
    mp_painter->setPen(QColor::fromRgb(hex_color));
    mp_painter->drawPolyline(point_buf.data(), vertices_length);
    point_buf.clear();
}
#endif
