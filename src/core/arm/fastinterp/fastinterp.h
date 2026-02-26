// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/arm/arm_interface.h"
#include "core/arm/fastinterp/fastinterp_cache.h"
#include "core/arm/fastinterp/fastinterp_types.h"

namespace Memory {
class MemorySystem;
}

namespace Core {
class System;
} // namespace Core

namespace Kernel {
class SVCContext;
}

namespace Core::FastInterp {

class ARM_FastInterp final : public ARM_Interface {
public:
    ARM_FastInterp(Core::System& system, Memory::MemorySystem& memory, u32 id,
                   std::shared_ptr<Core::Timing::Timer> timer);
    ~ARM_FastInterp() override;

    void Run() override;
    void Step() override;
    void ClearInstructionCache() override;
    void InvalidateCacheRange(u32 start_address, std::size_t length) override;
    void ClearExclusiveState() override;
    void SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) override;

    void SetPC(u32 addr) override;
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
    void PrepareReschedule() override;

    /// Block execution with chaining does not stop at single-instruction
    /// boundaries, and the PC is not kept current between memory accesses within
    /// a block, so watchpoints and memory-exception PCs are not exact.
    bool HasSingleInstructionBreakAccuracy() override {
        return false;
    }

protected:
    std::shared_ptr<Memory::PageTable> GetPageTable() const override;

private:
    /// Execute a single basic block, returns number of instructions executed
    u64 ExecuteBlock(BasicBlock* block);

    /// Decode a basic block into a pre-allocated block
    void DecodeBlockInto(BasicBlock* block, u32 pc);

    /// Fuse the just-decoded compare/test at pc with a following conditional
    /// branch (ARM also fuses ADD+CMP+Bcc). On success, rewrites inst into a
    /// fused opcode consuming both instructions and ending the block.
    bool TryFuseArm(DecodedInst& inst, u32 pc);
    bool TryFuseThumb(DecodedInst& inst, u32 pc);

    /// Decode a single ARM instruction
    void DecodeARM(u32 inst, u32 pc, DecodedInst& out);

    /// Decode a single Thumb instruction (16-bit)
    void DecodeThumb16(u16 inst, u32 pc, DecodedInst& out);

    /// Decode a single Thumb-2 instruction (32-bit)
    void DecodeThumb32(u32 inst, u32 pc, DecodedInst& out);

    /// True when the decoded instruction's handler reads regs[15] as a
    /// source operand (writes and read-back-after-write don't count)
    static bool ReadsPcAsSource(const DecodedInst& inst);

    /// Fold PC-source operands into decode-time constants where possible
    /// (pipeline_pc = pc+8 ARM, pc+4 Thumb). May rewrite the opcode entirely
    /// (e.g. ADD Rd, PC, #imm -> Mov #computed; LDR literal -> LdrLit).
    static void FoldPcSources(DecodedInst& inst, u32 pipeline_pc);

    /// Check if a condition passes
    bool CheckCondition(u8 cond);

    /// Log the instruction at pc as unhandled (once per unique encoding)
    void LogUnhandledInstruction(u32 pc);

    /// Memory access helpers (decode-time; ExecuteBlock uses the MEM_* fast paths)
    u16 ReadMemory16(u32 addr);
    u32 ReadMemory32(u32 addr);

    /// Barrel shifter operations
    u32 ShiftImm(u32 value, ShiftType type, u8 amount, bool& carry_out);
    u32 ShiftReg(u32 value, ShiftType type, u8 amount, bool& carry_out);
    u32 ShiftNoCarry(u32 value, ShiftType type, u8 amount); // Fast path, no carry

    /// Reoptimize a hot block (apply additional optimizations after detection)
    void ReoptimizeHotBlock(BasicBlock* block);

    Memory::MemorySystem& memory_;
    InterpreterState state_;
    BlockCache cache_;
    std::shared_ptr<Memory::PageTable> page_table_;

    // Per-core LDREX/STREX reservation. FastInterp handles exclusives itself
    // (there is no shared monitor without the JIT), so the common mapped-RAM
    // path inlines a page-table read plus an AtomicCompareAndSwap with no
    // out-of-line call. STREX succeeds only if the reserved word is unchanged,
    // so a concurrent write (other core, or the DSP host thread) still fails it.
    struct ExclusiveReservation {
        u32 addr = 0;
        u64 value = 0;
        u8 size = 0;
        bool valid = false;
    };
    ExclusiveReservation exclusive_{};

    // Reused across SVC calls: constructing one per call heap-allocates
    std::unique_ptr<Kernel::SVCContext> svc_context_;

    bool halted_{false};

    FusionStats fusion_stats_;

    // Flag to disable hot block reoptimization (for benchmarking)
    bool disable_reoptimization_{false};

public:
    /// Get fusion statistics for profiling
    const FusionStats& GetFusionStats() const {
        return fusion_stats_;
    }

    /// Reset fusion statistics
    void ResetFusionStats() {
        fusion_stats_.Reset();
    }

    /// Enable/disable hot block reoptimization (for benchmarking)
    void SetReoptimizationEnabled(bool enabled) {
        disable_reoptimization_ = !enabled;
    }
    bool IsReoptimizationEnabled() const {
        return !disable_reoptimization_;
    }
};

} // namespace Core::FastInterp
