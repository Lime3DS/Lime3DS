// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <boost/serialization/export.hpp>
#include "common/common_types.h"
#include "core/hle/kernel/object.h"

namespace Kernel {

enum class ResourceLimitCategory : u8 {
    Application = 0,
    SysApplet = 1,
    LibApplet = 2,
    Other = 3,
};

enum class ResourceLimitType : u32 {
    Priority = 0,
    Commit = 1,
    Thread = 2,
    Event = 3,
    Mutex = 4,
    Semaphore = 5,
    Timer = 6,
    SharedMemory = 7,
    AddressArbiter = 8,
    CpuTime = 9,
    Max = 10,
};

class ResourceLimit final : public Object {
public:
    explicit ResourceLimit(KernelSystem& kernel);
    ~ResourceLimit() override;

    /**
     * Creates a resource limit object.
     */
    static std::shared_ptr<ResourceLimit> Create(KernelSystem& kernel,
                                                 std::string name = "Unknown");

    std::string GetTypeName() const override {
        return "ResourceLimit";
    }
    std::string GetName() const override {
        return m_name;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::ResourceLimit;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    s32 GetCurrentValue(ResourceLimitType type) const;
    s32 GetLimitValue(ResourceLimitType type) const;

    void SetLimitValue(ResourceLimitType name, s32 value);

    bool Reserve(ResourceLimitType type, s32 amount);
    bool Release(ResourceLimitType type, s32 amount);

private:
    using ResourceArray = std::array<s32, static_cast<std::size_t>(ResourceLimitType::Max)>;
    ResourceArray m_limit_values{};
    ResourceArray m_current_values{};
    std::string m_name;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

class ResourceLimitList {
public:
    explicit ResourceLimitList(KernelSystem& kernel);
    ~ResourceLimitList();

    /**
     * Retrieves the resource limit associated with the specified resource limit category.
     * @param category The resource limit category
     * @returns The resource limit associated with the category
     */
    std::shared_ptr<ResourceLimit> GetForCategory(ResourceLimitCategory category);

private:
    std::array<std::shared_ptr<ResourceLimit>, 4> resource_limits;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::ResourceLimit)
BOOST_CLASS_EXPORT_KEY(Kernel::ResourceLimitList)
CONSTRUCT_KERNEL_OBJECT(Kernel::ResourceLimit)
CONSTRUCT_KERNEL_OBJECT(Kernel::ResourceLimitList)
