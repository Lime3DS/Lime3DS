// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Include the vulkan platform specific header
#if defined(ANDROID)
#define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#else
#define VK_USE_PLATFORM_WAYLAND_KHR
#define VK_USE_PLATFORM_XLIB_KHR
#endif

#include <memory>
#include <vector>
#include <boost/container/static_vector.hpp>
#include <fmt/format.h>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/frontend/emu_window.h"
#include "video_core/renderer_vulkan/vk_platform.h"

namespace Vulkan {

namespace {
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {

    switch (static_cast<u32>(callback_data->messageIdNumber)) {
    case 0x609a13b: // Vertex attribute at location not consumed by shader
    case 0xc81ad50e:
        return VK_FALSE;
    default:
        break;
    }

    Common::Log::Level level{};
    switch (severity) {
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
        level = Common::Log::Level::Error;
        break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
        level = Common::Log::Level::Info;
        break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
        level = Common::Log::Level::Debug;
        break;
    default:
        level = Common::Log::Level::Info;
    }

    LOG_GENERIC(Common::Log::Class::Render_Vulkan, level, "{}: {}",
                callback_data->pMessageIdName ? callback_data->pMessageIdName : "<null>",
                callback_data->pMessage ? callback_data->pMessage : "<null>");

    return VK_FALSE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(vk::DebugReportFlagsEXT flags,
                                                          vk::DebugReportObjectTypeEXT objectType,
                                                          uint64_t object, std::size_t location,
                                                          int32_t messageCode,
                                                          const char* pLayerPrefix,
                                                          const char* pMessage, void* pUserData) {

    const auto severity = static_cast<vk::DebugReportFlagBitsEXT>(flags.operator VkFlags());
    Common::Log::Level level{};
    switch (severity) {
    case vk::DebugReportFlagBitsEXT::eError:
        level = Common::Log::Level::Error;
        break;
    case vk::DebugReportFlagBitsEXT::eInformation:
        level = Common::Log::Level::Warning;
        break;
    case vk::DebugReportFlagBitsEXT::eDebug:
    case vk::DebugReportFlagBitsEXT::eWarning:
    case vk::DebugReportFlagBitsEXT::ePerformanceWarning:
        level = Common::Log::Level::Debug;
        break;
    default:
        level = Common::Log::Level::Info;
    }

    LOG_GENERIC(Common::Log::Class::Render_Vulkan, level,
                "type = {}, object = {} | MessageCode = {:#x}, LayerPrefix = {} | {}",
                vk::to_string(objectType), object, messageCode, pLayerPrefix, pMessage);

    return VK_FALSE;
}
} // Anonymous namespace

std::shared_ptr<Common::DynamicLibrary> OpenLibrary(
    [[maybe_unused]] Frontend::GraphicsContext* context) {
#ifdef ANDROID
    // Android may override the Vulkan driver from the frontend.
    if (auto library = context->GetDriverLibrary(); library) {
        return library;
    }
#endif
    auto library = std::make_shared<Common::DynamicLibrary>();
#ifdef __APPLE__
    const std::string filename = Common::DynamicLibrary::GetLibraryName("vulkan");
    if (!library->Load(filename)) {
        // Fall back to directly loading bundled MoltenVK library.
        const std::string mvk_filename = Common::DynamicLibrary::GetLibraryName("MoltenVK");
        void(library->Load(mvk_filename));
    }
#else
    std::string filename = Common::DynamicLibrary::GetLibraryName("vulkan", 1);
    LOG_DEBUG(Render_Vulkan, "Trying Vulkan library: {}", filename);
    if (!library->Load(filename)) {
        // Android devices may not have libvulkan.so.1, only libvulkan.so.
        filename = Common::DynamicLibrary::GetLibraryName("vulkan");
        LOG_DEBUG(Render_Vulkan, "Trying Vulkan library (second attempt): {}", filename);
        void(library->Load(filename));
    }
#endif
    return library;
}

vk::SurfaceKHR CreateSurface(vk::Instance instance, const Frontend::EmuWindow& emu_window) {
    const auto& window_info = emu_window.GetWindowInfo();
    vk::SurfaceKHR surface{};
    vk::Result res;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    if (window_info.type == Frontend::WindowSystemType::Windows) {
        const vk::Win32SurfaceCreateInfoKHR win32_ci = {
            .hinstance = nullptr,
            .hwnd = static_cast<HWND>(window_info.render_surface),
        };

        if ((res = instance.createWin32SurfaceKHR(&win32_ci, nullptr, &surface)) !=
            vk::Result::eSuccess) {
            LOG_CRITICAL(Render_Vulkan, "Failed to initialize Win32 surface: {}",
                         vk::to_string(res));
            UNREACHABLE();
        }
    }
#elif defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_WAYLAND_KHR)
    if (window_info.type == Frontend::WindowSystemType::X11) {
        const vk::XlibSurfaceCreateInfoKHR xlib_ci = {
            .dpy = static_cast<Display*>(window_info.display_connection),
            .window = reinterpret_cast<Window>(window_info.render_surface),
        };

        if ((res = instance.createXlibSurfaceKHR(&xlib_ci, nullptr, &surface)) !=
            vk::Result::eSuccess) {
            LOG_ERROR(Render_Vulkan, "Failed to initialize Xlib surface: {}", vk::to_string(res));
            UNREACHABLE();
        }
    } else if (window_info.type == Frontend::WindowSystemType::Wayland) {
        const vk::WaylandSurfaceCreateInfoKHR wayland_ci = {
            .display = static_cast<wl_display*>(window_info.display_connection),
            .surface = static_cast<wl_surface*>(window_info.render_surface),
        };

        if ((res = instance.createWaylandSurfaceKHR(&wayland_ci, nullptr, &surface)) !=
            vk::Result::eSuccess) {
            LOG_ERROR(Render_Vulkan, "Failed to initialize Wayland surface: {}",
                      vk::to_string(res));
            UNREACHABLE();
        }
    }
#elif defined(VK_USE_PLATFORM_METAL_EXT)
    if (window_info.type == Frontend::WindowSystemType::MacOS) {
        const vk::MetalSurfaceCreateInfoEXT macos_ci = {
            .pLayer = static_cast<const CAMetalLayer*>(window_info.render_surface),
        };

        if ((res = instance.createMetalSurfaceEXT(&macos_ci, nullptr, &surface)) !=
            vk::Result::eSuccess) {
            LOG_CRITICAL(Render_Vulkan, "Failed to initialize MacOS surface: {}",
                         vk::to_string(res));
            UNREACHABLE();
        }
    }
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    if (window_info.type == Frontend::WindowSystemType::Android) {
        vk::AndroidSurfaceCreateInfoKHR android_ci = {
            .window = reinterpret_cast<ANativeWindow*>(window_info.render_surface),
        };

        if ((res = instance.createAndroidSurfaceKHR(&android_ci, nullptr, &surface)) !=
            vk::Result::eSuccess) {
            LOG_CRITICAL(Render_Vulkan, "Failed to initialize Android surface: {}",
                         vk::to_string(res));
            UNREACHABLE();
        }
    }
#endif

    if (!surface) {
        LOG_CRITICAL(Render_Vulkan, "Presentation not supported on this platform");
        UNREACHABLE();
    }

    return surface;
}

std::vector<const char*> GetInstanceExtensions(Frontend::WindowSystemType window_type,
                                               bool enable_debug_utils) {
    const auto properties = vk::enumerateInstanceExtensionProperties();
    if (properties.empty()) {
        LOG_ERROR(Render_Vulkan, "Failed to query extension properties");
        return {};
    }

    // Add the windowing system specific extension
    std::vector<const char*> extensions;
    extensions.reserve(7);

#if defined(__APPLE__)
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    // For configuring MoltenVK.
    extensions.push_back(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);
#endif

    switch (window_type) {
    case Frontend::WindowSystemType::Headless:
        break;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    case Frontend::WindowSystemType::Windows:
        extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
        break;
#elif defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_WAYLAND_KHR)
    case Frontend::WindowSystemType::X11:
        extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
        break;
    case Frontend::WindowSystemType::Wayland:
        extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
        break;
#elif defined(VK_USE_PLATFORM_METAL_EXT)
    case Frontend::WindowSystemType::MacOS:
        extensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
        break;
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    case Frontend::WindowSystemType::Android:
        extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
        break;
#endif
    default:
        LOG_ERROR(Render_Vulkan, "Presentation not supported on this platform");
        break;
    }

    if (window_type != Frontend::WindowSystemType::Headless) {
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    }

    if (enable_debug_utils) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    // Sanitize extension list
    std::erase_if(extensions, [&](const char* extension) -> bool {
        const auto it =
            std::find_if(properties.begin(), properties.end(), [extension](const auto& prop) {
                return std::strcmp(extension, prop.extensionName) == 0;
            });

        if (it == properties.end()) {
            LOG_INFO(Render_Vulkan, "Candidate instance extension {} is not available", extension);
            return true;
        }
        return false;
    });

    return extensions;
}

vk::InstanceCreateFlags GetInstanceFlags() {
#if defined(__APPLE__)
    return vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#else
    return static_cast<vk::InstanceCreateFlags>(0);
#endif
}

vk::UniqueInstance CreateInstance(const Common::DynamicLibrary& library,
                                  Frontend::WindowSystemType window_type, bool enable_validation,
                                  bool dump_command_buffers) {
    if (!library.IsLoaded()) {
        throw std::runtime_error("Failed to load Vulkan driver library");
    }

    const auto vkGetInstanceProcAddr =
        library.GetSymbol<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    if (!vkGetInstanceProcAddr) {
        throw std::runtime_error("Failed GetSymbol vkGetInstanceProcAddr");
    }
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    const u32 available_version = VULKAN_HPP_DEFAULT_DISPATCHER.vkEnumerateInstanceVersion
                                      ? vk::enumerateInstanceVersion()
                                      : VK_API_VERSION_1_0;
    if (available_version < TargetVulkanApiVersion) {
        throw std::runtime_error(fmt::format(
            "Vulkan {}.{} is required, but only {}.{} is supported by instance!",
            VK_VERSION_MAJOR(TargetVulkanApiVersion), VK_VERSION_MINOR(TargetVulkanApiVersion),
            VK_VERSION_MAJOR(available_version), VK_VERSION_MINOR(available_version)));
    }

    const auto extensions = GetInstanceExtensions(window_type, enable_validation);

    const vk::ApplicationInfo application_info = {
        .pApplicationName = "Citra",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Citra Vulkan",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = TargetVulkanApiVersion,
    };

    boost::container::static_vector<const char*, 2> layers;
    if (enable_validation) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }
    if (dump_command_buffers) {
        layers.push_back("VK_LAYER_LUNARG_api_dump");
    }

    // Sanitize layers list
    const auto layer_properties = vk::enumerateInstanceLayerProperties();
    if (layer_properties.empty()) {
        LOG_WARNING(Render_Vulkan, "Instance layer properties list is empty");
    }

    boost::container::erase_if(layers, [&](const char* layer) -> bool {
        const auto it = std::find_if(
            layer_properties.begin(), layer_properties.end(),
            [layer](const auto& prop) { return std::strcmp(layer, prop.layerName) == 0; });

        if (it == layer_properties.end()) {
            LOG_INFO(Render_Vulkan, "Candidate instance layer {} is not available", layer);
            return true;
        }
        return false;
    });

    vk::InstanceCreateInfo instance_ci = {
        .flags = GetInstanceFlags(),
        .pApplicationInfo = &application_info,
        .enabledLayerCount = static_cast<u32>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = static_cast<u32>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

#ifdef __APPLE__
    // Use synchronous queue submits if async presentation is enabled, to avoid threading
    // indirection.
    const auto synchronous_queue_submits = Settings::values.async_presentation.GetValue();
    // If the device is lost, make an attempt to resume if possible to avoid crashes.
    constexpr auto resume_lost_device = true;
    // Maximize concurrency to improve shader compilation performance.
    constexpr auto maximize_concurrent_compilation = true;

    constexpr auto layer_name = "MoltenVK";
    const vk::LayerSettingEXT layer_settings[] = {
        {layer_name, "MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS", vk::LayerSettingTypeEXT::eBool32, 1,
         &synchronous_queue_submits},
        {layer_name, "MVK_CONFIG_RESUME_LOST_DEVICE", vk::LayerSettingTypeEXT::eBool32, 1,
         &resume_lost_device},
        {layer_name, "MVK_CONFIG_SHOULD_MAXIMIZE_CONCURRENT_COMPILATION",
         vk::LayerSettingTypeEXT::eBool32, 1, &maximize_concurrent_compilation},
    };
    const vk::LayerSettingsCreateInfoEXT layer_settings_ci = {
        .pNext = nullptr,
        .settingCount = static_cast<uint32_t>(std::size(layer_settings)),
        .pSettings = layer_settings,
    };

    if (std::find(extensions.begin(), extensions.end(), VK_EXT_LAYER_SETTINGS_EXTENSION_NAME) !=
        extensions.end()) {
        instance_ci.pNext = &layer_settings_ci;
    }
#endif

    auto instance = vk::createInstanceUnique(instance_ci);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

    return instance;
}

vk::UniqueDebugUtilsMessengerEXT CreateDebugMessenger(vk::Instance instance) {
    const vk::DebugUtilsMessengerCreateInfoEXT msg_ci = {
        .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        .pfnUserCallback = DebugUtilsCallback,
    };
    return instance.createDebugUtilsMessengerEXTUnique(msg_ci);
}

vk::UniqueDebugReportCallbackEXT CreateDebugReportCallback(vk::Instance instance) {
    const vk::DebugReportCallbackCreateInfoEXT callback_ci = {
        .flags = vk::DebugReportFlagBitsEXT::eDebug | vk::DebugReportFlagBitsEXT::eInformation |
                 vk::DebugReportFlagBitsEXT::eError |
                 vk::DebugReportFlagBitsEXT::ePerformanceWarning |
                 vk::DebugReportFlagBitsEXT::eWarning,
        .pfnCallback = DebugReportCallback,
    };
    return instance.createDebugReportCallbackEXTUnique(callback_ci);
}

DebugCallback CreateDebugCallback(vk::Instance instance, bool& debug_utils_supported) {
    if (!Settings::values.renderer_debug) {
        return {};
    }
    const auto properties = vk::enumerateInstanceExtensionProperties();
    if (properties.empty()) {
        LOG_ERROR(Render_Vulkan, "Failed to query extension properties");
        return {};
    }
    const auto it = std::find_if(properties.begin(), properties.end(), [](const auto& prop) {
        return std::strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, prop.extensionName) == 0;
    });
    // Prefer debug util messenger if available.
    debug_utils_supported = it != properties.end();
    if (debug_utils_supported) {
        return CreateDebugMessenger(instance);
    }
    // Otherwise fallback to debug report callback.
    return CreateDebugReportCallback(instance);
}

} // namespace Vulkan
