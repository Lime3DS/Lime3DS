// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include <array>
#include "common/common_types.h"

namespace Network {
namespace ArticBaseCommon {
enum class MethodState : int {
    PARSING_INPUT = 0,
    PARAMETER_TYPE_MISMATCH = 1,
    PARAMETER_COUNT_MISMATCH = 2,
    BIG_BUFFER_READ_FAIL = 3,
    BIG_BUFFER_WRITE_FAIL = 4,
    OUT_OF_MEMORY = 5,

    GENERATING_OUTPUT = 6,
    UNEXPECTED_PARSING_INPUT = 7,
    OUT_OF_MEMORY_OUTPUT = 8,

    INTERNAL_METHOD_ERROR = 9,
    FINISHED = 10,
};
enum class RequestParameterType : u16 {
    IN_INTEGER_8 = 0,
    IN_INTEGER_16 = 1,
    IN_INTEGER_32 = 2,
    IN_INTEGER_64 = 3,
    IN_SMALL_BUFFER = 4,
    IN_BIG_BUFFER = 5,
};
struct RequestParameter {
    RequestParameterType type{};
    union {
        u16 parameterSize{};
        u16 bigBufferID;
    };

    char data[0x1C]{};
};
struct RequestPacket {
    u32 requestID{};
    std::array<char, 0x20> method{};
    u32 parameterCount{};
};
static_assert(sizeof(RequestPacket) == 0x28);

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
struct Buffer {
    u32 bufferID;
    u32 bufferSize;

    char data[];
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

struct ResponseMethod {
    enum class ArticResult : u32 {
        SUCCESS = 0,
        METHOD_NOT_FOUND = 1,
        METHOD_ERROR = 2,
        PROVIDE_INPUT = 3,
    };
    ArticResult articResult{};
    union {
        int methodResult{};
        int provideInputBufferID;
    };
    int bufferSize{};
    u8 padding[0x10]{};
};

struct DataPacket {
    DataPacket() {}
    u32 requestID{};

    union {
        char dataRaw[0x1C]{};
        ResponseMethod resp;
    };
};

static_assert(sizeof(DataPacket) == 0x20);
}; // namespace ArticBaseCommon
} // namespace Network
