// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <unordered_map>
#include <QGraphicsOpacityEffect>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QString>
#include <QStyleOption>
#include <QTime>
#include <fmt/format.h>
#include "citra_qt/loading_screen.h"
#include "common/logging/log.h"
#include "core/loader/loader.h"
#include "core/loader/smdh.h"
#include "ui_loading_screen.h"
#include "video_core/rasterizer_interface.h"

constexpr char PROGRESSBAR_STYLE_PREPARE[] = R"(
QProgressBar {}
QProgressBar::chunk {})";

constexpr char PROGRESSBAR_STYLE_DECOMPILE[] = R"(
QProgressBar {
  background-color: black;
  border: 2px solid white;
  border-radius: 4px;
  padding: 2px;
}
QProgressBar::chunk {
  background-color: #fd8507;
  width: 1px;
})";

constexpr char PROGRESSBAR_STYLE_BUILD[] = R"(
QProgressBar {
  background-color: black;
  border: 2px solid white;
  border-radius: 4px;
  padding: 2px;
}
QProgressBar::chunk {
  background-color: #ffe402;
  width: 1px;
})";

constexpr char PROGRESSBAR_STYLE_COMPLETE[] = R"(
QProgressBar {
  background-color: #fd8507;
  border: 2px solid white;
  border-radius: 4px;
  padding: 2px;
}
QProgressBar::chunk {
  background-color: #ffe402;
})";

// Definitions for the differences in text and styling for each stage
const static std::unordered_map<VideoCore::LoadCallbackStage, const char*> stage_translations{
    {VideoCore::LoadCallbackStage::Prepare, QT_TRANSLATE_NOOP("LoadingScreen", "Loading...")},
    {VideoCore::LoadCallbackStage::Preload,
     QT_TRANSLATE_NOOP("LoadingScreen", "Preloading Textures %1 / %2")},
    {VideoCore::LoadCallbackStage::Decompile,
     QT_TRANSLATE_NOOP("LoadingScreen", "Preparing Shaders %1 / %2")},
    {VideoCore::LoadCallbackStage::Build,
     QT_TRANSLATE_NOOP("LoadingScreen", "Loading Shaders %1 / %2")},
    {VideoCore::LoadCallbackStage::Complete, QT_TRANSLATE_NOOP("LoadingScreen", "Launching...")},
};
const static std::unordered_map<VideoCore::LoadCallbackStage, const char*> progressbar_style{
    {VideoCore::LoadCallbackStage::Prepare, PROGRESSBAR_STYLE_PREPARE},
    {VideoCore::LoadCallbackStage::Preload, PROGRESSBAR_STYLE_BUILD},
    {VideoCore::LoadCallbackStage::Decompile, PROGRESSBAR_STYLE_DECOMPILE},
    {VideoCore::LoadCallbackStage::Build, PROGRESSBAR_STYLE_BUILD},
    {VideoCore::LoadCallbackStage::Complete, PROGRESSBAR_STYLE_COMPLETE},
};

static QPixmap GetQPixmapFromSMDH(std::vector<u8>& smdh_data) {
    Loader::SMDH smdh;
    std::memcpy(&smdh, smdh_data.data(), sizeof(Loader::SMDH));

    bool large = true;
    std::vector<u16> icon_data = smdh.GetIcon(large);
    const uchar* data = reinterpret_cast<const uchar*>(icon_data.data());
    int size = large ? 48 : 24;
    QImage icon(data, size, size, QImage::Format::Format_RGB16);
    return QPixmap::fromImage(icon);
}

LoadingScreen::LoadingScreen(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::LoadingScreen>()),
      previous_stage(VideoCore::LoadCallbackStage::Complete) {
    ui->setupUi(this);
    setMinimumSize(400, 240);

    // Create a fade out effect to hide this loading screen widget.
    // When fading opacity, it will fade to the parent widgets background color, which is why we
    // create an internal widget named fade_widget that we use the effect on, while keeping the
    // loading screen widget's background color black. This way we can create a fade to black effect
    opacity_effect = new QGraphicsOpacityEffect(this);
    opacity_effect->setOpacity(1);
    ui->fade_parent->setGraphicsEffect(opacity_effect);
    fadeout_animation = std::make_unique<QPropertyAnimation>(opacity_effect, "opacity");
    fadeout_animation->setDuration(500);
    fadeout_animation->setStartValue(1);
    fadeout_animation->setEndValue(0);
    fadeout_animation->setEasingCurve(QEasingCurve::OutBack);

    // After the fade completes, hide the widget and reset the opacity
    connect(fadeout_animation.get(), &QPropertyAnimation::finished, [this] {
        hide();
        opacity_effect->setOpacity(1);
        emit Hidden();
    });
    connect(this, &LoadingScreen::LoadProgress, this, &LoadingScreen::OnLoadProgress,
            Qt::QueuedConnection);
    qRegisterMetaType<VideoCore::LoadCallbackStage>();
}

LoadingScreen::~LoadingScreen() = default;

void LoadingScreen::Prepare(Loader::AppLoader& loader) {
    std::vector<u8> buffer;
    // TODO when banner becomes supported, decode it and add it as a movie

    if (loader.ReadIcon(buffer) == Loader::ResultStatus::Success) {
        QPixmap icon = GetQPixmapFromSMDH(buffer);
        ui->icon->setPixmap(icon);
    } else {
        ui->icon->clear();
    }
    std::string title;
    if (loader.ReadTitle(title) != Loader::ResultStatus::Success) {
        u64 program_id;
        if (loader.ReadProgramId(program_id) == Loader::ResultStatus::Success) {
            title = fmt::format("{:016x}", program_id);
        }
    }
    ui->title->setText(tr("Now Loading\n%1").arg(QString::fromStdString(title)));
    eta_shown = false;
    OnLoadProgress(VideoCore::LoadCallbackStage::Prepare, 0, 0);
}

void LoadingScreen::OnLoadComplete() {
    fadeout_animation->start(QPropertyAnimation::KeepWhenStopped);
}

void LoadingScreen::OnLoadProgress(VideoCore::LoadCallbackStage stage, std::size_t value,
                                   std::size_t total) {
    using namespace std::chrono;
    const auto now = high_resolution_clock::now();
    // reset the timer if the stage changes
    if (stage != previous_stage) {
        ui->progress_bar->setStyleSheet(QString::fromUtf8(progressbar_style.at(stage)));
        // Hide the progress bar during the prepare stage
        if (stage == VideoCore::LoadCallbackStage::Prepare) {
            ui->progress_bar->hide();
        } else {
            ui->progress_bar->show();
        }
        previous_stage = stage;
    }
    // update the max of the progress bar if the number of shaders change
    if (total != previous_total) {
        ui->progress_bar->setMaximum(static_cast<int>(total));
        previous_total = total;
    }

    // calculate a simple rolling average after the first shader is loaded
    if (value > 0) {
        rolling_average -= rolling_average / NumberOfDataPoints;
        rolling_average += (now - previous_time) / NumberOfDataPoints;
    }

    QString estimate;

    // After 25 shader load times were put into the rolling average, determine if the ETA is long
    // enough to show it
    if (value > NumberOfDataPoints &&
        (eta_shown || rolling_average * (total - value) > ETABreakPoint)) {
        if (!eta_shown) {
            eta_shown = true;
        }
        const auto eta_mseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
            rolling_average * (total - value));
        const auto limited_mseconds = std::max<long>(eta_mseconds.count(), 1000);
        estimate = tr("Estimated Time %1")
                       .arg(QTime(0, 0, 0, 0)
                                .addMSecs(static_cast<int>(limited_mseconds))
                                .toString(QStringLiteral("mm:ss")));
    }

    // update labels and progress bar
    const auto& stg = tr(stage_translations.at(stage));
    if (stage == VideoCore::LoadCallbackStage::Decompile ||
        stage == VideoCore::LoadCallbackStage::Build ||
        stage == VideoCore::LoadCallbackStage::Preload) {
        ui->stage->setText(stg.arg(value).arg(total));
    } else {
        ui->stage->setText(stg);
    }
    ui->value->setText(estimate);
    ui->progress_bar->setValue(static_cast<int>(value));
    previous_time = now;
}

void LoadingScreen::paintEvent(QPaintEvent* event) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(event);
}

void LoadingScreen::Clear() {}
