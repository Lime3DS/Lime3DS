// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <utility>
#include <vector>
#include <QFrame>
#include <QPointer>

class QLabel;

// Widget for representing touchscreen coordinates
class TouchScreenPreview : public QFrame {
    Q_OBJECT
    Q_PROPERTY(QColor dotHighlightColor MEMBER dot_highlight_color)

public:
    explicit TouchScreenPreview(QWidget* parent);
    ~TouchScreenPreview() override;

    void SetCoordLabel(QLabel*);
    int AddDot(int device_x, int device_y);
    void RemoveDot(int id);
    void HighlightDot(int id, bool active = true) const;
    void MoveDot(int id, int device_x, int device_y) const;

signals:
    void DotAdded(const QPoint& pos);
    void DotSelected(int dot_id);
    void DotMoved(int dot_id, const QPoint& pos);

protected:
    void resizeEvent(QResizeEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;

private:
    std::optional<QPoint> MapToDeviceCoords(int screen_x, int screen_y) const;
    void PositionDot(QLabel* dot, int device_x = -1, int device_y = -1) const;

    bool ignore_resize = false;
    QPointer<QLabel> coord_label;

    std::vector<std::pair<int, QLabel*>> dots;
    int max_dot_id = 0;
    QColor dot_highlight_color;
    static constexpr char PropId[] = "dot_id";
    static constexpr char PropX[] = "device_x";
    static constexpr char PropY[] = "device_y";

    struct DragState {
        bool active = false;
        QPointer<QLabel> dot;
        QPoint start_pos;
    };
    DragState drag_state;
};
