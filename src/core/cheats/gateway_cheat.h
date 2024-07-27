// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <vector>
#include "common/common_types.h"
#include "core/cheats/cheat_base.h"

namespace Cheats {
class GatewayCheat final : public CheatBase {
public:
    enum class CheatType {
        Null = -0x1,
        Write32 = 0x00,
        Write16 = 0x01,
        Write8 = 0x02,
        GreaterThan32 = 0x03,
        LessThan32 = 0x04,
        EqualTo32 = 0x05,
        NotEqualTo32 = 0x06,
        GreaterThan16WithMask = 0x07,
        LessThan16WithMask = 0x08,
        EqualTo16WithMask = 0x09,
        NotEqualTo16WithMask = 0x0A,
        LoadOffset = 0x0B,
        Loop = 0x0C,
        Terminator = 0xD0,
        LoopExecuteVariant = 0xD1,
        FullTerminator = 0xD2,
        SetOffset = 0xD3,
        AddValue = 0xD4,
        SetValue = 0xD5,
        IncrementiveWrite32 = 0xD6,
        IncrementiveWrite16 = 0xD7,
        IncrementiveWrite8 = 0xD8,
        Load32 = 0xD9,
        Load16 = 0xDA,
        Load8 = 0xDB,
        AddOffset = 0xDC,
        Joker = 0xDD,
        Patch = 0xE,
    };

    struct CheatLine {
        explicit CheatLine(const std::string& line);
        CheatType type;
        u32 address;
        u32 value;
        u32 first;
        std::string cheat_line;
        bool valid = true;
    };

    GatewayCheat(std::string name, std::vector<CheatLine> cheat_lines, std::string comments);
    GatewayCheat(std::string name, std::string code, std::string comments);
    ~GatewayCheat();

    void Execute(Core::System& system) const override;

    bool IsEnabled() const override;
    void SetEnabled(bool enabled) override;

    std::string GetComments() const override;
    std::string GetName() const override;
    std::string GetType() const override;
    std::string GetCode() const override;
    std::string ToString() const override;

    /// Gateway cheats look like:
    ///     [Name]
    ///     12345678 90ABCDEF
    ///     12345678 90ABCDEF
    ///     (there might be multiple lines of those hex numbers)
    ///     Comment lines start with a '*'
    /// This function will pares the file for such structures
    static std::vector<std::shared_ptr<CheatBase>> LoadFile(const std::string& filepath);

private:
    std::atomic<bool> enabled = false;
    const std::string name;
    std::vector<CheatLine> cheat_lines;
    const std::string comments;
};
} // namespace Cheats
