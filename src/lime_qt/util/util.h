// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <filesystem>
#include <QFont>
#include <QString>
#include "common/common_types.h"

/// Returns a QFont object appropriate to use as a monospace font for debugging widgets, etc.
QFont GetMonospaceFont();

/// Convert a size in bytes into a readable format (KiB, MiB, etc.)
QString ReadableByteSize(qulonglong size);

/**
 * Creates a circle pixmap from a specified color
 * @param color The color the pixmap shall have
 * @return QPixmap circle pixmap
 */
QPixmap CreateCirclePixmapFromColor(const QColor& color);

/**
 * Gets the game icon from SMDH data.
 * @param smdh_data SMDH data
 * @return QPixmap game icon
 */
QPixmap GetQPixmapFromSMDH(const std::vector<u8>& smdh_data);

/**
 * Saves a windows icon to a file
 * @param path The icons path
 * @param image The image to save
 * @return bool If the operation succeeded
 */
[[nodiscard]] bool SaveIconToFile(const std::filesystem::path& icon_path, const QImage& image);
