// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include "common/vector_math.h"
#include "video_core/pica/packed_attribute.h"
#include "video_core/pica_types.h"

namespace Pica {

constexpr u32 MAX_PROGRAM_CODE_LENGTH = 4096;
constexpr u32 MAX_SWIZZLE_DATA_LENGTH = 4096;

using ProgramCode = std::array<u32, MAX_PROGRAM_CODE_LENGTH>;
using SwizzleData = std::array<u32, MAX_SWIZZLE_DATA_LENGTH>;

struct Uniforms {
    alignas(16) std::array<Common::Vec4<f24>, 96> f;
    std::array<bool, 16> b;
    std::array<Common::Vec4<u8>, 4> i;

    static std::size_t GetFloatUniformOffset(u32 index) {
        return offsetof(Uniforms, f) + index * sizeof(Common::Vec4<f24>);
    }

    static std::size_t GetBoolUniformOffset(u32 index) {
        return offsetof(Uniforms, b) + index * sizeof(bool);
    }

    static std::size_t GetIntUniformOffset(u32 index) {
        return offsetof(Uniforms, i) + index * sizeof(Common::Vec4<u8>);
    }

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const u32 file_version) {
        ar & f;
        ar & b;
        ar & i;
    }
};

struct ShaderRegs;

/**
 * This structure contains the state information common for all shader units such as uniforms.
 * The geometry shaders has a unique configuration so when enabled it has its own setup.
 */
struct ShaderSetup {
public:
    explicit ShaderSetup();
    ~ShaderSetup();

    void WriteUniformBoolReg(u32 value);

    void WriteUniformIntReg(u32 index, const Common::Vec4<u8> values);

    std::optional<u32> WriteUniformFloatReg(ShaderRegs& config, u32 value);

    u64 GetProgramCodeHash();

    u64 GetSwizzleDataHash();

    void MarkProgramCodeDirty() {
        program_code_hash_dirty = true;
    }

    void MarkSwizzleDataDirty() {
        swizzle_data_hash_dirty = true;
    }

public:
    Uniforms uniforms;
    PackedAttribute uniform_queue;
    ProgramCode program_code{};
    SwizzleData swizzle_data{};
    u32 entry_point{};
    const void* cached_shader{};

private:
    bool program_code_hash_dirty{true};
    bool swizzle_data_hash_dirty{true};
    u64 program_code_hash{0xDEADC0DE};
    u64 swizzle_data_hash{0xDEADC0DE};

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const u32 file_version) {
        ar & uniforms;
        ar & uniform_queue;
        ar & program_code;
        ar & swizzle_data;
        ar & program_code_hash_dirty;
        ar & swizzle_data_hash_dirty;
        ar & program_code_hash;
        ar & swizzle_data_hash;
    }
};

} // namespace Pica
