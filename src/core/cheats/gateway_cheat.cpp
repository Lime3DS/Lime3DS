// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <span>
#include <string>
#include <vector>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/cheats/gateway_cheat.h"
#include "core/core.h"
#include "core/hle/service/hid/hid.h"
#include "core/memory.h"

namespace Cheats {

struct State {
    u32 reg = 0;
    u32 offset = 0;
    u32 if_flag = 0;
    u32 loop_count = 0;
    std::size_t loop_back_line = 0;
    std::size_t current_line_nr = 0;
    bool loop_flag = false;
};

template <typename T, typename ReadFunction, typename WriteFunction>
static inline std::enable_if_t<std::is_integral_v<T>> WriteOp(const GatewayCheat::CheatLine& line,
                                                              const State& state,
                                                              ReadFunction read_func,
                                                              WriteFunction write_func,
                                                              Core::System& system) {
    u32 addr = line.address + state.offset;
    T val = read_func(addr);
    if (val != static_cast<T>(line.value)) {
        write_func(addr, static_cast<T>(line.value));
        system.InvalidateCacheRange(addr, sizeof(T));
    }
}

template <typename T, typename ReadFunction, typename CompareFunc>
static inline std::enable_if_t<std::is_integral_v<T>> CompOp(const GatewayCheat::CheatLine& line,
                                                             State& state, ReadFunction read_func,
                                                             CompareFunc comp) {
    u32 addr = line.address + state.offset;
    T val = read_func(addr);
    if (!comp(val)) {
        state.if_flag++;
    }
}

static inline void LoadOffsetOp(Memory::MemorySystem& memory, const GatewayCheat::CheatLine& line,
                                State& state) {
    u32 addr = line.address + state.offset;
    state.offset = memory.Read32(addr);
}

static inline void LoopOp(const GatewayCheat::CheatLine& line, State& state) {
    state.loop_flag = state.loop_count < line.value;
    state.loop_count++;
    state.loop_back_line = state.current_line_nr;
}

static inline void TerminateOp(State& state) {
    if (state.if_flag > 0) {
        state.if_flag--;
    }
}

static inline void LoopExecuteVariantOp(State& state) {
    if (state.loop_flag) {
        state.current_line_nr = state.loop_back_line - 1;
    } else {
        state.loop_count = 0;
    }
}

static inline void FullTerminateOp(State& state) {
    if (state.loop_flag) {
        state.current_line_nr = state.loop_back_line - 1;
    } else {
        state.offset = 0;
        state.reg = 0;
        state.loop_count = 0;
        state.if_flag = 0;
        state.loop_flag = false;
    }
}

static inline void SetOffsetOp(const GatewayCheat::CheatLine& line, State& state) {
    state.offset = line.value;
}

static inline void AddValueOp(const GatewayCheat::CheatLine& line, State& state) {
    state.reg += line.value;
}

static inline void SetValueOp(const GatewayCheat::CheatLine& line, State& state) {
    state.reg = line.value;
}

template <typename T, typename ReadFunction, typename WriteFunction>
static inline std::enable_if_t<std::is_integral_v<T>> IncrementiveWriteOp(
    const GatewayCheat::CheatLine& line, State& state, ReadFunction read_func,
    WriteFunction write_func, Core::System& system) {
    u32 addr = line.value + state.offset;
    T val = read_func(addr);
    if (val != static_cast<T>(state.reg)) {
        write_func(addr, static_cast<T>(state.reg));
        system.InvalidateCacheRange(addr, sizeof(T));
    }
    state.offset += sizeof(T);
}

template <typename T, typename ReadFunction>
static inline std::enable_if_t<std::is_integral_v<T>> LoadOp(const GatewayCheat::CheatLine& line,
                                                             State& state, ReadFunction read_func) {

    u32 addr = line.value + state.offset;
    state.reg = read_func(addr);
}

static inline void AddOffsetOp(const GatewayCheat::CheatLine& line, State& state) {
    state.offset += line.value;
}

static inline void JokerOp(const GatewayCheat::CheatLine& line, State& state,
                           const Core::System& system) {
    u32 pad_state = system.ServiceManager()
                        .GetService<Service::HID::Module::Interface>("hid:USER")
                        ->GetModule()
                        ->GetState()
                        .hex;
    bool pressed = (pad_state & line.value) == line.value;
    if (!pressed) {
        state.if_flag++;
    }
}

static inline void PatchOp(const GatewayCheat::CheatLine& line, State& state, Core::System& system,
                           std::span<const GatewayCheat::CheatLine> cheat_lines) {
    if (state.if_flag > 0) {
        // Skip over the additional patch lines
        state.current_line_nr += static_cast<int>(std::ceil(line.value / 8.0));
        return;
    }
    u32 num_bytes = line.value;
    u32 addr = line.address + state.offset;
    system.InvalidateCacheRange(addr, num_bytes);

    bool first = true;
    u32 bit_offset = 0;
    if (num_bytes > 0)
        state.current_line_nr++; // skip over the current code
    while (num_bytes >= 4) {
        u32 tmp = first ? cheat_lines[state.current_line_nr].first
                        : cheat_lines[state.current_line_nr].value;
        if (!first && num_bytes > 4) {
            state.current_line_nr++;
        }
        first = !first;
        system.Memory().Write32(addr, tmp);
        addr += 4;
        num_bytes -= 4;
    }
    while (num_bytes > 0) {
        u32 tmp = (first ? cheat_lines[state.current_line_nr].first
                         : cheat_lines[state.current_line_nr].value) >>
                  bit_offset;
        system.Memory().Write8(addr, tmp);
        addr += 1;
        num_bytes -= 1;
        bit_offset += 8;
    }
}

GatewayCheat::CheatLine::CheatLine(const std::string& line) {
    constexpr std::size_t cheat_length = 17;
    if (line.length() != cheat_length) {
        type = CheatType::Null;
        cheat_line = line;
        LOG_ERROR(Core_Cheats, "Cheat contains invalid line: {}", line);
        valid = false;
        return;
    }
    try {
        std::string type_temp = line.substr(0, 1);
        // 0xD types have extra subtype value, i.e. 0xDA
        std::string sub_type_temp;
        if (type_temp == "D" || type_temp == "d")
            sub_type_temp = line.substr(1, 1);
        type = static_cast<CheatType>(std::stoi(type_temp + sub_type_temp, 0, 16));
        first = static_cast<u32>(std::stoul(line.substr(0, 8), 0, 16));
        address = first & 0x0FFFFFFF;
        value = static_cast<u32>(std::stoul(line.substr(9, 8), 0, 16));
        cheat_line = line;
    } catch (const std::logic_error&) {
        type = CheatType::Null;
        cheat_line = line;
        LOG_ERROR(Core_Cheats, "Cheat contains invalid line: {}", line);
        valid = false;
    }
}

GatewayCheat::GatewayCheat(std::string name_, std::vector<CheatLine> cheat_lines_,
                           std::string comments_)
    : name(std::move(name_)), cheat_lines(std::move(cheat_lines_)), comments(std::move(comments_)) {
}

GatewayCheat::GatewayCheat(std::string name_, std::string code, std::string comments_)
    : name(std::move(name_)), comments(std::move(comments_)) {

    const auto code_lines = Common::SplitString(code, '\n');

    std::vector<CheatLine> temp_cheat_lines;
    for (const std::string& line : code_lines) {
        if (!line.empty())
            temp_cheat_lines.emplace_back(line);
    }
    cheat_lines = std::move(temp_cheat_lines);
}

GatewayCheat::~GatewayCheat() = default;

void GatewayCheat::Execute(Core::System& system) const {
    State state;

    Memory::MemorySystem& memory = system.Memory();
    auto Read8 = [&memory](VAddr addr) { return memory.Read8(addr); };
    auto Read16 = [&memory](VAddr addr) { return memory.Read16(addr); };
    auto Read32 = [&memory](VAddr addr) { return memory.Read32(addr); };
    auto Write8 = [&memory](VAddr addr, u8 value) { memory.Write8(addr, value); };
    auto Write16 = [&memory](VAddr addr, u16 value) { memory.Write16(addr, value); };
    auto Write32 = [&memory](VAddr addr, u32 value) { memory.Write32(addr, value); };

    for (state.current_line_nr = 0; state.current_line_nr < cheat_lines.size();
         state.current_line_nr++) {
        auto line = cheat_lines[state.current_line_nr];
        if (state.if_flag > 0) {
            switch (line.type) {
            case CheatType::GreaterThan32:
            case CheatType::LessThan32:
            case CheatType::EqualTo32:
            case CheatType::NotEqualTo32:
            case CheatType::GreaterThan16WithMask:
            case CheatType::LessThan16WithMask:
            case CheatType::EqualTo16WithMask:
            case CheatType::NotEqualTo16WithMask:
            case CheatType::Joker:
                // Increment the if_flag to handle the end if correctly
                state.if_flag++;
                break;
            case CheatType::Patch:
                // EXXXXXXX YYYYYYYY
                // Copies YYYYYYYY bytes from (current code location + 8) to [XXXXXXXX + offset].
                // We need to call this here to skip the additional patch lines
                PatchOp(line, state, system, cheat_lines);
                break;
            case CheatType::Terminator:
                // D0000000 00000000 - ENDIF
                TerminateOp(state);
                break;
            case CheatType::FullTerminator:
                // D2000000 00000000 - END; offset = 0; reg = 0;
                FullTerminateOp(state);
                break;
            default:
                break;
            }
            // Do not execute any other op code
            continue;
        }
        switch (line.type) {
        case CheatType::Null:
            break;
        case CheatType::Write32:
            // 0XXXXXXX YYYYYYYY - word[XXXXXXX+offset] = YYYYYYYY
            WriteOp<u32>(line, state, Read32, Write32, system);
            break;
        case CheatType::Write16:
            // 1XXXXXXX 0000YYYY - half[XXXXXXX+offset] = YYYY
            WriteOp<u16>(line, state, Read16, Write16, system);
            break;
        case CheatType::Write8:
            // 2XXXXXXX 000000YY - byte[XXXXXXX+offset] = YY
            WriteOp<u8>(line, state, Read8, Write8, system);
            break;
        case CheatType::GreaterThan32:
            // 3XXXXXXX YYYYYYYY - Execute next block IF YYYYYYYY > word[XXXXXXX]   ;unsigned
            CompOp<u32>(line, state, Read32, [&line](u32 val) -> bool { return line.value > val; });
            break;
        case CheatType::LessThan32:
            // 4XXXXXXX YYYYYYYY - Execute next block IF YYYYYYYY < word[XXXXXXX]   ;unsigned
            CompOp<u32>(line, state, Read32, [&line](u32 val) -> bool { return line.value < val; });
            break;
        case CheatType::EqualTo32:
            // 5XXXXXXX YYYYYYYY - Execute next block IF YYYYYYYY == word[XXXXXXX]   ;unsigned
            CompOp<u32>(line, state, Read32,
                        [&line](u32 val) -> bool { return line.value == val; });
            break;
        case CheatType::NotEqualTo32:
            // 6XXXXXXX YYYYYYYY - Execute next block IF YYYYYYYY != word[XXXXXXX]   ;unsigned
            CompOp<u32>(line, state, Read32,
                        [&line](u32 val) -> bool { return line.value != val; });
            break;
        case CheatType::GreaterThan16WithMask:
            // 7XXXXXXX ZZZZYYYY - Execute next block IF YYYY > ((not ZZZZ) AND half[XXXXXXX])
            CompOp<u16>(line, state, Read16, [&line](u16 val) -> bool {
                return static_cast<u16>(line.value) > (static_cast<u16>(~line.value >> 16) & val);
            });
            break;
        case CheatType::LessThan16WithMask:
            // 8XXXXXXX ZZZZYYYY - Execute next block IF YYYY < ((not ZZZZ) AND half[XXXXXXX])
            CompOp<u16>(line, state, Read16, [&line](u16 val) -> bool {
                return static_cast<u16>(line.value) < (static_cast<u16>(~line.value >> 16) & val);
            });
            break;
        case CheatType::EqualTo16WithMask:
            // 9XXXXXXX ZZZZYYYY - Execute next block IF YYYY = ((not ZZZZ) AND half[XXXXXXX])
            CompOp<u16>(line, state, Read16, [&line](u16 val) -> bool {
                return static_cast<u16>(line.value) == (static_cast<u16>(~line.value >> 16) & val);
            });
            break;
        case CheatType::NotEqualTo16WithMask:
            // AXXXXXXX ZZZZYYYY - Execute next block IF YYYY <> ((not ZZZZ) AND half[XXXXXXX])
            CompOp<u16>(line, state, Read16, [&line](u16 val) -> bool {
                return static_cast<u16>(line.value) != (static_cast<u16>(~line.value >> 16) & val);
            });
            break;
        case CheatType::LoadOffset:
            // BXXXXXXX 00000000 - offset = word[XXXXXXX+offset]
            LoadOffsetOp(system.Memory(), line, state);
            break;
        case CheatType::Loop: {
            // C0000000 YYYYYYYY - LOOP next block YYYYYYYY times
            // TODO(B3N30): Support nested loops if necessary
            LoopOp(line, state);
            break;
        }
        case CheatType::Terminator: {
            // D0000000 00000000 - END IF
            TerminateOp(state);
            break;
        }
        case CheatType::LoopExecuteVariant: {
            // D1000000 00000000 - END LOOP
            LoopExecuteVariantOp(state);
            break;
        }
        case CheatType::FullTerminator: {
            // D2000000 00000000 - NEXT & Flush
            FullTerminateOp(state);
            break;
        }
        case CheatType::SetOffset: {
            // D3000000 XXXXXXXX – Sets the offset to XXXXXXXX
            SetOffsetOp(line, state);
            break;
        }
        case CheatType::AddValue: {
            // D4000000 XXXXXXXX – reg += XXXXXXXX
            AddValueOp(line, state);
            break;
        }
        case CheatType::SetValue: {
            // D5000000 XXXXXXXX – reg = XXXXXXXX
            SetValueOp(line, state);
            break;
        }
        case CheatType::IncrementiveWrite32: {
            // D6000000 XXXXXXXX – (32bit) [XXXXXXXX+offset] = reg ; offset += 4
            IncrementiveWriteOp<u32>(line, state, Read32, Write32, system);
            break;
        }
        case CheatType::IncrementiveWrite16: {
            // D7000000 XXXXXXXX – (16bit) [XXXXXXXX+offset] = reg & 0xffff ; offset += 2
            IncrementiveWriteOp<u16>(line, state, Read16, Write16, system);
            break;
        }
        case CheatType::IncrementiveWrite8: {
            // D8000000 XXXXXXXX – (16bit) [XXXXXXXX+offset] = reg & 0xff ; offset++
            IncrementiveWriteOp<u8>(line, state, Read8, Write8, system);
            break;
        }
        case CheatType::Load32: {
            // D9000000 XXXXXXXX – reg = [XXXXXXXX+offset]
            LoadOp<u32>(line, state, Read32);
            break;
        }
        case CheatType::Load16: {
            // DA000000 XXXXXXXX – reg = [XXXXXXXX+offset] & 0xFFFF
            LoadOp<u16>(line, state, Read16);
            break;
        }
        case CheatType::Load8: {
            // DB000000 XXXXXXXX – reg = [XXXXXXXX+offset] & 0xFF
            LoadOp<u8>(line, state, Read8);
            break;
        }
        case CheatType::AddOffset: {
            // DC000000 XXXXXXXX – offset + XXXXXXXX
            AddOffsetOp(line, state);
            break;
        }
        case CheatType::Joker: {
            // DD000000 XXXXXXXX – if KEYPAD has value XXXXXXXX execute next block
            JokerOp(line, state, system);
            break;
        }
        case CheatType::Patch: {
            // EXXXXXXX YYYYYYYY
            // Copies YYYYYYYY bytes from (current code location + 8) to [XXXXXXXX + offset].
            PatchOp(line, state, system, cheat_lines);
            break;
        }
        }
    }
}

bool GatewayCheat::IsEnabled() const {
    return enabled;
}

void GatewayCheat::SetEnabled(bool enabled_) {
    enabled = enabled_;
    if (enabled) {
        LOG_WARNING(Core_Cheats, "Cheats enabled. This might lead to weird behaviour or crashes");
    }
}

std::string GatewayCheat::GetComments() const {
    return comments;
}

std::string GatewayCheat::GetName() const {
    return name;
}

std::string GatewayCheat::GetType() const {
    return "Gateway";
}

std::string GatewayCheat::GetCode() const {
    std::string result;
    for (const auto& line : cheat_lines)
        result += line.cheat_line + '\n';
    return result;
}

/// A special marker used to keep track of enabled cheats
static constexpr char EnabledText[] = "*lime3ds_enabled";

std::string GatewayCheat::ToString() const {
    std::string result;
    result += '[' + name + "]\n";
    if (enabled) {
        result += EnabledText;
        result += '\n';
    }
    const auto comment_lines = Common::SplitString(comments, '\n');
    for (const auto& comment_line : comment_lines) {
        result += "*" + comment_line + '\n';
    }
    result += GetCode() + '\n';
    return result;
}

std::vector<std::shared_ptr<CheatBase>> GatewayCheat::LoadFile(const std::string& filepath) {
    std::vector<std::shared_ptr<CheatBase>> cheats;

    boost::iostreams::stream<boost::iostreams::file_descriptor_source> file;
    FileUtil::OpenFStream<std::ios_base::in>(file, filepath);
    if (!file.is_open()) {
        return cheats;
    }

    std::string comments;
    std::vector<CheatLine> cheat_lines;
    std::string name;
    bool enabled = false;

    while (!file.eof()) {
        std::string line;
        std::getline(file, line);
        line.erase(std::remove(line.begin(), line.end(), '\0'), line.end());
        line = Common::StripSpaces(line); // remove spaces at front and end
        if (line.length() >= 2 && line.front() == '[') {
            if (!cheat_lines.empty()) {
                cheats.push_back(std::make_shared<GatewayCheat>(name, cheat_lines, comments));
                cheats.back()->SetEnabled(enabled);
                enabled = false;
            }
            name = line.substr(1, line.length() - 2);
            cheat_lines.clear();
            comments.erase();
        } else if (!line.empty() && line.front() == '*') {
            if (line == EnabledText) {
                enabled = true;
            } else {
                comments += line.substr(1, line.length() - 1) + '\n';
            }
        } else if (!line.empty()) {
            cheat_lines.emplace_back(std::move(line));
        }
    }
    if (!cheat_lines.empty()) {
        cheats.push_back(std::make_shared<GatewayCheat>(name, cheat_lines, comments));
        cheats.back()->SetEnabled(enabled);
    }
    return cheats;
}
} // namespace Cheats
