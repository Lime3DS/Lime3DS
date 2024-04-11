// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <span>
#include <boost/container/static_vector.hpp>
#include <fmt/format.h>

#include "common/assert.h"
#include "common/settings.h"
#include "core/frontend/emu_window.h"
#include "core/telemetry_session.h"
#include "video_core/custom_textures/custom_format.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_platform.h"

#include <vk_mem_alloc.h>

namespace Vulkan {

namespace {

vk::Format MakeFormat(VideoCore::PixelFormat format) {
    switch (format) {
    case VideoCore::PixelFormat::RGBA8:
        return vk::Format::eR8G8B8A8Unorm;
    case VideoCore::PixelFormat::RGB8:
        return vk::Format::eB8G8R8Unorm;
    case VideoCore::PixelFormat::RGB5A1:
        return vk::Format::eR5G5B5A1UnormPack16;
    case VideoCore::PixelFormat::RGB565:
        return vk::Format::eR5G6B5UnormPack16;
    case VideoCore::PixelFormat::RGBA4:
        return vk::Format::eR4G4B4A4UnormPack16;
    case VideoCore::PixelFormat::D16:
        return vk::Format::eD16Unorm;
    case VideoCore::PixelFormat::D24:
        return vk::Format::eX8D24UnormPack32;
    case VideoCore::PixelFormat::D24S8:
        return vk::Format::eD24UnormS8Uint;
    case VideoCore::PixelFormat::Invalid:
        LOG_ERROR(Render_Vulkan, "Unknown texture format {}!", format);
        return vk::Format::eUndefined;
    default:
        return vk::Format::eR8G8B8A8Unorm; ///< Use default case for the texture formats
    }
}

vk::Format MakeCustomFormat(VideoCore::CustomPixelFormat format) {
    switch (format) {
    case VideoCore::CustomPixelFormat::RGBA8:
        return vk::Format::eR8G8B8A8Unorm;
    case VideoCore::CustomPixelFormat::BC1:
        return vk::Format::eBc1RgbaUnormBlock;
    case VideoCore::CustomPixelFormat::BC3:
        return vk::Format::eBc3UnormBlock;
    case VideoCore::CustomPixelFormat::BC5:
        return vk::Format::eBc5UnormBlock;
    case VideoCore::CustomPixelFormat::BC7:
        return vk::Format::eBc7UnormBlock;
    case VideoCore::CustomPixelFormat::ASTC4:
        return vk::Format::eAstc4x4UnormBlock;
    case VideoCore::CustomPixelFormat::ASTC6:
        return vk::Format::eAstc6x6UnormBlock;
    case VideoCore::CustomPixelFormat::ASTC8:
        return vk::Format::eAstc8x6UnormBlock;
    default:
        LOG_ERROR(Render_Vulkan, "Unknown custom format {}", format);
    }
    return vk::Format::eR8G8B8A8Unorm;
}

vk::Format MakeAttributeFormat(Pica::PipelineRegs::VertexAttributeFormat format, u32 count,
                               bool scaled = true) {
    static constexpr std::array attrib_formats_scaled = {
        vk::Format::eR8Sscaled,        vk::Format::eR8G8Sscaled,
        vk::Format::eR8G8B8Sscaled,    vk::Format::eR8G8B8A8Sscaled,
        vk::Format::eR8Uscaled,        vk::Format::eR8G8Uscaled,
        vk::Format::eR8G8B8Uscaled,    vk::Format::eR8G8B8A8Uscaled,
        vk::Format::eR16Sscaled,       vk::Format::eR16G16Sscaled,
        vk::Format::eR16G16B16Sscaled, vk::Format::eR16G16B16A16Sscaled,
        vk::Format::eR32Sfloat,        vk::Format::eR32G32Sfloat,
        vk::Format::eR32G32B32Sfloat,  vk::Format::eR32G32B32A32Sfloat,
    };
    static constexpr std::array attrib_formats_int = {
        vk::Format::eR8Sint,          vk::Format::eR8G8Sint,
        vk::Format::eR8G8B8Sint,      vk::Format::eR8G8B8A8Sint,
        vk::Format::eR8Uint,          vk::Format::eR8G8Uint,
        vk::Format::eR8G8B8Uint,      vk::Format::eR8G8B8A8Uint,
        vk::Format::eR16Sint,         vk::Format::eR16G16Sint,
        vk::Format::eR16G16B16Sint,   vk::Format::eR16G16B16A16Sint,
        vk::Format::eR32Sfloat,       vk::Format::eR32G32Sfloat,
        vk::Format::eR32G32B32Sfloat, vk::Format::eR32G32B32A32Sfloat,
    };

    const u32 index = static_cast<u32>(format);
    return (scaled ? attrib_formats_scaled : attrib_formats_int)[index * 4 + count - 1];
}

vk::ImageAspectFlags MakeAspect(VideoCore::SurfaceType type) {
    switch (type) {
    case VideoCore::SurfaceType::Color:
    case VideoCore::SurfaceType::Texture:
    case VideoCore::SurfaceType::Fill:
        return vk::ImageAspectFlagBits::eColor;
    case VideoCore::SurfaceType::Depth:
        return vk::ImageAspectFlagBits::eDepth;
    case VideoCore::SurfaceType::DepthStencil:
        return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    default:
        LOG_CRITICAL(Render_Vulkan, "Invalid surface type {}", type);
        UNREACHABLE();
    }

    return vk::ImageAspectFlagBits::eColor;
}

std::vector<std::string> GetSupportedExtensions(vk::PhysicalDevice physical) {
    const std::vector extensions = physical.enumerateDeviceExtensionProperties();
    std::vector<std::string> supported_extensions;
    supported_extensions.reserve(extensions.size());
    for (const auto& extension : extensions) {
        supported_extensions.emplace_back(extension.extensionName.data());
    }
    return supported_extensions;
}

std::string GetReadableVersion(u32 version) {
    return fmt::format("{}.{}.{}", VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version),
                       VK_VERSION_PATCH(version));
}

} // Anonymous namespace

Instance::Instance(bool enable_validation, bool dump_command_buffers)
    : library{OpenLibrary()}, instance{CreateInstance(*library,
                                                      Frontend::WindowSystemType::Headless,
                                                      enable_validation, dump_command_buffers)},
      physical_devices{instance->enumeratePhysicalDevices()} {}

Instance::Instance(Core::TelemetrySession& telemetry, Frontend::EmuWindow& window,
                   u32 physical_device_index)
    : library{OpenLibrary(&window)}, instance{CreateInstance(
                                         *library, window.GetWindowInfo().type,
                                         Settings::values.renderer_debug.GetValue(),
                                         Settings::values.dump_command_buffers.GetValue())},
      debug_callback{CreateDebugCallback(*instance, debug_utils_supported)},
      physical_devices{instance->enumeratePhysicalDevices()} {
    const std::size_t num_physical_devices = static_cast<u16>(physical_devices.size());
    ASSERT_MSG(physical_device_index < num_physical_devices,
               "Invalid physical device index {} provided when only {} devices exist",
               physical_device_index, num_physical_devices);

    physical_device = physical_devices[physical_device_index];
    available_extensions = GetSupportedExtensions(physical_device);
    properties = physical_device.getProperties();
    if (properties.apiVersion < TargetVulkanApiVersion) {
        throw std::runtime_error(fmt::format(
            "Vulkan {}.{} is required, but only {}.{} is supported by device!",
            VK_VERSION_MAJOR(TargetVulkanApiVersion), VK_VERSION_MINOR(TargetVulkanApiVersion),
            VK_VERSION_MAJOR(properties.apiVersion), VK_VERSION_MINOR(properties.apiVersion)));
    }

    CollectTelemetryParameters(telemetry);
    CreateDevice();
    CollectToolingInfo();
    CreateFormatTable();
    CreateCustomFormatTable();
    CreateAttribTable();
}

Instance::~Instance() {
    vmaDestroyAllocator(allocator);
}

const FormatTraits& Instance::GetTraits(VideoCore::PixelFormat pixel_format) const {
    if (pixel_format == VideoCore::PixelFormat::Invalid) [[unlikely]] {
        return null_traits;
    }
    return format_table[static_cast<u32>(pixel_format)];
}

const FormatTraits& Instance::GetTraits(VideoCore::CustomPixelFormat pixel_format) const {
    return custom_format_table[static_cast<u32>(pixel_format)];
}

const FormatTraits& Instance::GetTraits(Pica::PipelineRegs::VertexAttributeFormat format,
                                        u32 count) const {
    if (count == 0) [[unlikely]] {
        ASSERT_MSG(false, "Unable to retrieve traits for invalid attribute component count");
    }
    const u32 index = static_cast<u32>(format);
    return attrib_table[index * 4 + count - 1];
}

std::string Instance::GetDriverVersionName() {
    // Extracted from
    // https://github.com/SaschaWillems/vulkan.gpuinfo.org/blob/5dddea46ea1120b0df14eef8f15ff8e318e35462/functions.php#L308-L314
    const u32 version = properties.driverVersion;
    if (driver_id == vk::DriverId::eNvidiaProprietary) {
        const u32 major = (version >> 22) & 0x3ff;
        const u32 minor = (version >> 14) & 0x0ff;
        const u32 secondary = (version >> 6) & 0x0ff;
        const u32 tertiary = version & 0x003f;
        return fmt::format("{}.{}.{}.{}", major, minor, secondary, tertiary);
    }
    if (driver_id == vk::DriverId::eIntelProprietaryWindows) {
        const u32 major = version >> 14;
        const u32 minor = version & 0x3fff;
        return fmt::format("{}.{}", major, minor);
    }
    return GetReadableVersion(version);
}

FormatTraits Instance::DetermineTraits(VideoCore::PixelFormat pixel_format, vk::Format format) {
    const vk::ImageAspectFlags format_aspect = MakeAspect(VideoCore::GetFormatType(pixel_format));
    const vk::FormatProperties format_properties = physical_device.getFormatProperties(format);

    const vk::FormatFeatureFlagBits attachment_usage =
        (format_aspect & vk::ImageAspectFlagBits::eDepth)
            ? vk::FormatFeatureFlagBits::eDepthStencilAttachment
            : vk::FormatFeatureFlagBits::eColorAttachmentBlend;

    const vk::FormatFeatureFlags storage_usage = vk::FormatFeatureFlagBits::eStorageImage;
    const vk::FormatFeatureFlags transfer_usage = vk::FormatFeatureFlagBits::eSampledImage;
    const vk::FormatFeatureFlags blit_usage =
        vk::FormatFeatureFlagBits::eBlitSrc | vk::FormatFeatureFlagBits::eBlitDst;

    const bool supports_transfer =
        (format_properties.optimalTilingFeatures & transfer_usage) == transfer_usage;
    const bool supports_blit = (format_properties.optimalTilingFeatures & blit_usage) == blit_usage;
    const bool supports_attachment =
        (format_properties.optimalTilingFeatures & attachment_usage) == attachment_usage &&
        pixel_format != VideoCore::PixelFormat::RGB8;
    const bool supports_storage =
        (format_properties.optimalTilingFeatures & storage_usage) == storage_usage;
    const bool needs_conversion =
        // Requires component flip.
        pixel_format == VideoCore::PixelFormat::RGBA8 ||
        // Requires (de)interleaving.
        pixel_format == VideoCore::PixelFormat::D24S8;

    // Find the most inclusive usage flags for this format
    vk::ImageUsageFlags best_usage{};
    if (supports_blit || supports_transfer) {
        best_usage |= vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
                      vk::ImageUsageFlagBits::eTransferSrc;
    }
    // Attachment flag is only needed for color and depth formats.
    if (supports_attachment &&
        VideoCore::GetFormatType(pixel_format) != VideoCore::SurfaceType::Texture) {
        best_usage |= (format_aspect & vk::ImageAspectFlagBits::eDepth)
                          ? vk::ImageUsageFlagBits::eDepthStencilAttachment
                          : vk::ImageUsageFlagBits::eColorAttachment;
    }
    // Storage flag is only needed for shadow rendering with RGBA8 texture.
    // Keeping it disables can boost performance on mobile drivers.
    if (supports_storage && pixel_format == VideoCore::PixelFormat::RGBA8) {
        best_usage |= vk::ImageUsageFlagBits::eStorage;
    }

    return FormatTraits{
        .transfer_support = supports_transfer,
        .blit_support = supports_blit,
        .attachment_support = supports_attachment,
        .storage_support = supports_storage,
        .needs_conversion = needs_conversion,
        .usage = best_usage,
        .aspect = format_aspect,
        .native = format,
    };
}

void Instance::CreateFormatTable() {
    constexpr std::array pixel_formats = {
        VideoCore::PixelFormat::RGBA8,  VideoCore::PixelFormat::RGB8,
        VideoCore::PixelFormat::RGB5A1, VideoCore::PixelFormat::RGB565,
        VideoCore::PixelFormat::RGBA4,  VideoCore::PixelFormat::IA8,
        VideoCore::PixelFormat::RG8,    VideoCore::PixelFormat::I8,
        VideoCore::PixelFormat::A8,     VideoCore::PixelFormat::IA4,
        VideoCore::PixelFormat::I4,     VideoCore::PixelFormat::A4,
        VideoCore::PixelFormat::ETC1,   VideoCore::PixelFormat::ETC1A4,
        VideoCore::PixelFormat::D16,    VideoCore::PixelFormat::D24,
        VideoCore::PixelFormat::D24S8,
    };

    for (const auto& pixel_format : pixel_formats) {
        const vk::Format format = MakeFormat(pixel_format);
        FormatTraits traits = DetermineTraits(pixel_format, format);

        const bool is_suitable =
            traits.transfer_support && traits.attachment_support &&
            (traits.blit_support || traits.aspect & vk::ImageAspectFlagBits::eDepth);

        // Fall back if the native format is not suitable.
        if (!is_suitable) {
            // Always fallback to RGBA8 or D32(S8) for convenience
            auto fallback = vk::Format::eR8G8B8A8Unorm;
            if (traits.aspect & vk::ImageAspectFlagBits::eDepth) {
                fallback = vk::Format::eD32Sfloat;
                if (traits.aspect & vk::ImageAspectFlagBits::eStencil) {
                    fallback = vk::Format::eD32SfloatS8Uint;
                }
            }
            LOG_WARNING(Render_Vulkan, "Format {} unsupported, falling back unconditionally to {}",
                        vk::to_string(format), vk::to_string(fallback));
            traits = DetermineTraits(pixel_format, fallback);
            // Always requires conversion if backing format does not match.
            traits.needs_conversion = true;
        }

        const u32 index = static_cast<u32>(pixel_format);
        format_table[index] = traits;
    }
}

void Instance::CreateCustomFormatTable() {
    // The traits are the same for RGBA8
    custom_format_table[0] = format_table[static_cast<u32>(VideoCore::PixelFormat::RGBA8)];

    constexpr std::array custom_formats = {
        VideoCore::CustomPixelFormat::BC1,   VideoCore::CustomPixelFormat::BC3,
        VideoCore::CustomPixelFormat::BC5,   VideoCore::CustomPixelFormat::BC7,
        VideoCore::CustomPixelFormat::ASTC4, VideoCore::CustomPixelFormat::ASTC6,
        VideoCore::CustomPixelFormat::ASTC8,
    };

    for (const auto& custom_format : custom_formats) {
        const vk::Format format = MakeCustomFormat(custom_format);
        const vk::FormatProperties format_properties = physical_device.getFormatProperties(format);

        // Compressed formats don't support blit_dst in general so just check for transfer
        const vk::FormatFeatureFlags transfer_usage = vk::FormatFeatureFlagBits::eSampledImage;
        const bool supports_transfer =
            (format_properties.optimalTilingFeatures & transfer_usage) == transfer_usage;

        vk::ImageUsageFlags best_usage{};
        if (supports_transfer) {
            best_usage |= vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
                          vk::ImageUsageFlagBits::eTransferSrc;
        }

        const u32 index = static_cast<u32>(custom_format);
        custom_format_table[index] = FormatTraits{
            .transfer_support = supports_transfer,
            .usage = best_usage,
            .aspect = vk::ImageAspectFlagBits::eColor,
            .native = format,
        };
    }
}

void Instance::DetermineEmulation(Pica::PipelineRegs::VertexAttributeFormat format,
                                  bool& needs_cast) {
    // Check if (u)scaled formats can be used to emulate the 3 component format
    vk::Format four_comp_format = MakeAttributeFormat(format, 4);
    vk::FormatProperties format_properties = physical_device.getFormatProperties(four_comp_format);
    needs_cast = !(format_properties.bufferFeatures & vk::FormatFeatureFlagBits::eVertexBuffer);
}

void Instance::CreateAttribTable() {
    constexpr std::array attrib_formats = {
        Pica::PipelineRegs::VertexAttributeFormat::BYTE,
        Pica::PipelineRegs::VertexAttributeFormat::UBYTE,
        Pica::PipelineRegs::VertexAttributeFormat::SHORT,
        Pica::PipelineRegs::VertexAttributeFormat::FLOAT,
    };

    for (const auto& format : attrib_formats) {
        for (u32 count = 1; count <= 4; count++) {
            bool needs_cast{false};
            bool needs_emulation{false};
            vk::Format attrib_format = MakeAttributeFormat(format, count);
            vk::FormatProperties format_properties =
                physical_device.getFormatProperties(attrib_format);
            if (!(format_properties.bufferFeatures & vk::FormatFeatureFlagBits::eVertexBuffer)) {
                needs_cast = true;
                attrib_format = MakeAttributeFormat(format, count, false);
                format_properties = physical_device.getFormatProperties(attrib_format);
                if (!(format_properties.bufferFeatures &
                      vk::FormatFeatureFlagBits::eVertexBuffer)) {
                    ASSERT_MSG(
                        count == 3,
                        "Vertex attribute emulation is only supported for 3 component formats");
                    DetermineEmulation(format, needs_cast);
                    needs_emulation = true;
                }
            }

            const u32 index = static_cast<u32>(format) * 4 + count - 1;
            attrib_table[index] = FormatTraits{
                .needs_conversion = needs_cast,
                .needs_emulation = needs_emulation,
                .native = attrib_format,
            };
        }
    }
}

bool Instance::CreateDevice() {
    const vk::StructureChain feature_chain = physical_device.getFeatures2<
        vk::PhysicalDeviceFeatures2, vk::PhysicalDevicePortabilitySubsetFeaturesKHR,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
        vk::PhysicalDeviceExtendedDynamicState2FeaturesEXT,
        vk::PhysicalDeviceExtendedDynamicState3FeaturesEXT,
        vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR,
        vk::PhysicalDeviceCustomBorderColorFeaturesEXT, vk::PhysicalDeviceIndexTypeUint8FeaturesEXT,
        vk::PhysicalDeviceFragmentShaderInterlockFeaturesEXT,
        vk::PhysicalDevicePipelineCreationCacheControlFeaturesEXT,
        vk::PhysicalDeviceFragmentShaderBarycentricFeaturesKHR>();
    const vk::StructureChain properties_chain =
        physical_device.getProperties2<vk::PhysicalDeviceProperties2,
                                       vk::PhysicalDevicePortabilitySubsetPropertiesKHR,
                                       vk::PhysicalDeviceExternalMemoryHostPropertiesEXT>();

    features = feature_chain.get().features;
    if (available_extensions.empty()) {
        LOG_CRITICAL(Render_Vulkan, "No extensions supported by device.");
        return false;
    }

    boost::container::static_vector<const char*, 13> enabled_extensions;
    const auto add_extension = [&](std::string_view extension, bool blacklist = false,
                                   std::string_view reason = "") -> bool {
        const auto result =
            std::find_if(available_extensions.begin(), available_extensions.end(),
                         [&](const std::string& name) { return name == extension; });

        if (result != available_extensions.end() && !blacklist) {
            LOG_INFO(Render_Vulkan, "Enabling extension: {}", extension);
            enabled_extensions.push_back(extension.data());
            return true;
        } else if (blacklist) {
            LOG_WARNING(Render_Vulkan, "Extension {} has been blacklisted because {}", extension,
                        reason);
            return false;
        }

        LOG_WARNING(Render_Vulkan, "Extension {} unavailable.", extension);
        return false;
    };

    const bool is_nvidia = driver_id == vk::DriverIdKHR::eNvidiaProprietary;
    const bool is_moltenvk = driver_id == vk::DriverIdKHR::eMoltenvk;
    const bool is_arm = driver_id == vk::DriverIdKHR::eArmProprietary;
    const bool is_qualcomm = driver_id == vk::DriverIdKHR::eQualcommProprietary;
    const bool is_turnip = driver_id == vk::DriverIdKHR::eMesaTurnip;

    add_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    image_format_list = add_extension(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME);
    shader_stencil_export = add_extension(VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME);
    external_memory_host = add_extension(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
    tooling_info = add_extension(VK_EXT_TOOLING_INFO_EXTENSION_NAME);
    const bool has_timeline_semaphores =
        add_extension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, is_qualcomm || is_turnip,
                      "it is broken on Qualcomm drivers");
    const bool has_portability_subset = add_extension(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    const bool has_extended_dynamic_state =
        add_extension(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME, is_arm || is_qualcomm,
                      "it is broken on Qualcomm and ARM drivers");
    const bool has_custom_border_color =
        add_extension(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME, is_qualcomm,
                      "it is broken on most Qualcomm driver versions");
    const bool has_index_type_uint8 = add_extension(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME);
    const bool has_fragment_shader_interlock =
        add_extension(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME, is_nvidia,
                      "it is broken on Nvidia drivers");
    const bool has_pipeline_creation_cache_control =
        add_extension(VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME, is_nvidia,
                      "it is broken on Nvidia drivers");
    const bool has_fragment_shader_barycentric =
        add_extension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME, is_moltenvk,
                      "the PerVertexKHR attribute is not supported by MoltenVK");

    const auto family_properties = physical_device.getQueueFamilyProperties();
    if (family_properties.empty()) {
        LOG_CRITICAL(Render_Vulkan, "Physical device reported no queues.");
        return false;
    }

    bool graphics_queue_found = false;
    for (std::size_t i = 0; i < family_properties.size(); i++) {
        const u32 index = static_cast<u32>(i);
        if (family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            queue_family_index = index;
            graphics_queue_found = true;
        }
    }

    if (!graphics_queue_found) {
        LOG_CRITICAL(Render_Vulkan, "Unable to find graphics and/or present queues.");
        return false;
    }

    static constexpr std::array<f32, 1> queue_priorities = {1.0f};

    const vk::DeviceQueueCreateInfo queue_info = {
        .queueFamilyIndex = queue_family_index,
        .queueCount = static_cast<u32>(queue_priorities.size()),
        .pQueuePriorities = queue_priorities.data(),
    };

    vk::StructureChain device_chain = {
        vk::DeviceCreateInfo{
            .queueCreateInfoCount = 1u,
            .pQueueCreateInfos = &queue_info,
            .enabledExtensionCount = static_cast<u32>(enabled_extensions.size()),
            .ppEnabledExtensionNames = enabled_extensions.data(),
        },
        vk::PhysicalDeviceFeatures2{
            .features{
                .robustBufferAccess = features.robustBufferAccess,
                .geometryShader = features.geometryShader,
                .logicOp = features.logicOp,
                .samplerAnisotropy = features.samplerAnisotropy,
                .fragmentStoresAndAtomics = features.fragmentStoresAndAtomics,
                .shaderClipDistance = features.shaderClipDistance,
            },
        },
        vk::PhysicalDevicePortabilitySubsetFeaturesKHR{},
        vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR{},
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT{},
        vk::PhysicalDeviceExtendedDynamicState2FeaturesEXT{},
        vk::PhysicalDeviceExtendedDynamicState3FeaturesEXT{},
        vk::PhysicalDeviceCustomBorderColorFeaturesEXT{},
        vk::PhysicalDeviceIndexTypeUint8FeaturesEXT{},
        vk::PhysicalDeviceFragmentShaderInterlockFeaturesEXT{},
        vk::PhysicalDevicePipelineCreationCacheControlFeaturesEXT{},
        vk::PhysicalDeviceFragmentShaderBarycentricFeaturesKHR{},
    };

#define PROP_GET(structName, prop, property) property = properties_chain.get<structName>().prop;

#define FEAT_SET(structName, feature, property)                                                    \
    if (feature_chain.get<structName>().feature) {                                                 \
        property = true;                                                                           \
        device_chain.get<structName>().feature = true;                                             \
    } else {                                                                                       \
        property = false;                                                                          \
        device_chain.get<structName>().feature = false;                                            \
    }

    if (has_portability_subset) {
        FEAT_SET(vk::PhysicalDevicePortabilitySubsetFeaturesKHR, triangleFans,
                 triangle_fan_supported)
        FEAT_SET(vk::PhysicalDevicePortabilitySubsetFeaturesKHR, imageViewFormatReinterpretation,
                 image_view_reinterpretation)
        PROP_GET(vk::PhysicalDevicePortabilitySubsetPropertiesKHR,
                 minVertexInputBindingStrideAlignment, min_vertex_stride_alignment)
    } else {
        device_chain.unlink<vk::PhysicalDevicePortabilitySubsetFeaturesKHR>();
    }

    if (has_timeline_semaphores) {
        FEAT_SET(vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR, timelineSemaphore,
                 timeline_semaphores)
    } else {
        device_chain.unlink<vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR>();
    }

    if (has_index_type_uint8) {
        FEAT_SET(vk::PhysicalDeviceIndexTypeUint8FeaturesEXT, indexTypeUint8, index_type_uint8)
    } else {
        device_chain.unlink<vk::PhysicalDeviceIndexTypeUint8FeaturesEXT>();
    }

    if (has_fragment_shader_interlock) {
        FEAT_SET(vk::PhysicalDeviceFragmentShaderInterlockFeaturesEXT, fragmentShaderPixelInterlock,
                 fragment_shader_interlock)
    } else {
        device_chain.unlink<vk::PhysicalDeviceFragmentShaderInterlockFeaturesEXT>();
    }

    if (has_extended_dynamic_state) {
        FEAT_SET(vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT, extendedDynamicState,
                 extended_dynamic_state)
    } else {
        device_chain.unlink<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
    }

    if (has_custom_border_color) {
        FEAT_SET(vk::PhysicalDeviceCustomBorderColorFeaturesEXT, customBorderColors,
                 custom_border_color)
        FEAT_SET(vk::PhysicalDeviceCustomBorderColorFeaturesEXT, customBorderColorWithoutFormat,
                 custom_border_color)
    } else {
        device_chain.unlink<vk::PhysicalDeviceCustomBorderColorFeaturesEXT>();
    }

    if (has_pipeline_creation_cache_control) {
        FEAT_SET(vk::PhysicalDevicePipelineCreationCacheControlFeaturesEXT,
                 pipelineCreationCacheControl, pipeline_creation_cache_control)
    } else {
        device_chain.unlink<vk::PhysicalDevicePipelineCreationCacheControlFeaturesEXT>();
    }

    if (external_memory_host) {
        PROP_GET(vk::PhysicalDeviceExternalMemoryHostPropertiesEXT, minImportedHostPointerAlignment,
                 min_imported_host_pointer_alignment);
    }

    if (has_fragment_shader_barycentric) {
        FEAT_SET(vk::PhysicalDeviceFragmentShaderBarycentricFeaturesKHR, fragmentShaderBarycentric,
                 fragment_shader_barycentric)
    } else {
        device_chain.unlink<vk::PhysicalDeviceFragmentShaderBarycentricFeaturesKHR>();
    }

#undef PROP_GET
#undef FEAT_SET

    try {
        device = physical_device.createDeviceUnique(device_chain.get());
    } catch (vk::ExtensionNotPresentError& err) {
        LOG_CRITICAL(Render_Vulkan, "Some required extensions are not available {}", err.what());
        return false;
    }

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

    graphics_queue = device->getQueue(queue_family_index, 0);
    present_queue = device->getQueue(queue_family_index, 0);

    CreateAllocator();
    return true;
}

void Instance::CreateAllocator() {
    const VmaVulkanFunctions functions = {
        .vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr,
    };

    const VmaAllocatorCreateInfo allocator_info = {
        .physicalDevice = physical_device,
        .device = *device,
        .pVulkanFunctions = &functions,
        .instance = *instance,
        .vulkanApiVersion = TargetVulkanApiVersion,
    };

    const VkResult result = vmaCreateAllocator(&allocator_info, &allocator);
    if (result != VK_SUCCESS) {
        UNREACHABLE_MSG("Failed to initialize VMA with error {}", result);
    }
}

void Instance::CollectTelemetryParameters(Core::TelemetrySession& telemetry) {
    const vk::StructureChain property_chain =
        physical_device
            .getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDriverProperties>();
    const vk::PhysicalDeviceDriverProperties driver =
        property_chain.get<vk::PhysicalDeviceDriverProperties>();

    driver_id = driver.driverID;
    vendor_name = driver.driverName.data();

    const std::string model_name{GetModelName()};
    const std::string driver_version = GetDriverVersionName();
    const std::string driver_name = fmt::format("{} {}", vendor_name, driver_version);
    const std::string api_version = GetReadableVersion(properties.apiVersion);
    const std::string extensions = fmt::format("{}", fmt::join(available_extensions, ", "));

    LOG_INFO(Render_Vulkan, "VK_DRIVER: {}", driver_name);
    LOG_INFO(Render_Vulkan, "VK_DEVICE: {}", model_name);
    LOG_INFO(Render_Vulkan, "VK_VERSION: {}", api_version);

    static constexpr auto field = Common::Telemetry::FieldType::UserSystem;
    telemetry.AddField(field, "GPU_Vendor", vendor_name);
    telemetry.AddField(field, "GPU_Model", model_name);
    telemetry.AddField(field, "GPU_Vulkan_Driver", driver_name);
    telemetry.AddField(field, "GPU_Vulkan_Version", api_version);
    telemetry.AddField(field, "GPU_Vulkan_Extensions", extensions);
}

void Instance::CollectToolingInfo() {
    if (!tooling_info) {
        return;
    }
    const auto tools = physical_device.getToolPropertiesEXT();
    for (const vk::PhysicalDeviceToolProperties& tool : tools) {
        const std::string_view name = tool.name;
        LOG_INFO(Render_Vulkan, "Attached debugging tool: {}", name);
        has_renderdoc = has_renderdoc || name == "RenderDoc";
        has_nsight_graphics = has_nsight_graphics || name == "NVIDIA Nsight Graphics";
    }
}

} // namespace Vulkan
