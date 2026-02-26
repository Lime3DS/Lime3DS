// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <bit>
#include <cmath>
#include <cstring>

#include "common/atomic_ops.h"
#include "common/logging/log.h"
#include "core/arm/dynarmic/arm_tick_counts.h"
#include "core/arm/fastinterp/fastinterp.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/svc.h"
#include "core/memory.h"

namespace Core::FastInterp {

// Never-null page table. When no page table is set, page_pointers points at
// this zero-initialized array (8MB of BSS nullptrs, zero-fill-on-demand, touched
// only if the guest runs without a page table), so the MEM_* fast paths and the
// unmapped-PC check never need a null test on the array itself.
static u8* empty_page_table[1 << (32 - FASTINTERP_PAGE_BITS)];

// Included inside the namespace: the macros reference LazyFlags and CondPassed.
#ifdef __aarch64__
#include "core/arm/fastinterp/fastinterp_arm64.inc"
#endif

ARM_FastInterp::ARM_FastInterp(Core::System& system, Memory::MemorySystem& memory, u32 id,
                               std::shared_ptr<Core::Timing::Timer> timer)
    : ARM_Interface(id, timer), memory_(memory),
      svc_context_(std::make_unique<Kernel::SVCContext>(system)) {
    LOG_INFO(Core_ARM11, "FastInterp ARM interpreter created for core {}", id);
    state_.regs.fill(0);
    state_.vfp_regs.fill(0);
    state_.cpsr_other = 0x10; // User mode
    state_.fpscr = 0;
    state_.fpexc = 0;
    state_.thumb_mode = false;
    state_.page_pointers = empty_page_table; // Never null
}

ARM_FastInterp::~ARM_FastInterp() = default;

// Log each unique unhandled encoding once: a bad decode in a loop would
// otherwise flood the log on every block entry.
void ARM_FastInterp::LogUnhandledInstruction(u32 pc) {
    static std::array<u32, 64> seen{};
    static u32 seen_count = 0;
    const u32 raw = state_.thumb_mode ? ReadMemory16(pc) : ReadMemory32(pc);
    const u32 key = (raw & 0x0FF000F0) | (state_.thumb_mode ? 0x80000000u : 0);
    for (u32 i = 0; i < seen_count; i++) {
        if (seen[i] == key) {
            return;
        }
    }
    if (seen_count >= seen.size()) {
        return;
    }
    seen[seen_count++] = key;
    LOG_ERROR(Core_ARM11, "Unhandled instruction at PC {:08X}: raw={:08X}, thumb={}, cpsr={:08X}",
              pc, raw, state_.thumb_mode, GetCPSR());
}

void ARM_FastInterp::Run() {
    halted_ = false;

    // Run until the timer slice is exhausted, flushing ticks to the timer per block so the
    // downcount stays live. This mirrors Dynarmic (which flushes incrementally via its
    // AddTicks callback and stops on the live GetTicksRemaining) rather than DynCom's batched
    // "capture the budget once, flush at the end" model. The live downcount lets events that
    // are force-scheduled mid-slice (ForceExceptionCheck) stop execution promptly instead of
    // being overshot, keeping multi-threaded scheduling aligned with Dynarmic.
    while (!halted_ && timer->GetDowncount() > 0) {
        u32 pc = state_.PC();

        // PC in unmapped memory: consume the slice so other threads still run
        if (!state_.page_pointers[pc >> 12]) {
            static u32 unmapped_pc_count = 0;
            if (unmapped_pc_count++ < 5) {
                LOG_ERROR(Core_ARM11, "PC {:08X} in unmapped memory (thumb={}, lr={:08X})", pc,
                          state_.thumb_mode, state_.regs[14]);
            }
            timer->AddTicks(static_cast<u64>(std::max<s64>(timer->GetDowncount(), 0)));
            break;
        }

        bool cache_hit;
        BasicBlock* block = cache_.LookupOrAllocate(pc, cache_hit);
        if (cache_hit && block->IsThumb() != state_.thumb_mode) {
            cache_hit = false; // Cached block is for wrong mode, re-decode
        }
        if (!cache_hit) {
            DecodeBlockInto(block, pc);
        }

        u64 executed = ExecuteBlock(block);

        if (executed == 0 && !halted_ && state_.PC() == pc &&
            block->instructions[0].opcode == Opcode::Invalid) {
            // The block truly failed to execute: it decoded to an invalid
            // instruction, which exits without adding ticks.
            // Every other executed==0 return is the SVC handlers' deliberate
            // tick flush - and an SVC may context-switch to a thread whose PC
            // equals this block's start (threads sharing an IPC wrapper), so
            // the PC check alone is not sufficient: without the opcode guard
            // the recovery below would skip the new thread's first instruction.
            LogUnhandledInstruction(pc);
            // Advance PC past the invalid instruction to avoid infinite loop
            state_.SetPC(state_.PC() + block->instructions[0].inst_size);
            executed = 1;
        }

        // Flush this block's ticks to the timer immediately (Dynarmic-style incremental
        // accounting), keeping the downcount live for the loop condition above. SVC blocks
        // flush inside their handler and return executed==0, so they are not double-counted.
        if (executed > 0) {
            timer->AddTicks(executed);
        }
    }
}

void ARM_FastInterp::Step() {
    u32 pc = state_.PC();

    // Decode single instruction block
    BasicBlock* block = cache_.Lookup(pc);
    if (!block || block->inst_count > 1) {
        // Create a single-instruction block for stepping
        bool was_hit;
        block = cache_.LookupOrAllocate(pc, was_hit);
        block->inst_count = 0;

        u32 inst = ReadMemory32(pc);
        DecodeARM(inst, pc, block->instructions[0]);
        u16 slot = 0;
        // Resolve PC-source reads (fold to constants or PcSetup prefix),
        // matching DecodeBlockInto. Step() always decodes ARM, so pc+8.
        if (ReadsPcAsSource(block->instructions[0])) {
            FoldPcSources(block->instructions[0], pc + 8);
            if (ReadsPcAsSource(block->instructions[0])) {
                block->instructions[1] = block->instructions[0];
                DecodedInst& pfx = block->instructions[0];
                std::memset(&pfx, 0, sizeof(pfx));
                pfx.opcode = Opcode::PcSetup;
                pfx.cond = 0xE;
                pfx.imm32 = pc + 8;
                slot = 1;
            }
        }
        block->instructions[slot].ticks = static_cast<u8>(Core::TicksForInstruction(false, inst));
        // Append the EndBlock terminator: DISPATCH has no bounds check, so a
        // non-block-ending instruction must run into the terminator.
        DecodedInst& term = block->instructions[slot + 1];
        std::memset(&term, 0, sizeof(term));
        term.opcode = Opcode::EndBlock;
        term.cond = 0xE;
        block->inst_count = slot + 1;
        block->end_pc = pc + 4;
    }

    u64 executed = ExecuteBlock(block);
    timer->AddTicks(executed > 0 ? executed : 1);
}

// ============================================================================
// Computed-Goto Dispatch Loop
// ============================================================================
// Direct threading via GCC/Clang computed goto: each indirect jump gets its
// own branch predictor entry, unlike a switch's single dispatch point.
//
// Everything below (macros, lambdas, and the #included handlers) must live in
// this one function: computed-goto labels have function scope. Its codegen is
// fragile - an out-of-line call in a hot handler forces a frame that costs
// more than the call itself (measured more than once) - so keep slow paths in
// noinline helpers and benchmark any structural change here.

u64 ARM_FastInterp::ExecuteBlock(BasicBlock* block) {
// Must match Opcode order (the static_assert below checks the count)
#define HANDLER(name) &&handle_##name
    static const void* dispatch_table[] = {
        HANDLER(Invalid),
        HANDLER(Nop),
        HANDLER(EndBlock),

        // Data Processing - ALU (no flags)
        HANDLER(And),
        HANDLER(Eor),
        HANDLER(Sub),
        HANDLER(Rsb),
        HANDLER(Add),
        HANDLER(Adc),
        HANDLER(Sbc),
        HANDLER(Rsc),
        HANDLER(Tst),
        HANDLER(Teq),
        HANDLER(Cmp),
        HANDLER(Cmn),
        HANDLER(Orr),
        HANDLER(Mov),
        HANDLER(Bic),
        HANDLER(Mvn),

        // Data Processing - ALU (with flags)
        HANDLER(AndS),
        HANDLER(EorS),
        HANDLER(SubS),
        HANDLER(RsbS),
        HANDLER(AddS),
        HANDLER(AdcS),
        HANDLER(SbcS),
        HANDLER(RscS),
        HANDLER(OrrS),
        HANDLER(MovS),
        HANDLER(BicS),
        HANDLER(MvnS),

        // Multiply
        HANDLER(Mul),
        HANDLER(MulS),
        HANDLER(Mla),
        HANDLER(MlaS),
        HANDLER(Umull),
        HANDLER(UmullS),
        HANDLER(Umlal),
        HANDLER(UmlalS),
        HANDLER(Smull),
        HANDLER(SmullS),
        HANDLER(Smlal),
        HANDLER(SmlalS),

        // Load/Store
        HANDLER(Ldr),
        HANDLER(LdrB),
        HANDLER(LdrH),
        HANDLER(LdrSB),
        HANDLER(LdrSH),
        HANDLER(Str),
        HANDLER(StrB),
        HANDLER(StrH),

        // Load/Store Multiple
        HANDLER(Ldm),
        HANDLER(Stm),

        // Branch
        HANDLER(B),
        HANDLER(Bl),
        HANDLER(Bx),
        HANDLER(Blx),

        // Miscellaneous
        HANDLER(Clz),
        HANDLER(Swi),
        HANDLER(Mrs),
        HANDLER(Msr),

        // Extension
        HANDLER(Sxtb),
        HANDLER(Sxth),
        HANDLER(Uxtb),
        HANDLER(Uxth),

        // Byte reversal
        HANDLER(Rev),
        HANDLER(Rev16),
        HANDLER(Revsh),

        // Saturating arithmetic
        HANDLER(Qadd),
        HANDLER(Qsub),
        HANDLER(Qdadd),
        HANDLER(Qdsub),
        HANDLER(Ssat),
        HANDLER(Usat),

        // Parallel add/sub
        HANDLER(Sadd16),
        HANDLER(Ssub16),
        HANDLER(Sadd8),
        HANDLER(Ssub8),
        HANDLER(Uadd16),
        HANDLER(Usub16),
        HANDLER(Uadd8),
        HANDLER(Usub8),

        // Unsigned saturating parallel
        HANDLER(Uqadd16),
        HANDLER(Uqsub16),
        HANDLER(Uqadd8),
        HANDLER(Uqsub8),

        // Signed saturating parallel
        HANDLER(Qadd16),
        HANDLER(Qsub16),
        HANDLER(Qadd8),
        HANDLER(Qsub8),

        // Signed halving parallel
        HANDLER(Shadd16),
        HANDLER(Shsub16),
        HANDLER(Shadd8),
        HANDLER(Shsub8),

        // Unsigned halving parallel
        HANDLER(Uhadd16),
        HANDLER(Uhsub16),
        HANDLER(Uhadd8),
        HANDLER(Uhsub8),

        // Extend with Add
        HANDLER(Sxtab),
        HANDLER(Sxtab16),
        HANDLER(Sxtah),
        HANDLER(Uxtab),
        HANDLER(Uxtab16),
        HANDLER(Uxtah),

        // Bit field
        HANDLER(Bfc),
        HANDLER(Bfi),
        HANDLER(Sbfx),
        HANDLER(Ubfx),

        // Bit reversal
        HANDLER(Rbit),

        // Move top halfword
        HANDLER(Movt),

        // SIMD/Packed
        HANDLER(Pkhbt),
        HANDLER(Pkhtb),
        HANDLER(Sel),
        HANDLER(Usad8),
        HANDLER(Usada8),

        // VFP Register Transfer
        HANDLER(VmovArmToS),
        HANDLER(VmovSToArm),
        HANDLER(VmovArmToD),
        HANDLER(VmovDToArm),
        HANDLER(Vmrs),
        HANDLER(Vmsr),

        // VFP Data Processing (single precision)
        HANDLER(VaddS),
        HANDLER(VsubS),
        HANDLER(VmulS),
        HANDLER(VdivS),
        HANDLER(VmlaS),
        HANDLER(VmlsS),
        HANDLER(VnmlaS),
        HANDLER(VnmlsS),
        HANDLER(VnmulS),
        HANDLER(VabsS),
        HANDLER(VnegS),
        HANDLER(VsqrtS),
        HANDLER(VmovS),
        HANDLER(VmovImmS),

        // VFP Data Processing (double precision)
        HANDLER(VaddD),
        HANDLER(VsubD),
        HANDLER(VmulD),
        HANDLER(VdivD),
        HANDLER(VmlaD),
        HANDLER(VmlsD),
        HANDLER(VnmlaD),
        HANDLER(VnmlsD),
        HANDLER(VnmulD),
        HANDLER(VabsD),
        HANDLER(VnegD),
        HANDLER(VsqrtD),
        HANDLER(VmovD),
        HANDLER(VmovImmD),

        // VFP Comparison
        HANDLER(VcmpS),
        HANDLER(VcmpD),
        HANDLER(VcmpzS),
        HANDLER(VcmpzD),

        // VFP Conversion
        HANDLER(VcvtSToInt),
        HANDLER(VcvtIntToS),
        HANDLER(VcvtDToS),
        HANDLER(VcvtSToD),
        HANDLER(VcvtDToInt),
        HANDLER(VcvtIntToD),

        // VFP Load/Store
        HANDLER(VldrS),
        HANDLER(VstrS),
        HANDLER(VldrD),
        HANDLER(VstrD),

        // VFP Load/Store Multiple
        HANDLER(VldmS),
        HANDLER(VstmS),
        HANDLER(VldmD),
        HANDLER(VstmD),

        // Native Thumb16 handlers
        HANDLER(ThumbLslImm),
        HANDLER(ThumbLsrImm),
        HANDLER(ThumbAsrImm),
        HANDLER(ThumbAddReg3),
        HANDLER(ThumbSubReg3),
        HANDLER(ThumbAddImm3),
        HANDLER(ThumbSubImm3),
        HANDLER(ThumbMovImm8),
        HANDLER(ThumbCmpImm8),
        HANDLER(ThumbAddImm8),
        HANDLER(ThumbSubImm8),
        HANDLER(ThumbAnd),
        HANDLER(ThumbEor),
        HANDLER(ThumbLslReg),
        HANDLER(ThumbLsrReg),
        HANDLER(ThumbAsrReg),
        HANDLER(ThumbAdc),
        HANDLER(ThumbSbc),
        HANDLER(ThumbRor),
        HANDLER(ThumbTst),
        HANDLER(ThumbNeg),
        HANDLER(ThumbCmpReg),
        HANDLER(ThumbCmn),
        HANDLER(ThumbOrr),
        HANDLER(ThumbMul),
        HANDLER(ThumbBic),
        HANDLER(ThumbMvn),
        HANDLER(ThumbAddHi),
        HANDLER(ThumbCmpHi),
        HANDLER(ThumbMovHi),
        HANDLER(ThumbBx),
        HANDLER(ThumbBlxReg),
        HANDLER(ThumbLdrReg),
        HANDLER(ThumbStrReg),
        HANDLER(ThumbLdrImm5),
        HANDLER(ThumbStrImm5),
        HANDLER(ThumbLdrbReg),
        HANDLER(ThumbStrbReg),
        HANDLER(ThumbLdrbImm5),
        HANDLER(ThumbStrbImm5),
        HANDLER(ThumbLdrhReg),
        HANDLER(ThumbStrhReg),
        HANDLER(ThumbLdrhImm5),
        HANDLER(ThumbStrhImm5),
        HANDLER(ThumbLdrsb),
        HANDLER(ThumbLdrsh),
        HANDLER(ThumbLdrSp),
        HANDLER(ThumbStrSp),
        HANDLER(ThumbLdrPc),
        HANDLER(ThumbAddPcImm),
        HANDLER(ThumbAddSpImm),
        HANDLER(ThumbAddSpImm7),
        HANDLER(ThumbSubSpImm7),
        HANDLER(ThumbPush),
        HANDLER(ThumbPop),
        HANDLER(ThumbLdmia),
        HANDLER(ThumbStmia),
        HANDLER(ThumbBCond),
        HANDLER(ThumbB),
        HANDLER(ThumbSwi),
        HANDLER(ThumbSxth),
        HANDLER(ThumbSxtb),
        HANDLER(ThumbUxth),
        HANDLER(ThumbUxtb),
        HANDLER(ThumbRev),
        HANDLER(ThumbRev16),
        HANDLER(ThumbRevsh),

        // Thumb32 handlers
        HANDLER(ThumbBl),
        HANDLER(ThumbBlxImm),

        // Fused instruction handlers
        HANDLER(FusedSubsBcc),
        HANDLER(FusedSubsRegBcc),
        HANDLER(FusedCmpBcc),
        HANDLER(FusedCmpRegBcc),
        HANDLER(FusedTstBcc),
        HANDLER(FusedTstRegBcc),
        HANDLER(FusedTeqBcc),
        HANDLER(FusedTeqRegBcc),
        HANDLER(ThumbFusedSubsBcc),
        HANDLER(ThumbFusedCmpBcc),
        HANDLER(ThumbFusedCmpRegBcc),
        HANDLER(ThumbFusedTstBcc),
        HANDLER(FusedAddCmpBcc),
        // Unrolled loop handlers (hot block optimization)
        HANDLER(ThumbUnrolledSubsLoop),
        HANDLER(ArmUnrolledSubsLoop),
        HANDLER(ThumbUnrolledCmpLoop),
        // Coprocessor (CP15)
        HANDLER(Mrc),
        HANDLER(Mcr),
        // Exclusive load/store
        HANDLER(Ldrex),
        HANDLER(Strex),
        HANDLER(Ldrexb),
        HANDLER(Strexb),
        HANDLER(Ldrexh),
        HANDLER(Strexh),
        HANDLER(Ldrexd),
        HANDLER(Strexd),
        HANDLER(Clrex),
        // Doubleword load/store
        HANDLER(Ldrd),
        HANDLER(Strd),
        // Halfword multiplies
        HANDLER(Smulxy),
        HANDLER(Smlaxy),
        HANDLER(Smulwy),
        HANDLER(Smlawy),
        HANDLER(Smlalxy),
        // Decode-time specialized opcodes
        HANDLER(LdrImm),
        HANDLER(StrImm),
        HANDLER(LdrBImm),
        HANDLER(StrBImm),
        HANDLER(LdrHImm),
        HANDLER(StrHImm),
        HANDLER(BlxImm),
        // PC-source elimination
        HANDLER(LdrLit),
        HANDLER(PcSetup),
    };
#undef HANDLER

    static_assert(sizeof(dispatch_table) / sizeof(dispatch_table[0]) ==
                      static_cast<size_t>(Opcode::OpcodeCount),
                  "Dispatch table size mismatch");

    const DecodedInst* inst = block->instructions.data();
    const DecodedInst* end = inst + block->inst_count;
    u64 executed = 0;

    // Block chaining: limit chains to ensure timers/interrupts are checked
    constexpr u32 MAX_CHAINS = 16;
    u32 chain_count = 0;

    // PC lives in this local (register-allocated); state_.regs[15] is not kept
    // current inside a block. That's safe because decode guarantees no handler
    // reads regs[15] as a source operand (such reads are folded to constants at
    // decode time, or preceded by a PcSetup that writes regs[15] first).
    // local_pc syncs back to regs[15] at chain/done (SYNC_PC) and before SVC.
    u32 local_pc = state_.regs[15];

// Macros for PC access through local variable (synced at chain/done)
#define GET_PC() local_pc
#define SET_PC(val) (local_pc = (val))
#define SYNC_PC() (state_.regs[15] = local_pc)

// No bounds check: every block ends with an EndBlock terminator.
#define DISPATCH()                                                                                 \
    do {                                                                                           \
        ++inst;                                                                                    \
        goto* dispatch_table[static_cast<size_t>(inst->opcode)];                                   \
    } while (0)

#define CHECK_COND()                                                                               \
    do {                                                                                           \
        if (inst->cond != 0xE && !CheckCondition(inst->cond)) {                                    \
            executed += inst->ticks;                                                               \
            SET_PC(GET_PC() + inst->inst_size);                                                    \
            DISPATCH();                                                                            \
        }                                                                                          \
    } while (0)

#define ADVANCE_AND_DISPATCH()                                                                     \
    do {                                                                                           \
        executed += inst->ticks;                                                                   \
        SET_PC(GET_PC() + inst->inst_size);                                                        \
        DISPATCH();                                                                                \
    } while (0)

    // Helper to compute shifted operand 2 (with carry computation for flag-setting ops)
    auto GetOp2 = [this, &inst](bool& carry) -> u32 {
        if (inst->dp.rm == 0xFF) {
            // Immediate: flags bit 0 = carry-out is imm bit 31,
            // bit 1 = complemented (ORN stores the inverted immediate)
            if (inst->dp.flags & 1) {
                carry = ((inst->dp.imm >> 31) ^ (inst->dp.flags >> 1)) & 1;
            }
            return inst->dp.imm;
        }
        u32 rm_val = state_.regs[inst->dp.rm];
        if (inst->dp.rs != 0xFF) {
            // Register-specified shift amount
            u8 shift_amt = state_.regs[inst->dp.rs] & 0xFF;
            return ShiftReg(rm_val, inst->dp.shift_type, shift_amt, carry);
        } else {
            // Immediate shift amount
            return ShiftImm(rm_val, inst->dp.shift_type, inst->dp.shift_imm, carry);
        }
    };

    // Fast helper for non-flag-setting operations (skips carry computation)
    auto GetOp2Fast = [this, &inst]() -> u32 {
        if (inst->dp.rm == 0xFF) {
            return inst->dp.imm;
        }
        u32 rm_val = state_.regs[inst->dp.rm];
        // Fast path: no shift (LSL #0)
        if (inst->dp.rs == 0xFF && inst->dp.shift_imm == 0 &&
            inst->dp.shift_type == ShiftType::LSL) {
            return rm_val;
        }
        if (inst->dp.rs != 0xFF) {
            u8 shift_amt = state_.regs[inst->dp.rs] & 0xFF;
            if (shift_amt == 0)
                return rm_val;
            return ShiftNoCarry(rm_val, inst->dp.shift_type, shift_amt);
        } else {
            return ShiftNoCarry(rm_val, inst->dp.shift_type, inst->dp.shift_imm);
        }
    };

    // ========================================================================
    // Memory Access Macros
    // ========================================================================
    // Macros rather than lambdas: capturing 'this' costs a closure in tight
    // loops like LDM/STM.

    u8** const local_page_ptrs = state_.page_pointers;

// Fast-path read macros with direct page table access
#define MEM_READ8(addr, result)                                                                    \
    do {                                                                                           \
        const u32 _addr = (addr);                                                                  \
        const u8* _page = local_page_ptrs[_addr >> FASTINTERP_PAGE_BITS];                          \
        if (__builtin_expect(_page != nullptr, 1)) {                                               \
            (result) = _page[_addr & FASTINTERP_PAGE_MASK];                                        \
        } else {                                                                                   \
            (result) = memory_.Read8(_addr);                                                       \
        }                                                                                          \
    } while (0)

#define MEM_READ16(addr, result)                                                                   \
    do {                                                                                           \
        const u32 _addr = (addr);                                                                  \
        const u8* _page = local_page_ptrs[_addr >> FASTINTERP_PAGE_BITS];                          \
        if (__builtin_expect(_page != nullptr, 1)) {                                               \
            std::memcpy(&(result), &_page[_addr & FASTINTERP_PAGE_MASK], 2);                       \
        } else {                                                                                   \
            (result) = memory_.Read16(_addr);                                                      \
        }                                                                                          \
    } while (0)

#define MEM_READ32(addr, result)                                                                   \
    do {                                                                                           \
        const u32 _addr = (addr);                                                                  \
        const u8* _page = local_page_ptrs[_addr >> FASTINTERP_PAGE_BITS];                          \
        if (__builtin_expect(_page != nullptr, 1)) {                                               \
            std::memcpy(&(result), &_page[_addr & FASTINTERP_PAGE_MASK], 4);                       \
        } else {                                                                                   \
            (result) = memory_.Read32(_addr);                                                      \
        }                                                                                          \
    } while (0)

// Fast-path write macros with direct page table access
#define MEM_WRITE8(addr, value)                                                                    \
    do {                                                                                           \
        const u32 _addr = (addr);                                                                  \
        const u8 _val = (value);                                                                   \
        u8* _page = local_page_ptrs[_addr >> FASTINTERP_PAGE_BITS];                                \
        if (__builtin_expect(_page != nullptr, 1)) {                                               \
            _page[_addr & FASTINTERP_PAGE_MASK] = _val;                                            \
        } else {                                                                                   \
            memory_.Write8(_addr, _val);                                                           \
        }                                                                                          \
    } while (0)

#define MEM_WRITE16(addr, value)                                                                   \
    do {                                                                                           \
        const u32 _addr = (addr);                                                                  \
        const u16 _val = (value);                                                                  \
        u8* _page = local_page_ptrs[_addr >> FASTINTERP_PAGE_BITS];                                \
        if (__builtin_expect(_page != nullptr, 1)) {                                               \
            std::memcpy(&_page[_addr & FASTINTERP_PAGE_MASK], &_val, 2);                           \
        } else {                                                                                   \
            memory_.Write16(_addr, _val);                                                          \
        }                                                                                          \
    } while (0)

#define MEM_WRITE32(addr, value)                                                                   \
    do {                                                                                           \
        const u32 _addr = (addr);                                                                  \
        const u32 _val = (value);                                                                  \
        u8* _page = local_page_ptrs[_addr >> FASTINTERP_PAGE_BITS];                                \
        if (__builtin_expect(_page != nullptr, 1)) {                                               \
            std::memcpy(&_page[_addr & FASTINTERP_PAGE_MASK], &_val, 4);                           \
        } else {                                                                                   \
            memory_.Write32(_addr, _val);                                                          \
        }                                                                                          \
    } while (0)

    // Expression-form counterparts of the statement-form MEM_* macros, used
    // inline by the load/store, VFP, and Thumb handlers.
    auto MemRead8 = [this, local_page_ptrs](u32 addr) -> u8 {
        const u8* page = local_page_ptrs[addr >> FASTINTERP_PAGE_BITS];
        if (__builtin_expect(page != nullptr, 1)) {
            return page[addr & FASTINTERP_PAGE_MASK];
        }
        return memory_.Read8(addr);
    };

    auto MemRead16 = [this, local_page_ptrs](u32 addr) -> u16 {
        const u8* page = local_page_ptrs[addr >> FASTINTERP_PAGE_BITS];
        if (__builtin_expect(page != nullptr, 1)) {
            u16 value;
            std::memcpy(&value, &page[addr & FASTINTERP_PAGE_MASK], 2);
            return value;
        }
        return memory_.Read16(addr);
    };

    auto MemRead32 = [this, local_page_ptrs](u32 addr) -> u32 {
        const u8* page = local_page_ptrs[addr >> FASTINTERP_PAGE_BITS];
        if (__builtin_expect(page != nullptr, 1)) {
            u32 value;
            std::memcpy(&value, &page[addr & FASTINTERP_PAGE_MASK], 4);
            return value;
        }
        return memory_.Read32(addr);
    };

    auto MemWrite8 = [this, local_page_ptrs](u32 addr, u8 value) {
        u8* page = local_page_ptrs[addr >> FASTINTERP_PAGE_BITS];
        if (__builtin_expect(page != nullptr, 1)) {
            page[addr & FASTINTERP_PAGE_MASK] = value;
            return;
        }
        memory_.Write8(addr, value);
    };

    auto MemWrite16 = [this, local_page_ptrs](u32 addr, u16 value) {
        u8* page = local_page_ptrs[addr >> FASTINTERP_PAGE_BITS];
        if (__builtin_expect(page != nullptr, 1)) {
            std::memcpy(&page[addr & FASTINTERP_PAGE_MASK], &value, 2);
            return;
        }
        memory_.Write16(addr, value);
    };

    auto MemWrite32 = [this, local_page_ptrs](u32 addr, u32 value) {
        u8* page = local_page_ptrs[addr >> FASTINTERP_PAGE_BITS];
        if (__builtin_expect(page != nullptr, 1)) {
            std::memcpy(&page[addr & FASTINTERP_PAGE_MASK], &value, 4);
            return;
        }
        memory_.Write32(addr, value);
    };

    // Entry guard: poisoned blocks (inst_count = 0 from InvalidateRange/Clear)
    // must not execute their stale instructions.
    if (inst >= end)
        goto done;
    goto* dispatch_table[static_cast<size_t>(inst->opcode)];

    // ========================================================================
    // Instruction Handlers
    // ========================================================================
    // Each handlers/*.inc is #included here and becomes part of ExecuteBlock().
    // The handler environment: `inst`, `executed`, GET_PC()/SET_PC(), the MEM_*
    // macros and MemRead*/MemWrite* lambdas, GetOp2/GetOp2Fast, and the `chain`/
    // `done` exit labels. Every handler adds its ticks to `executed` and leaves
    // via a DISPATCH macro or `goto chain`/`goto done`; no handler may read
    // regs[15] as a source (see PC-source elimination, fastinterp_decode.cpp).

    // clang-format off: arm_alu.inc defines ALU_DISPATCH for arm_alu_s.inc,
    // so these are not sortable
#include "handlers/special.inc"
#include "handlers/arm_alu.inc"
#include "handlers/arm_alu_s.inc"
#include "handlers/arm_multiply.inc"
#include "handlers/arm_loadstore.inc"
#include "handlers/arm_branch.inc"
#include "handlers/arm_misc.inc"
#include "handlers/arm_vfp.inc"
#include "handlers/thumb16.inc"
#include "handlers/thumb32.inc"
#include "handlers/fused.inc"
    // clang-format on

chain:
    SYNC_PC();

    // Try to chain to the next block instead of returning to Run()
    if (chain_count < MAX_CHAINS) {
        u32 next_pc = local_pc;

        // Self-loop fast path: branch back to the same block (tight loops)
        if (next_pc == block->start_pc) {
            chain_count++;
            block->exec_count++;
            inst = block->instructions.data();
            end = inst + block->inst_count;
            if (inst < end) {
                goto* dispatch_table[static_cast<size_t>(inst->opcode)];
            }
        }

        // Fast path: use cached chain target if it matches
        BasicBlock* next = block->chain_target;
        if (next && next->start_pc == next_pc && next->IsThumb() == state_.thumb_mode) {
            chain_count++;
            block->exec_count++;
            block = next;
            inst = block->instructions.data();
            end = inst + block->inst_count;
            if (inst < end) {
                goto* dispatch_table[static_cast<size_t>(inst->opcode)];
            }
        }

        // Slow path: look up next block
        // Only cache if we don't already have a chain_target (don't thrash on alternating branches)
        next = cache_.Lookup(next_pc);
        if (next && next->IsThumb() == state_.thumb_mode) {
            if (!block->chain_target) {
                block->chain_target = next; // Cache for next time
            }
            chain_count++;
            block->exec_count++;
            block = next;
            inst = block->instructions.data();
            end = inst + block->inst_count;
            if (inst < end) {
                goto* dispatch_table[static_cast<size_t>(inst->opcode)];
            }
        }
    }

done:
    SYNC_PC();
    block->exec_count++;
    // Track and reoptimize hot block when threshold is first reached. The chain
    // paths increment exec_count without this check, so a self-chaining block
    // strides past the exact threshold - compare with >= (IsHot() prevents
    // repeat triggers).
    if (block->exec_count >= HOT_BLOCK_THRESHOLD && !block->IsHot()) {
        block->SetHot();
        fusion_stats_.hot_blocks_detected++;
        if (!disable_reoptimization_) {
            ReoptimizeHotBlock(block);
        }
    }

#undef GET_PC
#undef SET_PC
#undef SYNC_PC
#undef DISPATCH
#undef CHECK_COND
#undef ADVANCE_AND_DISPATCH
#undef MEM_READ8
#undef MEM_READ16
#undef MEM_READ32
#undef MEM_WRITE8
#undef MEM_WRITE16
#undef MEM_WRITE32

    return executed;
}

// ============================================================================
// Condition Check
// ============================================================================

bool ARM_FastInterp::CheckCondition(u8 cond) {
    state_.lazy_flags.Materialize();
    return CondPassed(cond, state_.lazy_flags.nzcv);
}

// ============================================================================
// Barrel Shifter
// ============================================================================

// For immediate shifts, amount==0 has special meanings:
// LSL #0 = no shift (carry unchanged), LSR/ASR #0 = #32, ROR #0 = RRX
u32 ARM_FastInterp::ShiftImm(u32 value, ShiftType type, u8 amount, bool& carry_out) {
    switch (type) {
    case ShiftType::LSL:
        if (amount == 0) {
            carry_out = state_.lazy_flags.GetC();
            return value;
        }
        if (amount >= 32) {
            carry_out = (amount == 32) ? (value & 1) : false;
            return 0;
        }
        carry_out = (value >> (32 - amount)) & 1;
        return value << amount;

    case ShiftType::LSR:
        // LSR #0 encodes LSR #32
        if (amount == 0) {
            amount = 32;
        }
        if (amount >= 32) {
            carry_out = (amount == 32) ? ((value >> 31) & 1) : false;
            return 0;
        }
        carry_out = (value >> (amount - 1)) & 1;
        return value >> amount;

    case ShiftType::ASR:
        // ASR #0 encodes ASR #32
        if (amount == 0) {
            amount = 32;
        }
        if (amount >= 32) {
            carry_out = (value >> 31) & 1;
            return static_cast<u32>(static_cast<s32>(value) >> 31);
        }
        carry_out = (value >> (amount - 1)) & 1;
        return static_cast<u32>(static_cast<s32>(value) >> amount);

    case ShiftType::ROR:
        if (amount == 0) {
            // RRX: rotate right extended (through carry)
            carry_out = value & 1;
            return (state_.lazy_flags.GetC() ? 0x80000000 : 0) | (value >> 1);
        }
        amount &= 31;
        if (amount == 0) {
            carry_out = (value >> 31) & 1;
            return value;
        }
        carry_out = (value >> (amount - 1)) & 1;
        return (value >> amount) | (value << (32 - amount));

    default:
        carry_out = false;
        return value;
    }
}

u32 ARM_FastInterp::ShiftReg(u32 value, ShiftType type, u8 amount, bool& carry_out) {
    // Register-specified shift amount (bottom byte only; amount is already u8)
    if (amount == 0) {
        carry_out = state_.lazy_flags.GetC();
        return value;
    }
    return ShiftImm(value, type, amount, carry_out);
}

// Fast shift without carry computation (for non-flag-setting operations)
u32 ARM_FastInterp::ShiftNoCarry(u32 value, ShiftType type, u8 amount) {
    switch (type) {
    case ShiftType::LSL:
        if (amount == 0)
            return value;
        if (amount >= 32)
            return 0;
        return value << amount;

    case ShiftType::LSR:
        if (amount == 0)
            amount = 32; // LSR #0 encodes LSR #32
        if (amount >= 32)
            return 0;
        return value >> amount;

    case ShiftType::ASR:
        if (amount == 0)
            amount = 32; // ASR #0 encodes ASR #32
        if (amount >= 32)
            return static_cast<u32>(static_cast<s32>(value) >> 31);
        return static_cast<u32>(static_cast<s32>(value) >> amount);

    case ShiftType::ROR:
        if (amount == 0) {
            // RRX - need carry for this, fall back to slow path
            bool carry;
            return ShiftImm(value, type, amount, carry);
        }
        amount &= 31;
        if (amount == 0)
            return value;
        return (value >> amount) | (value << (32 - amount));

    default:
        return value;
    }
}

// ============================================================================
// Memory Access
// ============================================================================
// Plain forwards to the memory system, for decode-time access (ExecuteBlock
// uses the MEM_* fast paths instead).

u16 ARM_FastInterp::ReadMemory16(u32 addr) {
    return memory_.Read16(addr);
}

u32 ARM_FastInterp::ReadMemory32(u32 addr) {
    return memory_.Read32(addr);
}

// ============================================================================
// Cache, Page Table & Register Accessors
// ============================================================================

void ARM_FastInterp::ClearInstructionCache() {
    cache_.Clear();
}

void ARM_FastInterp::InvalidateCacheRange(u32 start_address, std::size_t length) {
    cache_.InvalidateRange(start_address, static_cast<u32>(length));
}

void ARM_FastInterp::ClearExclusiveState() {
    exclusive_.valid = false;
}

void ARM_FastInterp::SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) {
    // The kernel calls this on every context switch, almost always with the
    // same page table. Only reset when the address space actually changes;
    // flushing the block cache every switch would re-decode all hot code.
    if (page_table_ == page_table) {
        return;
    }
    page_table_ = page_table;
    if (page_table) {
        state_.page_pointers = page_table->GetPointerArray().data();
    } else {
        state_.page_pointers = empty_page_table; // Never null
    }
    ClearInstructionCache();
}

std::shared_ptr<Memory::PageTable> ARM_FastInterp::GetPageTable() const {
    return page_table_;
}

void ARM_FastInterp::SetPC(u32 addr) {
    state_.thumb_mode = (addr & 1) != 0;
    state_.SetPC(addr & ~1u);
}

u32 ARM_FastInterp::GetPC() const {
    return state_.PC();
}

u32 ARM_FastInterp::GetReg(int index) const {
    return state_.regs[index];
}

void ARM_FastInterp::SetReg(int index, u32 value) {
    state_.regs[index] = value;
}

u32 ARM_FastInterp::GetVFPReg(int index) const {
    return state_.vfp_regs[index];
}

void ARM_FastInterp::SetVFPReg(int index, u32 value) {
    state_.vfp_regs[index] = value;
}

u32 ARM_FastInterp::GetVFPSystemReg(VFPSystemRegister reg) const {
    switch (reg) {
    case VFP_FPSCR:
        return state_.fpscr;
    case VFP_FPEXC:
        return state_.fpexc;
    default:
        return 0;
    }
}

void ARM_FastInterp::SetVFPSystemReg(VFPSystemRegister reg, u32 value) {
    switch (reg) {
    case VFP_FPSCR:
        state_.fpscr = value;
        break;
    case VFP_FPEXC:
        state_.fpexc = value;
        break;
    default:
        break;
    }
}

u32 ARM_FastInterp::GetCPSR() const {
    // Need to materialize lazy flags for const access
    LazyFlags& flags = const_cast<LazyFlags&>(state_.lazy_flags);
    u32 cpsr = flags.GetPacked() | (state_.cpsr_other & 0x0FFFFFFF);
    // Merge T from thumb_mode, as in InterpreterState::GetCPSR
    cpsr = (cpsr & ~(1u << 5)) | (static_cast<u32>(state_.thumb_mode) << 5);
    return cpsr;
}

void ARM_FastInterp::SetCPSR(u32 cpsr) {
    state_.SetCPSR(cpsr);
    state_.thumb_mode = (cpsr >> 5) & 1;
}

u32 ARM_FastInterp::GetCP15Register(CP15Register reg) const {
    switch (reg) {
    case CP15_THREAD_UPRW:
        return state_.cp15_thread_uprw;
    case CP15_THREAD_URO:
        return state_.cp15_thread_uro;
    default:
        return 0;
    }
}

void ARM_FastInterp::SetCP15Register(CP15Register reg, u32 value) {
    switch (reg) {
    case CP15_THREAD_UPRW:
        state_.cp15_thread_uprw = value;
        break;
    case CP15_THREAD_URO:
        state_.cp15_thread_uro = value;
        break;
    default:
        break;
    }
}

void ARM_FastInterp::SaveContext(ThreadContext& ctx) {
    for (int i = 0; i < 16; i++) {
        ctx.cpu_registers[i] = state_.regs[i];
    }
    ctx.cpsr = GetCPSR();
    for (int i = 0; i < 64; i++) {
        ctx.fpu_registers[i] = state_.vfp_regs[i];
    }
    ctx.fpscr = state_.fpscr;
    ctx.fpexc = state_.fpexc;
}

void ARM_FastInterp::LoadContext(const ThreadContext& ctx) {
    for (int i = 0; i < 16; i++) {
        state_.regs[i] = ctx.cpu_registers[i];
    }
    SetCPSR(ctx.cpsr);
    for (int i = 0; i < 64; i++) {
        state_.vfp_regs[i] = ctx.fpu_registers[i];
    }
    state_.fpscr = ctx.fpscr;
    state_.fpexc = ctx.fpexc;
}

void ARM_FastInterp::PrepareReschedule() {
    halted_ = true;
}

} // namespace Core::FastInterp
