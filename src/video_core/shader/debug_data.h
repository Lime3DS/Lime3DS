// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/pica_types.h"

namespace Pica::Shader {

/// Helper structure used to keep track of data useful for inspection of shader emulation
template <bool full_debugging>
struct DebugData;

template <>
struct DebugData<false> {
    // TODO: Hide these behind and interface and move them to DebugData<true>
    u32 max_offset = 0;    ///< maximum program counter ever reached
    u32 max_opdesc_id = 0; ///< maximum swizzle pattern index ever used
};

template <>
struct DebugData<true> {
    /// Records store the input and output operands of a particular instruction.
    struct Record {
        enum Type {
            // Floating point arithmetic operands
            SRC1 = 0x1,
            SRC2 = 0x2,
            SRC3 = 0x4,

            // Initial and final output operand value
            DEST_IN = 0x8,
            DEST_OUT = 0x10,

            // Current and next instruction offset (in words)
            CUR_INSTR = 0x20,
            NEXT_INSTR = 0x40,

            // Output address register value
            ADDR_REG_OUT = 0x80,

            // Result of a comparison instruction
            CMP_RESULT = 0x100,

            // Input values for conditional flow control instructions
            COND_BOOL_IN = 0x200,
            COND_CMP_IN = 0x400,

            // Input values for a loop
            LOOP_INT_IN = 0x800,
        };

        Common::Vec4<f24> src1;
        Common::Vec4<f24> src2;
        Common::Vec4<f24> src3;

        Common::Vec4<f24> dest_in;
        Common::Vec4<f24> dest_out;

        s32 address_registers[2];
        bool conditional_code[2];
        bool cond_bool;
        bool cond_cmp[2];
        Common::Vec4<u8> loop_int;

        u32 instruction_offset;
        u32 next_instruction;

        /// set of enabled fields (as a combination of Type flags)
        unsigned mask = 0;
    };

    u32 max_offset = 0;    ///< maximum program counter ever reached
    u32 max_opdesc_id = 0; ///< maximum swizzle pattern index ever used

    /// List of records for each executed shader instruction
    std::vector<DebugData<true>::Record> records;
};

/// Type alias for better readability
using DebugDataRecord = DebugData<true>::Record;

/// Helper function to set a DebugData<true>::Record field based on the template enum parameter.
template <DebugDataRecord::Type type, typename ValueType>
inline void SetField(DebugDataRecord& record, ValueType value);

template <>
inline void SetField<DebugDataRecord::SRC1>(DebugDataRecord& record, f24* value) {
    record.src1.x = value[0];
    record.src1.y = value[1];
    record.src1.z = value[2];
    record.src1.w = value[3];
}

template <>
inline void SetField<DebugDataRecord::SRC2>(DebugDataRecord& record, f24* value) {
    record.src2.x = value[0];
    record.src2.y = value[1];
    record.src2.z = value[2];
    record.src2.w = value[3];
}

template <>
inline void SetField<DebugDataRecord::SRC3>(DebugDataRecord& record, f24* value) {
    record.src3.x = value[0];
    record.src3.y = value[1];
    record.src3.z = value[2];
    record.src3.w = value[3];
}

template <>
inline void SetField<DebugDataRecord::DEST_IN>(DebugDataRecord& record, f24* value) {
    record.dest_in.x = value[0];
    record.dest_in.y = value[1];
    record.dest_in.z = value[2];
    record.dest_in.w = value[3];
}

template <>
inline void SetField<DebugDataRecord::DEST_OUT>(DebugDataRecord& record, f24* value) {
    record.dest_out.x = value[0];
    record.dest_out.y = value[1];
    record.dest_out.z = value[2];
    record.dest_out.w = value[3];
}

template <>
inline void SetField<DebugDataRecord::ADDR_REG_OUT>(DebugDataRecord& record, s32* value) {
    record.address_registers[0] = value[0];
    record.address_registers[1] = value[1];
}

template <>
inline void SetField<DebugDataRecord::CMP_RESULT>(DebugDataRecord& record, bool* value) {
    record.conditional_code[0] = value[0];
    record.conditional_code[1] = value[1];
}

template <>
inline void SetField<DebugDataRecord::COND_BOOL_IN>(DebugDataRecord& record, bool value) {
    record.cond_bool = value;
}

template <>
inline void SetField<DebugDataRecord::COND_CMP_IN>(DebugDataRecord& record, bool* value) {
    record.cond_cmp[0] = value[0];
    record.cond_cmp[1] = value[1];
}

template <>
inline void SetField<DebugDataRecord::LOOP_INT_IN>(DebugDataRecord& record,
                                                   Common::Vec4<u8> value) {
    record.loop_int = value;
}

template <>
inline void SetField<DebugDataRecord::CUR_INSTR>(DebugDataRecord& record, u32 value) {
    record.instruction_offset = value;
}

template <>
inline void SetField<DebugDataRecord::NEXT_INSTR>(DebugDataRecord& record, u32 value) {
    record.next_instruction = value;
}

/// Helper function to set debug information on the current shader iteration.
template <DebugDataRecord::Type type, typename ValueType>
inline void Record(DebugData<false>& debug_data, u32 offset, ValueType value) {
    // Debugging disabled => nothing to do
}

template <DebugDataRecord::Type type, typename ValueType>
inline void Record(DebugData<true>& debug_data, u32 offset, ValueType value) {
    if (offset >= debug_data.records.size())
        debug_data.records.resize(offset + 1);

    SetField<type, ValueType>(debug_data.records[offset], value);
    debug_data.records[offset].mask |= type;
}

} // namespace Pica::Shader
