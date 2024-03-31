// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <memory>
#include <QWidget>

namespace Loader {
class AppLoader;
}

namespace Ui {
class LoadingScreen;
}

namespace VideoCore {
enum class LoadCallbackStage;
}

class QGraphicsOpacityEffect;
class QPropertyAnimation;

class LoadingScreen : public QWidget {
    Q_OBJECT

public:
    explicit LoadingScreen(QWidget* parent = nullptr);

    ~LoadingScreen();

    /// Call before showing the loading screen to load the widgets with the logo and banner for the
    /// currently loaded application.
    void Prepare(Loader::AppLoader& loader);

    /// After the loading screen is hidden, the owner of this class can call this to clean up any
    /// used resources such as the logo and banner.
    void Clear();

    /// Slot used to update the status of the progress bar
    void OnLoadProgress(VideoCore::LoadCallbackStage stage, std::size_t value, std::size_t total);

    /// Hides the LoadingScreen with a fade out effect
    void OnLoadComplete();

    // In order to use a custom widget with a stylesheet, you need to override the paintEvent
    // See https://wiki.qt.io/How_to_Change_the_Background_Color_of_QWidget
    void paintEvent(QPaintEvent* event) override;

signals:
    void LoadProgress(VideoCore::LoadCallbackStage stage, std::size_t value, std::size_t total);
    /// Signals that this widget is completely hidden now and should be replaced with the other
    /// widget
    void Hidden();

private:
    std::unique_ptr<Ui::LoadingScreen> ui;
    std::size_t previous_total = 0;
    VideoCore::LoadCallbackStage previous_stage;

    QGraphicsOpacityEffect* opacity_effect = nullptr;
    std::unique_ptr<QPropertyAnimation> fadeout_animation;

    // Variables used to keep track of the current ETA.
    // If the rolling_average * shaders_remaining > eta_break_point then we want to display the eta.
    // We don't want to always display it since showing an ETA leads people to think its taking
    // longer that it is because ETAs are often wrong
    static constexpr std::chrono::seconds ETABreakPoint = std::chrono::seconds{10};
    static constexpr std::size_t NumberOfDataPoints = 25;
    std::chrono::high_resolution_clock::time_point previous_time;
    std::chrono::duration<double> rolling_average = {};
    bool eta_shown = false;
};
