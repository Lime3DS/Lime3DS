// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QFont>
#include <QString>

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
