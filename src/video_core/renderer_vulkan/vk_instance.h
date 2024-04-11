// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>

#include "video_core/pica/regs_pipeline.h"
#include "video_core/rasterizer_cache/pixel_format.h"
#include "video_core/renderer_vulkan/vk_platform.h"

namespace Core {
class TelemetrySession;
}

namespace Frontend {
class EmuWindow;
}

namespace VideoCore {
enum class CustomPixelFormat : u32;
}

VK_DEFINE_HANDLE(VmaAllocator)

namespace Vulkan {

struct FormatTraits {
    bool transfer_support = false;
    bool blit_support = false;
    bool attachment_support = false;
    bool storage_support = false;
    bool needs_conversion = false;
    bool needs_emulation = false;
    vk::ImageUsageFlags usage{};
    vk::ImageAspectFlags aspect;
    vk::Format native = vk::Format::eUndefined;
};

class Instance {
public:
    explicit Instance(bool validation = false, bool dump_command_buffers = false);
    explicit Instance(Core::TelemetrySession& telemetry, Frontend::EmuWindow& window,
                      u32 physical_device_index);
    ~Instance();

    /// Returns the FormatTraits struct for the provided pixel format
    const FormatTraits& GetTraits(VideoCore::PixelFormat pixel_format) const;
    const FormatTraits& GetTraits(VideoCore::CustomPixelFormat pixel_format) const;

    /// Returns the FormatTraits struct for the provided attribute format and count
    const FormatTraits& GetTraits(Pica::PipelineRegs::VertexAttributeFormat format,
                                  u32 count) const;

    /// Returns a formatted string for the driver version
    std::string GetDriverVersionName();

    /// Returns the Vulkan instance
    vk::Instance GetInstance() const {
        return *instance;
    }

    /// Returns the current physical device
    vk::PhysicalDevice GetPhysicalDevice() const {
        return physical_device;
    }

    /// Returns the Vulkan device
    vk::Device GetDevice() const {
        return *device;
    }

    /// Returns the VMA allocator handle
    VmaAllocator GetAllocator() const {
        return allocator;
    }

    /// Returns a list of the available physical devices
    std::span<const vk::PhysicalDevice> GetPhysicalDevices() const {
        return physical_devices;
    }

    /// Retrieve queue information
    u32 GetGraphicsQueueFamilyIndex() const {
        return queue_family_index;
    }

    u32 GetPresentQueueFamilyIndex() const {
        return queue_family_index;
    }

    vk::Queue GetGraphicsQueue() const {
        return graphics_queue;
    }

    vk::Queue GetPresentQueue() const {
        return present_queue;
    }

    /// Returns true when a known debugging tool is attached.
    bool HasDebuggingToolAttached() const {
        return has_renderdoc || has_nsight_graphics;
    }

    /// Returns true when VK_EXT_debug_utils is supported.
    bool IsExtDebugUtilsSupported() const {
        return debug_utils_supported;
    }

    /// Returns true if logic operations need shader emulation
    bool NeedsLogicOpEmulation() const {
        return !features.logicOp;
    }

    bool UseGeometryShaders() const {
#ifdef __ANDROID__
        // Geometry shaders are extremely expensive on tilers to avoid them at all
        // cost even if it hurts accuracy somewhat. TODO: Make this an option
        return false;
#else
        return features.geometryShader;
#endif
    }

    /// Returns true if anisotropic filtering is supported
    bool IsAnisotropicFilteringSupported() const {
        return features.samplerAnisotropy;
    }

    /// Returns true when VK_KHR_timeline_semaphore is supported
    bool IsTimelineSemaphoreSupported() const {
        return timeline_semaphores;
    }

    /// Returns true when VK_EXT_extended_dynamic_state is supported
    bool IsExtendedDynamicStateSupported() const {
        return extended_dynamic_state;
    }

    /// Returns true when VK_EXT_custom_border_color is supported
    bool IsCustomBorderColorSupported() const {
        return custom_border_color;
    }

    /// Returns true when VK_EXT_index_type_uint8 is supported
    bool IsIndexTypeUint8Supported() const {
        return index_type_uint8;
    }

    /// Returns true when VK_EXT_fragment_shader_interlock is supported
    bool IsFragmentShaderInterlockSupported() const {
        return fragment_shader_interlock;
    }

    /// Returns true when VK_KHR_image_format_list is supported
    bool IsImageFormatListSupported() const {
        return image_format_list;
    }

    /// Returns true when VK_EXT_pipeline_creation_cache_control is supported
    bool IsPipelineCreationCacheControlSupported() const {
        return pipeline_creation_cache_control;
    }

    /// Returns true when VK_EXT_shader_stencil_export is supported
    bool IsShaderStencilExportSupported() const {
        return shader_stencil_export;
    }

    /// Returns true when VK_EXT_external_memory_host is supported
    bool IsExternalMemoryHostSupported() const {
        return external_memory_host;
    }

    /// Returns true when VK_KHR_fragment_shader_barycentric is supported
    bool IsFragmentShaderBarycentricSupported() const {
        return fragment_shader_barycentric;
    }

    /// Returns the vendor ID of the physical device
    u32 GetVendorID() const {
        return properties.vendorID;
    }

    /// Returns the device ID of the physical device
    u32 GetDeviceID() const {
        return properties.deviceID;
    }

    /// Returns the driver ID.
    vk::DriverId GetDriverID() const {
        return driver_id;
    }

    /// Returns the current driver version provided in Vulkan-formatted version numbers.
    u32 GetDriverVersion() const {
        return properties.driverVersion;
    }

    /// Returns the current Vulkan API version provided in Vulkan-formatted version numbers.
    u32 ApiVersion() const {
        return properties.apiVersion;
    }

    /// Returns the vendor name reported from Vulkan.
    std::string_view GetVendorName() const {
        return vendor_name;
    }

    /// Returns the list of available extensions.
    std::span<const std::string> GetAvailableExtensions() const {
        return available_extensions;
    }

    /// Returns the device name.
    std::string_view GetModelName() const {
        return properties.deviceName;
    }

    /// Returns the pipeline cache unique identifier
    const auto GetPipelineCacheUUID() const {
        return properties.pipelineCacheUUID;
    }

    /// Returns the minimum required alignment for uniforms
    vk::DeviceSize UniformMinAlignment() const {
        return properties.limits.minUniformBufferOffsetAlignment;
    }

    /// Returns the minimum alignemt required for accessing host-mapped device memory
    vk::DeviceSize NonCoherentAtomSize() const {
        return properties.limits.nonCoherentAtomSize;
    }

    /// Returns the maximum supported elements in a texel buffer
    u32 MaxTexelBufferElements() const {
        return properties.limits.maxTexelBufferElements;
    }

    /// Returns true if shaders can declare the ClipDistance attribute
    bool IsShaderClipDistanceSupported() const {
        return features.shaderClipDistance;
    }

    /// Returns true if triangle fan is an accepted primitive topology
    bool IsTriangleFanSupported() const {
        return triangle_fan_supported;
    }

    /// Returns the minimum vertex stride alignment
    u32 GetMinVertexStrideAlignment() const {
        return min_vertex_stride_alignment;
    }

    /// Returns the minimum imported host pointer alignment
    u64 GetMinImportedHostPointerAlignment() const {
        return min_imported_host_pointer_alignment;
    }

    /// Returns true if commands should be flushed at the end of each major renderpass
    bool ShouldFlush() const {
        return driver_id == vk::DriverIdKHR::eArmProprietary ||
               driver_id == vk::DriverIdKHR::eQualcommProprietary;
    }

private:
    /// Returns the optimal supported usage for the requested format
    [[nodiscard]] FormatTraits DetermineTraits(VideoCore::PixelFormat pixel_format,
                                               vk::Format format);

    /// Determines the best available vertex attribute format emulation
    void DetermineEmulation(Pica::PipelineRegs::VertexAttributeFormat format, bool& needs_cast);

    /// Creates the format compatibility table for the current device
    void CreateFormatTable();
    void CreateCustomFormatTable();

    /// Creates the attribute format table for the current device
    void CreateAttribTable();

    /// Creates the logical device opportunistically enabling extensions
    bool CreateDevice();

    /// Creates the VMA allocator handle
    void CreateAllocator();

    /// Collects telemetry information from the device.
    void CollectTelemetryParameters(Core::TelemetrySession& telemetry);
    void CollectToolingInfo();

private:
    std::shared_ptr<Common::DynamicLibrary> library;
    vk::UniqueInstance instance;
    vk::PhysicalDevice physical_device;
    vk::UniqueDevice device;
    vk::PhysicalDeviceProperties properties;
    vk::PhysicalDeviceFeatures features;
    vk::DriverIdKHR driver_id;
    DebugCallback debug_callback;
    std::string vendor_name;
    VmaAllocator allocator{};
    vk::Queue present_queue;
    vk::Queue graphics_queue;
    std::vector<vk::PhysicalDevice> physical_devices;
    FormatTraits null_traits;
    std::array<FormatTraits, VideoCore::PIXEL_FORMAT_COUNT> format_table;
    std::array<FormatTraits, 10> custom_format_table;
    std::array<FormatTraits, 16> attrib_table;
    std::vector<std::string> available_extensions;
    u32 queue_family_index{0};
    bool triangle_fan_supported{true};
    bool image_view_reinterpretation{true};
    u32 min_vertex_stride_alignment{1};
    bool timeline_semaphores{};
    bool extended_dynamic_state{};
    bool custom_border_color{};
    bool index_type_uint8{};
    bool fragment_shader_interlock{};
    bool image_format_list{};
    bool pipeline_creation_cache_control{};
    bool fragment_shader_barycentric{};
    bool shader_stencil_export{};
    bool external_memory_host{};
    u64 min_imported_host_pointer_alignment{};
    bool tooling_info{};
    bool debug_utils_supported{};
    bool has_nsight_graphics{};
    bool has_renderdoc{};
};

} // namespace Vulkan
