// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QImage>
#include <QImageReader>
#include <QString>
#include "citra_qt/qt_image_interface.h"
#include "common/logging/log.h"

QtImageInterface::QtImageInterface() {
    QImageReader::setAllocationLimit(0);
}

bool QtImageInterface::DecodePNG(std::vector<u8>& dst, u32& width, u32& height,
                                 std::span<const u8> src) {
    QImage image(QImage::fromData(src.data(), static_cast<int>(src.size())));
    if (image.isNull()) {
        LOG_ERROR(Frontend, "Failed to decode png because image is null");
        return false;
    }
    width = image.width();
    height = image.height();

    image = image.convertToFormat(QImage::Format_RGBA8888);

    // Write RGBA8 to vector
    const std::size_t image_size = width * height * 4;
    dst.resize(image_size);
    std::memcpy(dst.data(), image.constBits(), image_size);

    return true;
}

bool QtImageInterface::EncodePNG(const std::string& path, u32 width, u32 height,
                                 std::span<const u8> src) {
    QImage image(src.data(), width, height, QImage::Format_RGBA8888);

    if (!image.save(QString::fromStdString(path), "PNG")) {
        LOG_ERROR(Frontend, "Failed to save {}", path);
        return false;
    }
    return true;
}
