// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_types.h"
#include "core/arm/arm_interface.h"
#include "core/arm/skyeye_common/arm_regformat.h"
#include "core/arm/skyeye_common/armstate.h"

namespace Memory {
class MemorySystem;
}

namespace Core {

class System;

class ARM_DynCom final : public ARM_Interface {
public:
    explicit ARM_DynCom(Core::System& system, Memory::MemorySystem& memory,
                        PrivilegeMode initial_mode, u32 id,
                        std::shared_ptr<Core::Timing::Timer> timer);
    ~ARM_DynCom() override;

    void Run() override;
    void Step() override;

    void ClearInstructionCache() override;
    void InvalidateCacheRange(u32 start_address, std::size_t length) override;
    void ClearExclusiveState() override {};

    void SetPC(u32 pc) override;
    u32 GetPC() const override;
    u32 GetReg(int index) const override;
    void SetReg(int index, u32 value) override;
    u32 GetVFPReg(int index) const override;
    void SetVFPReg(int index, u32 value) override;
    u32 GetVFPSystemReg(VFPSystemRegister reg) const override;
    void SetVFPSystemReg(VFPSystemRegister reg, u32 value) override;
    u32 GetCPSR() const override;
    void SetCPSR(u32 cpsr) override;
    u32 GetCP15Register(CP15Register reg) const override;
    void SetCP15Register(CP15Register reg, u32 value) override;

    void SaveContext(ThreadContext& ctx) override;
    void LoadContext(const ThreadContext& ctx) override;

    void SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) override;
    void PrepareReschedule() override;

protected:
    std::shared_ptr<Memory::PageTable> GetPageTable() const override;

private:
    void ExecuteInstructions(u64 num_instructions);

    Core::System& system;
    std::unique_ptr<ARMul_State> state;
};

} // namespace Core
