// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <variant>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

class Instance;

struct DescriptorInfoUnion {
    DescriptorInfoUnion() {}

    union {
        vk::DescriptorImageInfo image_info;
        vk::DescriptorBufferInfo buffer_info;
        vk::BufferView buffer_view;
    };
};

class DescriptorUpdateQueue {
public:
    explicit DescriptorUpdateQueue(const Instance& instance, u32 descriptor_write_max = 2048);
    ~DescriptorUpdateQueue() = default;

    void Flush();

    void AddStorageImage(vk::DescriptorSet target, u8 binding, vk::ImageView image_view,
                         vk::ImageLayout image_layout = vk::ImageLayout::eGeneral);

    void AddImageSampler(vk::DescriptorSet target, u8 binding, u8 array_index,
                         vk::ImageView image_view, vk::Sampler sampler,
                         vk::ImageLayout imageLayout = vk::ImageLayout::eGeneral);

    void AddBuffer(vk::DescriptorSet target, u8 binding, vk::Buffer buffer, vk::DeviceSize offset,
                   vk::DeviceSize size = VK_WHOLE_SIZE,
                   vk::DescriptorType type = vk::DescriptorType::eUniformBufferDynamic);

    void AddTexelBuffer(vk::DescriptorSet target, u8 binding, vk::BufferView buffer_view);

private:
    const vk::Device device;
    const u32 descriptor_write_max;
    std::unique_ptr<DescriptorInfoUnion[]> descriptor_infos;
    std::unique_ptr<vk::WriteDescriptorSet[]> descriptor_writes;
    u32 descriptor_write_end = 0;
};

} // namespace Vulkan
