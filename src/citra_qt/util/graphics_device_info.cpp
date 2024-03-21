// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/util/graphics_device_info.h"

#ifdef ENABLE_OPENGL
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#endif

#ifdef ENABLE_VULKAN
#include "video_core/renderer_vulkan/vk_instance.h"
#endif

#ifdef ENABLE_OPENGL
QString GetOpenGLRenderer() {
    QOffscreenSurface surface;
    surface.create();

    QOpenGLContext context;
    if (context.create()) {
        context.makeCurrent(&surface);
        return QString::fromUtf8(reinterpret_cast<const char*>(context.functions()->glGetString(GL_RENDERER)));
    } else {
        return QStringLiteral("");
    }
}
#endif

#ifdef ENABLE_VULKAN
std::vector<QString> GetVulkanPhysicalDevices() {
    std::vector<QString> result;
    try {
        Vulkan::Instance instance{};
        const auto physical_devices = instance.GetPhysicalDevices();

        for (const vk::PhysicalDevice physical_device : physical_devices) {
            const QString name = QString::fromUtf8(physical_device.getProperties().deviceName, -1);
            result.push_back(name);
        }
    } catch (const std::runtime_error& err) {
        LOG_ERROR(Frontend, "Error occured while querying for physical devices: {}", err.what());
    }

    return result;
}
#endif
