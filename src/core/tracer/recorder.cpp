// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "common/assert.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/tracer/recorder.h"

namespace CiTrace {

Recorder::Recorder(const InitialState& initial_state) : initial_state(initial_state) {}

void Recorder::Finish(const std::string& filename) {
    // Setup CiTrace header
    CTHeader header;
    std::memcpy(header.magic, CTHeader::ExpectedMagicWord(), 4);
    header.version = CTHeader::ExpectedVersion();
    header.header_size = sizeof(CTHeader);

    // Calculate file offsets
    auto& initial = header.initial_state_offsets;

    initial.pica_registers_size = static_cast<u32>(initial_state.pica_registers.size());
    initial.lcd_registers_size = static_cast<u32>(initial_state.lcd_registers.size());
    initial.default_attributes_size = static_cast<u32>(initial_state.default_attributes.size());
    initial.vs_program_binary_size = static_cast<u32>(initial_state.vs_program_binary.size());
    initial.vs_swizzle_data_size = static_cast<u32>(initial_state.vs_swizzle_data.size());
    initial.vs_float_uniforms_size = static_cast<u32>(initial_state.vs_float_uniforms.size());
    initial.gs_program_binary_size = static_cast<u32>(initial_state.gs_program_binary.size());
    initial.gs_swizzle_data_size = static_cast<u32>(initial_state.gs_swizzle_data.size());
    initial.gs_float_uniforms_size = static_cast<u32>(initial_state.gs_float_uniforms.size());
    header.stream_size = static_cast<u32>(stream.size());

    initial.gpu_registers = sizeof(header);
    initial.lcd_registers = initial.gpu_registers + initial.gpu_registers_size * sizeof(u32);
    initial.pica_registers = initial.lcd_registers + initial.lcd_registers_size * sizeof(u32);
    ;
    initial.default_attributes = initial.pica_registers + initial.pica_registers_size * sizeof(u32);
    initial.vs_program_binary =
        initial.default_attributes + initial.default_attributes_size * sizeof(u32);
    initial.vs_swizzle_data =
        initial.vs_program_binary + initial.vs_program_binary_size * sizeof(u32);
    initial.vs_float_uniforms =
        initial.vs_swizzle_data + initial.vs_swizzle_data_size * sizeof(u32);
    initial.gs_program_binary =
        initial.vs_float_uniforms + initial.vs_float_uniforms_size * sizeof(u32);
    initial.gs_swizzle_data =
        initial.gs_program_binary + initial.gs_program_binary_size * sizeof(u32);
    initial.gs_float_uniforms =
        initial.gs_swizzle_data + initial.gs_swizzle_data_size * sizeof(u32);
    header.stream_offset = initial.gs_float_uniforms + initial.gs_float_uniforms_size * sizeof(u32);

    // Iterate through stream elements, update relevant stream element data
    for (auto& stream_element : stream) {
        switch (stream_element.data.type) {
        case MemoryLoad: {
            auto& file_offset = memory_regions[stream_element.hash];
            if (!stream_element.uses_existing_data) {
                file_offset = header.stream_offset;
            }
            stream_element.data.memory_load.file_offset = file_offset;
            break;
        }

        default:
            // Other commands don't use any extra data
            DEBUG_ASSERT(stream_element.extra_data.size() == 0);
            break;
        }
        header.stream_offset += static_cast<u32>(stream_element.extra_data.size());
    }

    try {
        // Open file and write header
        FileUtil::IOFile file(filename, "wb");
        std::size_t written = file.WriteObject(header);
        if (written != 1 || file.Tell() != initial.gpu_registers)
            throw "Failed to write header";

        // Write initial state
        written = file.WriteArray(initial_state.pica_registers.data(),
                                  initial_state.pica_registers.size());
        if (written != initial_state.pica_registers.size() ||
            file.Tell() != initial.default_attributes)
            throw "Failed to write Pica registers";

        written =
            file.WriteArray(initial_state.lcd_registers.data(), initial_state.lcd_registers.size());
        if (written != initial_state.lcd_registers.size() || file.Tell() != initial.pica_registers)
            throw "Failed to write LCD registers";

        written = file.WriteArray(initial_state.default_attributes.data(),
                                  initial_state.default_attributes.size());
        if (written != initial_state.default_attributes.size() ||
            file.Tell() != initial.vs_program_binary)
            throw "Failed to write default vertex attributes";

        written = file.WriteArray(initial_state.vs_program_binary.data(),
                                  initial_state.vs_program_binary.size());
        if (written != initial_state.vs_program_binary.size() ||
            file.Tell() != initial.vs_swizzle_data)
            throw "Failed to write vertex shader program binary";

        written = file.WriteArray(initial_state.vs_swizzle_data.data(),
                                  initial_state.vs_swizzle_data.size());
        if (written != initial_state.vs_swizzle_data.size() ||
            file.Tell() != initial.vs_float_uniforms)
            throw "Failed to write vertex shader swizzle data";

        written = file.WriteArray(initial_state.vs_float_uniforms.data(),
                                  initial_state.vs_float_uniforms.size());
        if (written != initial_state.vs_float_uniforms.size() ||
            file.Tell() != initial.gs_program_binary)
            throw "Failed to write vertex shader float uniforms";

        written = file.WriteArray(initial_state.gs_program_binary.data(),
                                  initial_state.gs_program_binary.size());
        if (written != initial_state.gs_program_binary.size() ||
            file.Tell() != initial.gs_swizzle_data)
            throw "Failed to write geomtry shader program binary";

        written = file.WriteArray(initial_state.gs_swizzle_data.data(),
                                  initial_state.gs_swizzle_data.size());
        if (written != initial_state.gs_swizzle_data.size() ||
            file.Tell() != initial.gs_float_uniforms)
            throw "Failed to write geometry shader swizzle data";

        written = file.WriteArray(initial_state.gs_float_uniforms.data(),
                                  initial_state.gs_float_uniforms.size());
        if (written != initial_state.gs_float_uniforms.size() ||
            file.Tell() != initial.gs_float_uniforms + sizeof(u32) * initial.gs_float_uniforms_size)
            throw "Failed to write geometry shader float uniforms";

        // Iterate through stream elements, write "extra data"
        for (const auto& stream_element : stream) {
            if (stream_element.extra_data.size() == 0)
                continue;

            written =
                file.WriteBytes(stream_element.extra_data.data(), stream_element.extra_data.size());
            if (written != stream_element.extra_data.size())
                throw "Failed to write extra data";
        }

        if (file.Tell() != header.stream_offset)
            throw "Unexpected end of extra data";

        // Write actual stream elements
        for (const auto& stream_element : stream) {
            if (1 != file.WriteObject(stream_element.data))
                throw "Failed to write stream element";
        }
    } catch (const char* str) {
        LOG_ERROR(HW_GPU, "Writing CiTrace file failed: {}", str);
    }
}

void Recorder::FrameFinished() {
    stream.push_back({{FrameMarker}});
}

void Recorder::MemoryAccessed(const u8* data, u32 size, u32 physical_address) {
    StreamElement element = {{MemoryLoad}};
    element.data.memory_load.size = size;
    element.data.memory_load.physical_address = physical_address;

    // Compute hash over given memory region to check if the contents are already stored internally
    boost::crc_32_type result;
    result.process_bytes(data, size);
    element.hash = result.checksum();

    element.uses_existing_data = (memory_regions.find(element.hash) != memory_regions.end());
    if (!element.uses_existing_data) {
        element.extra_data.resize(size);
        std::memcpy(element.extra_data.data(), data, size);
        memory_regions.insert({element.hash, 0}); // file offset will be initialized in Finish()
    }

    stream.push_back(element);
}

void Recorder::RegisterWritten(u32 physical_address, u32 value) {
    StreamElement element = {{RegisterWrite}};
    element.data.register_write.physical_address = physical_address;
    element.data.register_write.value = value;

    stream.push_back(element);
}

} // namespace CiTrace
