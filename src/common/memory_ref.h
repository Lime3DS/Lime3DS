// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <span>
#include <vector>
#include <boost/serialization/export.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/vector.hpp>
#include "common/assert.h"
#include "common/common_types.h"

/// Abstract host-side memory - for example a static buffer, or local vector
class BackingMem {
public:
    virtual ~BackingMem() = default;
    virtual u8* GetPtr() = 0;
    virtual const u8* GetPtr() const = 0;
    virtual std::size_t GetSize() const = 0;

private:
    template <class Archive>
    void serialize(Archive&, const unsigned int) {}
    friend class boost::serialization::access;
};

/// Backing memory implemented by a local buffer
class BufferMem : public BackingMem {
public:
    BufferMem() = default;
    explicit BufferMem(std::size_t size) : data(size) {}

    u8* GetPtr() override {
        return data.data();
    }

    const u8* GetPtr() const override {
        return data.data();
    }

    std::size_t GetSize() const override {
        return data.size();
    }

    std::vector<u8>& Vector() {
        return data;
    }

    const std::vector<u8>& Vector() const {
        return data;
    }

private:
    std::vector<u8> data;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<BackingMem>(*this);
        ar & data;
    }
    friend class boost::serialization::access;
};

BOOST_CLASS_EXPORT_KEY(BufferMem);

/**
 * A managed reference to host-side memory.
 * Fast enough to be used everywhere instead of u8*
 * Supports serialization.
 */
class MemoryRef {
public:
    MemoryRef() = default;
    MemoryRef(std::nullptr_t) {}

    MemoryRef(std::shared_ptr<BackingMem> backing_mem_)
        : backing_mem(std::move(backing_mem_)), offset(0) {
        Init();
    }
    MemoryRef(std::shared_ptr<BackingMem> backing_mem_, u64 offset_)
        : backing_mem(std::move(backing_mem_)), offset(offset_) {
        ASSERT(offset <= backing_mem->GetSize());
        Init();
    }

    explicit operator bool() const {
        return cptr != nullptr;
    }

    operator u8*() {
        return cptr;
    }

    u8* GetPtr() {
        return cptr;
    }

    operator const u8*() const {
        return cptr;
    }

    const u8* GetPtr() const {
        return cptr;
    }

    std::span<u8> GetWriteBytes(std::size_t size) {
        return std::span{cptr, std::min(size, csize)};
    }

    template <typename T>
    std::span<const T> GetReadBytes(std::size_t size) const {
        const auto* cptr_t = reinterpret_cast<T*>(cptr);
        return std::span{cptr_t, std::min(size, csize) / sizeof(T)};
    }

    std::size_t GetSize() const {
        return csize;
    }

    MemoryRef& operator+=(u32 offset_by) {
        ASSERT(offset_by < csize);
        offset += offset_by;
        Init();
        return *this;
    }

    MemoryRef operator+(u32 offset_by) const {
        ASSERT(offset_by < csize);
        return MemoryRef(backing_mem, offset + offset_by);
    }

private:
    std::shared_ptr<BackingMem> backing_mem{};
    u64 offset{};
    // Cached values for speed
    u8* cptr{};
    std::size_t csize{};

    void Init() {
        if (backing_mem) {
            cptr = backing_mem->GetPtr() + offset;
            csize = static_cast<std::size_t>(backing_mem->GetSize() - offset);
        } else {
            cptr = nullptr;
            csize = 0;
        }
    }

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & backing_mem;
        ar & offset;
        Init();
    }
    friend class boost::serialization::access;
};
