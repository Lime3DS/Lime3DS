// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/logging/log.h"
#include "common/swap.h"
#include "core/arm/skyeye_common/armstate.h"
#include "core/arm/skyeye_common/vfp/vfp.h"
#include "core/core.h"
#include "core/memory.h"

ARMul_State::ARMul_State(Core::System& system_, Memory::MemorySystem& memory_,
                         PrivilegeMode initial_mode)
    : system{system_}, memory{memory_} {
    Reset();
    ChangePrivilegeMode(initial_mode);
}

void ARMul_State::ChangePrivilegeMode(u32 new_mode) {
    if (Mode == new_mode)
        return;

    if (new_mode != USERBANK) {
        switch (Mode) {
        case SYSTEM32MODE: // Shares registers with user mode
        case USER32MODE:
            Reg_usr[0] = Reg[13];
            Reg_usr[1] = Reg[14];
            break;
        case IRQ32MODE:
            Reg_irq[0] = Reg[13];
            Reg_irq[1] = Reg[14];
            Spsr[IRQBANK] = Spsr_copy;
            break;
        case SVC32MODE:
            Reg_svc[0] = Reg[13];
            Reg_svc[1] = Reg[14];
            Spsr[SVCBANK] = Spsr_copy;
            break;
        case ABORT32MODE:
            Reg_abort[0] = Reg[13];
            Reg_abort[1] = Reg[14];
            Spsr[ABORTBANK] = Spsr_copy;
            break;
        case UNDEF32MODE:
            Reg_undef[0] = Reg[13];
            Reg_undef[1] = Reg[14];
            Spsr[UNDEFBANK] = Spsr_copy;
            break;
        case FIQ32MODE:
            std::copy(Reg.begin() + 8, Reg.end() - 1, Reg_firq.begin());
            Spsr[FIQBANK] = Spsr_copy;
            break;
        }

        switch (new_mode) {
        case USER32MODE:
            Reg[13] = Reg_usr[0];
            Reg[14] = Reg_usr[1];
            Bank = USERBANK;
            break;
        case IRQ32MODE:
            Reg[13] = Reg_irq[0];
            Reg[14] = Reg_irq[1];
            Spsr_copy = Spsr[IRQBANK];
            Bank = IRQBANK;
            break;
        case SVC32MODE:
            Reg[13] = Reg_svc[0];
            Reg[14] = Reg_svc[1];
            Spsr_copy = Spsr[SVCBANK];
            Bank = SVCBANK;
            break;
        case ABORT32MODE:
            Reg[13] = Reg_abort[0];
            Reg[14] = Reg_abort[1];
            Spsr_copy = Spsr[ABORTBANK];
            Bank = ABORTBANK;
            break;
        case UNDEF32MODE:
            Reg[13] = Reg_undef[0];
            Reg[14] = Reg_undef[1];
            Spsr_copy = Spsr[UNDEFBANK];
            Bank = UNDEFBANK;
            break;
        case FIQ32MODE:
            std::copy(Reg_firq.begin(), Reg_firq.end(), Reg.begin() + 8);
            Spsr_copy = Spsr[FIQBANK];
            Bank = FIQBANK;
            break;
        case SYSTEM32MODE: // Shares registers with user mode.
            Reg[13] = Reg_usr[0];
            Reg[14] = Reg_usr[1];
            Bank = SYSTEMBANK;
            break;
        }

        // Set the mode bits in the APSR
        Cpsr = (Cpsr & ~Mode) | new_mode;
        Mode = new_mode;
    }
}

// Performs a reset
void ARMul_State::Reset() {
    VFPInit(this);

    // Set stack pointer to the top of the stack
    Reg[13] = 0x10000000;
    Reg[15] = 0;

    Cpsr = static_cast<u32>(INTBITS) | static_cast<u32>(SVC32MODE);
    Mode = SVC32MODE;
    Bank = SVCBANK;

    ResetMPCoreCP15Registers();

    NresetSig = HIGH;
    NfiqSig = HIGH;
    NirqSig = HIGH;
    NtransSig = (Mode & 3) ? HIGH : LOW;
    abortSig = LOW;

    NumInstrs = 0;
    Emulate = RUN;
}

// Resets certain MPCore CP15 values to their ARM-defined reset values.
void ARMul_State::ResetMPCoreCP15Registers() {
    // c0
    CP15[CP15_MAIN_ID] = 0x410FB024;
    CP15[CP15_TLB_TYPE] = 0x00000800;
    CP15[CP15_PROCESSOR_FEATURE_0] = 0x00000111;
    CP15[CP15_PROCESSOR_FEATURE_1] = 0x00000001;
    CP15[CP15_DEBUG_FEATURE_0] = 0x00000002;
    CP15[CP15_MEMORY_MODEL_FEATURE_0] = 0x01100103;
    CP15[CP15_MEMORY_MODEL_FEATURE_1] = 0x10020302;
    CP15[CP15_MEMORY_MODEL_FEATURE_2] = 0x01222000;
    CP15[CP15_MEMORY_MODEL_FEATURE_3] = 0x00000000;
    CP15[CP15_ISA_FEATURE_0] = 0x00100011;
    CP15[CP15_ISA_FEATURE_1] = 0x12002111;
    CP15[CP15_ISA_FEATURE_2] = 0x11221011;
    CP15[CP15_ISA_FEATURE_3] = 0x01102131;
    CP15[CP15_ISA_FEATURE_4] = 0x00000141;

    // c1
    CP15[CP15_CONTROL] = 0x00054078;
    CP15[CP15_AUXILIARY_CONTROL] = 0x0000000F;
    CP15[CP15_COPROCESSOR_ACCESS_CONTROL] = 0x00000000;

    // c2
    CP15[CP15_TRANSLATION_BASE_TABLE_0] = 0x00000000;
    CP15[CP15_TRANSLATION_BASE_TABLE_1] = 0x00000000;
    CP15[CP15_TRANSLATION_BASE_CONTROL] = 0x00000000;

    // c3
    CP15[CP15_DOMAIN_ACCESS_CONTROL] = 0x00000000;

    // c7
    CP15[CP15_PHYS_ADDRESS] = 0x00000000;

    // c9
    CP15[CP15_DATA_CACHE_LOCKDOWN] = 0xFFFFFFF0;

    // c10
    CP15[CP15_TLB_LOCKDOWN] = 0x00000000;
    CP15[CP15_PRIMARY_REGION_REMAP] = 0x00098AA4;
    CP15[CP15_NORMAL_REGION_REMAP] = 0x44E048E0;

    // c13
    CP15[CP15_PID] = 0x00000000;
    CP15[CP15_CONTEXT_ID] = 0x00000000;
    CP15[CP15_THREAD_UPRW] = 0x00000000;
    CP15[CP15_THREAD_URO] = 0x00000000;
    CP15[CP15_THREAD_PRW] = 0x00000000;

    // c15
    CP15[CP15_PERFORMANCE_MONITOR_CONTROL] = 0x00000000;
    CP15[CP15_MAIN_TLB_LOCKDOWN_VIRT_ADDRESS] = 0x00000000;
    CP15[CP15_MAIN_TLB_LOCKDOWN_PHYS_ADDRESS] = 0x00000000;
    CP15[CP15_MAIN_TLB_LOCKDOWN_ATTRIBUTE] = 0x00000000;
    CP15[CP15_TLB_DEBUG_CONTROL] = 0x00000000;
}
#ifdef ANDROID
static void CheckMemoryBreakpoint(u32 address, GDBStub::BreakpointType type) {}
#else
static void CheckMemoryBreakpoint(u32 address, GDBStub::BreakpointType type) {
    if (GDBStub::IsServerEnabled() && GDBStub::CheckBreakpoint(address, type)) {
        LOG_DEBUG(Debug, "Found memory breakpoint @ {:08x}", address);
        GDBStub::Break(true);
    }
}
#endif

u8 ARMul_State::ReadMemory8(u32 address) const {
    CheckMemoryBreakpoint(address, GDBStub::BreakpointType::Read);

    return memory.Read8(address);
}

u16 ARMul_State::ReadMemory16(u32 address) const {
    CheckMemoryBreakpoint(address, GDBStub::BreakpointType::Read);

    u16 data = memory.Read16(address);

    if (InBigEndianMode())
        data = Common::swap16(data);

    return data;
}

u32 ARMul_State::ReadMemory32(u32 address) const {
    CheckMemoryBreakpoint(address, GDBStub::BreakpointType::Read);

    u32 data = memory.Read32(address);

    if (InBigEndianMode())
        data = Common::swap32(data);

    return data;
}

u64 ARMul_State::ReadMemory64(u32 address) const {
    CheckMemoryBreakpoint(address, GDBStub::BreakpointType::Read);

    u64 data = memory.Read64(address);

    if (InBigEndianMode())
        data = Common::swap64(data);

    return data;
}

void ARMul_State::WriteMemory8(u32 address, u8 data) {
    CheckMemoryBreakpoint(address, GDBStub::BreakpointType::Write);

    memory.Write8(address, data);
}

void ARMul_State::WriteMemory16(u32 address, u16 data) {
    CheckMemoryBreakpoint(address, GDBStub::BreakpointType::Write);

    if (InBigEndianMode())
        data = Common::swap16(data);

    memory.Write16(address, data);
}

void ARMul_State::WriteMemory32(u32 address, u32 data) {
    CheckMemoryBreakpoint(address, GDBStub::BreakpointType::Write);

    if (InBigEndianMode())
        data = Common::swap32(data);

    memory.Write32(address, data);
}

void ARMul_State::WriteMemory64(u32 address, u64 data) {
    CheckMemoryBreakpoint(address, GDBStub::BreakpointType::Write);

    if (InBigEndianMode())
        data = Common::swap64(data);

    memory.Write64(address, data);
}

// Reads from the CP15 registers. Used with implementation of the MRC instruction.
// Note that since the 3DS does not have the hypervisor extensions, these registers
// are not implemented.
u32 ARMul_State::ReadCP15Register(u32 crn, u32 opcode_1, u32 crm, u32 opcode_2) const {
    // Unprivileged registers
    if (crn == 13 && opcode_1 == 0 && crm == 0) {
        if (opcode_2 == 2)
            return CP15[CP15_THREAD_UPRW];

        if (opcode_2 == 3)
            return CP15[CP15_THREAD_URO];
    }

    if (InAPrivilegedMode()) {
        if (crn == 0 && opcode_1 == 0) {
            if (crm == 0) {
                if (opcode_2 == 0)
                    return CP15[CP15_MAIN_ID];

                if (opcode_2 == 1)
                    return CP15[CP15_CACHE_TYPE];

                if (opcode_2 == 3)
                    return CP15[CP15_TLB_TYPE];

                if (opcode_2 == 5)
                    return CP15[CP15_CPU_ID];
            } else if (crm == 1) {
                if (opcode_2 == 0)
                    return CP15[CP15_PROCESSOR_FEATURE_0];

                if (opcode_2 == 1)
                    return CP15[CP15_PROCESSOR_FEATURE_1];

                if (opcode_2 == 2)
                    return CP15[CP15_DEBUG_FEATURE_0];

                if (opcode_2 == 4)
                    return CP15[CP15_MEMORY_MODEL_FEATURE_0];

                if (opcode_2 == 5)
                    return CP15[CP15_MEMORY_MODEL_FEATURE_1];

                if (opcode_2 == 6)
                    return CP15[CP15_MEMORY_MODEL_FEATURE_2];

                if (opcode_2 == 7)
                    return CP15[CP15_MEMORY_MODEL_FEATURE_3];
            } else if (crm == 2) {
                if (opcode_2 == 0)
                    return CP15[CP15_ISA_FEATURE_0];

                if (opcode_2 == 1)
                    return CP15[CP15_ISA_FEATURE_1];

                if (opcode_2 == 2)
                    return CP15[CP15_ISA_FEATURE_2];

                if (opcode_2 == 3)
                    return CP15[CP15_ISA_FEATURE_3];

                if (opcode_2 == 4)
                    return CP15[CP15_ISA_FEATURE_4];
            }
        }

        if (crn == 1 && opcode_1 == 0 && crm == 0) {
            if (opcode_2 == 0)
                return CP15[CP15_CONTROL];

            if (opcode_2 == 1)
                return CP15[CP15_AUXILIARY_CONTROL];

            if (opcode_2 == 2)
                return CP15[CP15_COPROCESSOR_ACCESS_CONTROL];
        }

        if (crn == 2 && opcode_1 == 0 && crm == 0) {
            if (opcode_2 == 0)
                return CP15[CP15_TRANSLATION_BASE_TABLE_0];

            if (opcode_2 == 1)
                return CP15[CP15_TRANSLATION_BASE_TABLE_1];

            if (opcode_2 == 2)
                return CP15[CP15_TRANSLATION_BASE_CONTROL];
        }

        if (crn == 3 && opcode_1 == 0 && crm == 0 && opcode_2 == 0)
            return CP15[CP15_DOMAIN_ACCESS_CONTROL];

        if (crn == 5 && opcode_1 == 0 && crm == 0) {
            if (opcode_2 == 0)
                return CP15[CP15_FAULT_STATUS];

            if (opcode_2 == 1)
                return CP15[CP15_INSTR_FAULT_STATUS];
        }

        if (crn == 6 && opcode_1 == 0 && crm == 0) {
            if (opcode_2 == 0)
                return CP15[CP15_FAULT_ADDRESS];

            if (opcode_2 == 1)
                return CP15[CP15_WFAR];
        }

        if (crn == 7 && opcode_1 == 0 && crm == 4 && opcode_2 == 0)
            return CP15[CP15_PHYS_ADDRESS];

        if (crn == 9 && opcode_1 == 0 && crm == 0 && opcode_2 == 0)
            return CP15[CP15_DATA_CACHE_LOCKDOWN];

        if (crn == 10 && opcode_1 == 0) {
            if (crm == 0 && opcode_2 == 0)
                return CP15[CP15_TLB_LOCKDOWN];

            if (crm == 2) {
                if (opcode_2 == 0)
                    return CP15[CP15_PRIMARY_REGION_REMAP];

                if (opcode_2 == 1)
                    return CP15[CP15_NORMAL_REGION_REMAP];
            }
        }

        if (crn == 13 && crm == 0) {
            if (opcode_2 == 0)
                return CP15[CP15_PID];

            if (opcode_2 == 1)
                return CP15[CP15_CONTEXT_ID];

            if (opcode_2 == 4)
                return CP15[CP15_THREAD_PRW];
        }

        if (crn == 15) {
            if (opcode_1 == 0 && crm == 12) {
                if (opcode_2 == 0)
                    return CP15[CP15_PERFORMANCE_MONITOR_CONTROL];

                if (opcode_2 == 1)
                    return CP15[CP15_CYCLE_COUNTER];

                if (opcode_2 == 2)
                    return CP15[CP15_COUNT_0];

                if (opcode_2 == 3)
                    return CP15[CP15_COUNT_1];
            }

            if (opcode_1 == 5 && opcode_2 == 2) {
                if (crm == 5)
                    return CP15[CP15_MAIN_TLB_LOCKDOWN_VIRT_ADDRESS];

                if (crm == 6)
                    return CP15[CP15_MAIN_TLB_LOCKDOWN_PHYS_ADDRESS];

                if (crm == 7)
                    return CP15[CP15_MAIN_TLB_LOCKDOWN_ATTRIBUTE];
            }

            if (opcode_1 == 7 && crm == 1 && opcode_2 == 0)
                return CP15[CP15_TLB_DEBUG_CONTROL];
        }
    }

    LOG_ERROR(Core_ARM11, "MRC CRn={}, CRm={}, OP1={} OP2={} is not implemented. Returning zero.",
              crn, crm, opcode_1, opcode_2);
    return 0;
}

// Write to the CP15 registers. Used with implementation of the MCR instruction.
// Note that since the 3DS does not have the hypervisor extensions, these registers
// are not implemented.
void ARMul_State::WriteCP15Register(u32 value, u32 crn, u32 opcode_1, u32 crm, u32 opcode_2) {
    if (InAPrivilegedMode()) {
        if (crn == 1 && opcode_1 == 0 && crm == 0) {
            if (opcode_2 == 0)
                CP15[CP15_CONTROL] = value;
            else if (opcode_2 == 1)
                CP15[CP15_AUXILIARY_CONTROL] = value;
            else if (opcode_2 == 2)
                CP15[CP15_COPROCESSOR_ACCESS_CONTROL] = value;
        } else if (crn == 2 && opcode_1 == 0 && crm == 0) {
            if (opcode_2 == 0)
                CP15[CP15_TRANSLATION_BASE_TABLE_0] = value;
            else if (opcode_2 == 1)
                CP15[CP15_TRANSLATION_BASE_TABLE_1] = value;
            else if (opcode_2 == 2)
                CP15[CP15_TRANSLATION_BASE_CONTROL] = value;
        } else if (crn == 3 && opcode_1 == 0 && crm == 0 && opcode_2 == 0) {
            CP15[CP15_DOMAIN_ACCESS_CONTROL] = value;
        } else if (crn == 5 && opcode_1 == 0 && crm == 0) {
            if (opcode_2 == 0)
                CP15[CP15_FAULT_STATUS] = value;
            else if (opcode_2 == 1)
                CP15[CP15_INSTR_FAULT_STATUS] = value;
        } else if (crn == 6 && opcode_1 == 0 && crm == 0) {
            if (opcode_2 == 0)
                CP15[CP15_FAULT_ADDRESS] = value;
            else if (opcode_2 == 1)
                CP15[CP15_WFAR] = value;
        } else if (crn == 7 && opcode_1 == 0) {
            if (crm == 0 && opcode_2 == 4) {
                CP15[CP15_WAIT_FOR_INTERRUPT] = value;
            } else if (crm == 4 && opcode_2 == 0) {
                LOG_ERROR(Core_ARM11, "Unimplemented virtual to physical address");
            } else if (crm == 5) {
                if (opcode_2 == 0)
                    CP15[CP15_INVALIDATE_INSTR_CACHE] = value;
                else if (opcode_2 == 1)
                    CP15[CP15_INVALIDATE_INSTR_CACHE_USING_MVA] = value;
                else if (opcode_2 == 2)
                    CP15[CP15_INVALIDATE_INSTR_CACHE_USING_INDEX] = value;
                else if (opcode_2 == 6)
                    CP15[CP15_FLUSH_BRANCH_TARGET_CACHE] = value;
                else if (opcode_2 == 7)
                    CP15[CP15_FLUSH_BRANCH_TARGET_CACHE_ENTRY] = value;
            } else if (crm == 6) {
                if (opcode_2 == 0)
                    CP15[CP15_INVALIDATE_DATA_CACHE] = value;
                else if (opcode_2 == 1)
                    CP15[CP15_INVALIDATE_DATA_CACHE_LINE_USING_MVA] = value;
                else if (opcode_2 == 2)
                    CP15[CP15_INVALIDATE_DATA_CACHE_LINE_USING_INDEX] = value;
            } else if (crm == 7 && opcode_2 == 0) {
                CP15[CP15_INVALIDATE_DATA_AND_INSTR_CACHE] = value;
            } else if (crm == 10) {
                if (opcode_2 == 0)
                    CP15[CP15_CLEAN_DATA_CACHE] = value;
                else if (opcode_2 == 1)
                    CP15[CP15_CLEAN_DATA_CACHE_LINE_USING_MVA] = value;
                else if (opcode_2 == 2)
                    CP15[CP15_CLEAN_DATA_CACHE_LINE_USING_INDEX] = value;
            } else if (crm == 14) {
                if (opcode_2 == 0)
                    CP15[CP15_CLEAN_AND_INVALIDATE_DATA_CACHE] = value;
                else if (opcode_2 == 1)
                    CP15[CP15_CLEAN_AND_INVALIDATE_DATA_CACHE_LINE_USING_MVA] = value;
                else if (opcode_2 == 2)
                    CP15[CP15_CLEAN_AND_INVALIDATE_DATA_CACHE_LINE_USING_INDEX] = value;
            }
        } else if (crn == 8 && opcode_1 == 0) {
            if (crm == 5) {
                if (opcode_2 == 0)
                    CP15[CP15_INVALIDATE_ITLB] = value;
                else if (opcode_2 == 1)
                    CP15[CP15_INVALIDATE_ITLB_SINGLE_ENTRY] = value;
                else if (opcode_2 == 2)
                    CP15[CP15_INVALIDATE_ITLB_ENTRY_ON_ASID_MATCH] = value;
                else if (opcode_2 == 3)
                    CP15[CP15_INVALIDATE_ITLB_ENTRY_ON_MVA] = value;
            } else if (crm == 6) {
                if (opcode_2 == 0)
                    CP15[CP15_INVALIDATE_DTLB] = value;
                else if (opcode_2 == 1)
                    CP15[CP15_INVALIDATE_DTLB_SINGLE_ENTRY] = value;
                else if (opcode_2 == 2)
                    CP15[CP15_INVALIDATE_DTLB_ENTRY_ON_ASID_MATCH] = value;
                else if (opcode_2 == 3)
                    CP15[CP15_INVALIDATE_DTLB_ENTRY_ON_MVA] = value;
            } else if (crm == 7) {
                if (opcode_2 == 0)
                    CP15[CP15_INVALIDATE_UTLB] = value;
                else if (opcode_2 == 1)
                    CP15[CP15_INVALIDATE_UTLB_SINGLE_ENTRY] = value;
                else if (opcode_2 == 2)
                    CP15[CP15_INVALIDATE_UTLB_ENTRY_ON_ASID_MATCH] = value;
                else if (opcode_2 == 3)
                    CP15[CP15_INVALIDATE_UTLB_ENTRY_ON_MVA] = value;
            }
        } else if (crn == 9 && opcode_1 == 0 && crm == 0 && opcode_2 == 0) {
            CP15[CP15_DATA_CACHE_LOCKDOWN] = value;
        } else if (crn == 10 && opcode_1 == 0) {
            if (crm == 0 && opcode_2 == 0) {
                CP15[CP15_TLB_LOCKDOWN] = value;
            } else if (crm == 2) {
                if (opcode_2 == 0)
                    CP15[CP15_PRIMARY_REGION_REMAP] = value;
                else if (opcode_2 == 1)
                    CP15[CP15_NORMAL_REGION_REMAP] = value;
            }
        } else if (crn == 13 && opcode_1 == 0 && crm == 0) {
            if (opcode_2 == 0)
                CP15[CP15_PID] = value;
            else if (opcode_2 == 1)
                CP15[CP15_CONTEXT_ID] = value;
            else if (opcode_2 == 3)
                CP15[CP15_THREAD_URO] = value;
            else if (opcode_2 == 4)
                CP15[CP15_THREAD_PRW] = value;
        } else if (crn == 15) {
            if (opcode_1 == 0 && crm == 12) {
                if (opcode_2 == 0)
                    CP15[CP15_PERFORMANCE_MONITOR_CONTROL] = value;
                else if (opcode_2 == 1)
                    CP15[CP15_CYCLE_COUNTER] = value;
                else if (opcode_2 == 2)
                    CP15[CP15_COUNT_0] = value;
                else if (opcode_2 == 3)
                    CP15[CP15_COUNT_1] = value;
            } else if (opcode_1 == 5) {
                if (crm == 4) {
                    if (opcode_2 == 2)
                        CP15[CP15_READ_MAIN_TLB_LOCKDOWN_ENTRY] = value;
                    else if (opcode_2 == 4)
                        CP15[CP15_WRITE_MAIN_TLB_LOCKDOWN_ENTRY] = value;
                } else if (crm == 5 && opcode_2 == 2) {
                    CP15[CP15_MAIN_TLB_LOCKDOWN_VIRT_ADDRESS] = value;
                } else if (crm == 6 && opcode_2 == 2) {
                    CP15[CP15_MAIN_TLB_LOCKDOWN_PHYS_ADDRESS] = value;
                } else if (crm == 7 && opcode_2 == 2) {
                    CP15[CP15_MAIN_TLB_LOCKDOWN_ATTRIBUTE] = value;
                }
            } else if (opcode_1 == 7 && crm == 1 && opcode_2 == 0) {
                CP15[CP15_TLB_DEBUG_CONTROL] = value;
            }
        }
    }

    // Unprivileged registers
    if (crn == 7 && opcode_1 == 0 && crm == 5 && opcode_2 == 4) {
        CP15[CP15_FLUSH_PREFETCH_BUFFER] = value;
    } else if (crn == 7 && opcode_1 == 0 && crm == 10) {
        if (opcode_2 == 4)
            CP15[CP15_DATA_SYNC_BARRIER] = value;
        else if (opcode_2 == 5)
            CP15[CP15_DATA_MEMORY_BARRIER] = value;
    } else if (crn == 13 && opcode_1 == 0 && crm == 0 && opcode_2 == 2) {
        CP15[CP15_THREAD_UPRW] = value;
    }
}

void ARMul_State::ServeBreak() {
    if (!GDBStub::IsServerEnabled()) {
        return;
    }

    if (last_bkpt_hit && last_bkpt.type == GDBStub::BreakpointType::Execute) {
        DEBUG_ASSERT(Reg[15] == last_bkpt.address);
    }

    Kernel::Thread* thread = system.Kernel().GetCurrentThreadManager().GetCurrentThread();
    system.GetRunningCore().SaveContext(thread->context);

    if (last_bkpt_hit || GDBStub::IsMemoryBreak() || GDBStub::GetCpuStepFlag()) {
        last_bkpt_hit = false;
        GDBStub::Break();
        GDBStub::SendTrap(thread, 5);
    }
}
