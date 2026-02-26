// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// VFP (Floating Point) handlers: Register transfers (VmovArmToS, etc.),
//     Single precision (VaddS, VsubS, etc.), Double precision (VaddD, etc.),
//     Comparison (VcmpS, VcmpD, etc.), Conversion (VcvtSToInt, etc.),
//     Load/Store (VldrS, VstrS, VldrD, VstrD)

// Helper macros for VFP register access (bit_cast/memcpy: no aliasing UB).
// #undef'd at the bottom of this file.
#define VFP_S_READ(idx) (std::bit_cast<float>(state_.vfp_regs[(idx)]))
#define VFP_S_WRITE(idx, val) \
    (state_.vfp_regs[(idx)] = std::bit_cast<u32>(static_cast<float>(val)))
#define VFP_S_READ_U32(idx) (state_.vfp_regs[(idx)])
#define VFP_S_WRITE_U32(idx, val) (state_.vfp_regs[(idx)] = (val))

// D-registers are stored as two consecutive u32 halves in vfp_regs
#define VFP_D_READ(idx) ({ \
    double vfp_d_read_tmp_; \
    std::memcpy(&vfp_d_read_tmp_, &state_.vfp_regs[(idx) * 2], sizeof(vfp_d_read_tmp_)); \
    vfp_d_read_tmp_; \
})
#define VFP_D_WRITE(idx, val) do { \
    double vfp_d_tmp_ = (val); \
    std::memcpy(&state_.vfp_regs[(idx) * 2], &vfp_d_tmp_, sizeof(vfp_d_tmp_)); \
} while(0)

// VFP Register Transfer: VMOV ARM to single VFP
handle_VmovArmToS:
    CHECK_COND();
    VFP_S_WRITE_U32(inst->vfp.sd, state_.regs[inst->vfp.rt]);
    ADVANCE_AND_DISPATCH();

handle_VmovSToArm:
    CHECK_COND();
    state_.regs[inst->vfp.rt] = VFP_S_READ_U32(inst->vfp.sd);
    ADVANCE_AND_DISPATCH();

handle_VmovArmToD:
    CHECK_COND();
    state_.vfp_regs[inst->vfp.sd * 2] = state_.regs[inst->vfp.rt];
    state_.vfp_regs[inst->vfp.sd * 2 + 1] = state_.regs[inst->vfp.rt2];
    ADVANCE_AND_DISPATCH();

handle_VmovDToArm:
    CHECK_COND();
    state_.regs[inst->vfp.rt] = state_.vfp_regs[inst->vfp.sd * 2];
    state_.regs[inst->vfp.rt2] = state_.vfp_regs[inst->vfp.sd * 2 + 1];
    ADVANCE_AND_DISPATCH();

handle_Vmrs:
    CHECK_COND();
    if (inst->vfp.rt == 15) {
        // Move FPSCR flags to CPSR (APSR_nzcv); SetPacked masks to bits 31-28
        state_.lazy_flags.SetPacked(state_.fpscr);
    } else {
        state_.regs[inst->vfp.rt] = state_.fpscr;
    }
    ADVANCE_AND_DISPATCH();

handle_Vmsr:
    CHECK_COND();
    state_.fpscr = state_.regs[inst->vfp.rt];
    ADVANCE_AND_DISPATCH();

handle_VaddS:
    CHECK_COND();
    VFP_S_WRITE(inst->vfp.sd, VFP_S_READ(inst->vfp.sn) + VFP_S_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VsubS:
    CHECK_COND();
    VFP_S_WRITE(inst->vfp.sd, VFP_S_READ(inst->vfp.sn) - VFP_S_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VmulS:
    CHECK_COND();
    VFP_S_WRITE(inst->vfp.sd, VFP_S_READ(inst->vfp.sn) * VFP_S_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VdivS:
    CHECK_COND();
    VFP_S_WRITE(inst->vfp.sd, VFP_S_READ(inst->vfp.sn) / VFP_S_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VmlaS:
    CHECK_COND();
    VFP_S_WRITE(inst->vfp.sd, VFP_S_READ(inst->vfp.sd) + VFP_S_READ(inst->vfp.sn) * VFP_S_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VmlsS:
    CHECK_COND();
    VFP_S_WRITE(inst->vfp.sd, VFP_S_READ(inst->vfp.sd) - VFP_S_READ(inst->vfp.sn) * VFP_S_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VnmlaS:
    CHECK_COND();
    VFP_S_WRITE(inst->vfp.sd, -VFP_S_READ(inst->vfp.sd) - VFP_S_READ(inst->vfp.sn) * VFP_S_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VnmlsS:
    CHECK_COND();
    VFP_S_WRITE(inst->vfp.sd, -VFP_S_READ(inst->vfp.sd) + VFP_S_READ(inst->vfp.sn) * VFP_S_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VnmulS:
    CHECK_COND();
    VFP_S_WRITE(inst->vfp.sd, -(VFP_S_READ(inst->vfp.sn) * VFP_S_READ(inst->vfp.sm)));
    ADVANCE_AND_DISPATCH();

handle_VabsS:
    CHECK_COND();
    VFP_S_WRITE(inst->vfp.sd, std::fabs(VFP_S_READ(inst->vfp.sm)));
    ADVANCE_AND_DISPATCH();

handle_VnegS:
    CHECK_COND();
    VFP_S_WRITE(inst->vfp.sd, -VFP_S_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VsqrtS:
    CHECK_COND();
    VFP_S_WRITE(inst->vfp.sd, std::sqrt(VFP_S_READ(inst->vfp.sm)));
    ADVANCE_AND_DISPATCH();

handle_VmovS:
    CHECK_COND();
    VFP_S_WRITE_U32(inst->vfp.sd, VFP_S_READ_U32(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

// Expanded constant precomputed at decode into vimm.lo
handle_VmovImmS:
    CHECK_COND();
    VFP_S_WRITE_U32(inst->vimm.sd, inst->vimm.lo);
    ADVANCE_AND_DISPATCH();

handle_VaddD:
    CHECK_COND();
    VFP_D_WRITE(inst->vfp.sd, VFP_D_READ(inst->vfp.sn) + VFP_D_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VsubD:
    CHECK_COND();
    VFP_D_WRITE(inst->vfp.sd, VFP_D_READ(inst->vfp.sn) - VFP_D_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VmulD:
    CHECK_COND();
    VFP_D_WRITE(inst->vfp.sd, VFP_D_READ(inst->vfp.sn) * VFP_D_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VdivD:
    CHECK_COND();
    VFP_D_WRITE(inst->vfp.sd, VFP_D_READ(inst->vfp.sn) / VFP_D_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VmlaD:
    CHECK_COND();
    VFP_D_WRITE(inst->vfp.sd, VFP_D_READ(inst->vfp.sd) + VFP_D_READ(inst->vfp.sn) * VFP_D_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VmlsD:
    CHECK_COND();
    VFP_D_WRITE(inst->vfp.sd, VFP_D_READ(inst->vfp.sd) - VFP_D_READ(inst->vfp.sn) * VFP_D_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VnmlaD:
    CHECK_COND();
    VFP_D_WRITE(inst->vfp.sd, -VFP_D_READ(inst->vfp.sd) - VFP_D_READ(inst->vfp.sn) * VFP_D_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VnmlsD:
    CHECK_COND();
    VFP_D_WRITE(inst->vfp.sd, -VFP_D_READ(inst->vfp.sd) + VFP_D_READ(inst->vfp.sn) * VFP_D_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VnmulD:
    CHECK_COND();
    VFP_D_WRITE(inst->vfp.sd, -(VFP_D_READ(inst->vfp.sn) * VFP_D_READ(inst->vfp.sm)));
    ADVANCE_AND_DISPATCH();

handle_VabsD:
    CHECK_COND();
    VFP_D_WRITE(inst->vfp.sd, std::fabs(VFP_D_READ(inst->vfp.sm)));
    ADVANCE_AND_DISPATCH();

handle_VnegD:
    CHECK_COND();
    VFP_D_WRITE(inst->vfp.sd, -VFP_D_READ(inst->vfp.sm));
    ADVANCE_AND_DISPATCH();

handle_VsqrtD:
    CHECK_COND();
    VFP_D_WRITE(inst->vfp.sd, std::sqrt(VFP_D_READ(inst->vfp.sm)));
    ADVANCE_AND_DISPATCH();

handle_VmovD:
    CHECK_COND();
    state_.vfp_regs[inst->vfp.sd * 2] = state_.vfp_regs[inst->vfp.sm * 2];
    state_.vfp_regs[inst->vfp.sd * 2 + 1] = state_.vfp_regs[inst->vfp.sm * 2 + 1];
    ADVANCE_AND_DISPATCH();

// Expanded constant precomputed at decode into vimm.lo/hi
handle_VmovImmD:
    CHECK_COND();
    state_.vfp_regs[inst->vimm.sd * 2] = inst->vimm.lo;
    state_.vfp_regs[inst->vimm.sd * 2 + 1] = inst->vimm.hi;
    ADVANCE_AND_DISPATCH();

handle_VcmpS:
    CHECK_COND();
    {
        float a = VFP_S_READ(inst->vfp.sd);
        float b = VFP_S_READ(inst->vfp.sm);
        u32 flags = 0;
        if (std::isnan(a) || std::isnan(b)) {
            flags = 0x30000000;  // C=1, V=1 (unordered)
        } else if (a == b) {
            flags = 0x60000000;  // Z=1, C=1
        } else if (a < b) {
            flags = 0x80000000;  // N=1
        } else {
            flags = 0x20000000;  // C=1
        }
        state_.fpscr = (state_.fpscr & 0x0FFFFFFF) | flags;
    }
    ADVANCE_AND_DISPATCH();

handle_VcmpD:
    CHECK_COND();
    {
        double a = VFP_D_READ(inst->vfp.sd);
        double b = VFP_D_READ(inst->vfp.sm);
        u32 flags = 0;
        if (std::isnan(a) || std::isnan(b)) {
            flags = 0x30000000;  // C=1, V=1 (unordered)
        } else if (a == b) {
            flags = 0x60000000;  // Z=1, C=1
        } else if (a < b) {
            flags = 0x80000000;  // N=1
        } else {
            flags = 0x20000000;  // C=1
        }
        state_.fpscr = (state_.fpscr & 0x0FFFFFFF) | flags;
    }
    ADVANCE_AND_DISPATCH();

handle_VcmpzS:
    CHECK_COND();
    {
        float a = VFP_S_READ(inst->vfp.sd);
        u32 flags = 0;
        if (std::isnan(a)) {
            flags = 0x30000000;  // C=1, V=1 (unordered)
        } else if (a == 0.0f) {
            flags = 0x60000000;  // Z=1, C=1
        } else if (a < 0.0f) {
            flags = 0x80000000;  // N=1
        } else {
            flags = 0x20000000;  // C=1
        }
        state_.fpscr = (state_.fpscr & 0x0FFFFFFF) | flags;
    }
    ADVANCE_AND_DISPATCH();

handle_VcmpzD:
    CHECK_COND();
    {
        double a = VFP_D_READ(inst->vfp.sd);
        u32 flags = 0;
        if (std::isnan(a)) {
            flags = 0x30000000;  // C=1, V=1 (unordered)
        } else if (a == 0.0) {
            flags = 0x60000000;  // Z=1, C=1
        } else if (a < 0.0) {
            flags = 0x80000000;  // N=1
        } else {
            flags = 0x20000000;  // C=1
        }
        state_.fpscr = (state_.fpscr & 0x0FFFFFFF) | flags;
    }
    ADVANCE_AND_DISPATCH();

handle_VcvtSToInt:
    CHECK_COND();
    {
        float val = VFP_S_READ(inst->vfp.sm);
        // VCVT truncates towards zero; the cast does that directly. Out-of-range/NaN
        // values rely on ARM64 fcvtzs/fcvtzu saturation, matching VFP semantics.
        s32 result;
        if (inst->vfp.flags & 1) {
            // Unsigned
            result = static_cast<s32>(static_cast<u32>(val));
        } else {
            // Signed
            result = static_cast<s32>(val);
        }
        VFP_S_WRITE_U32(inst->vfp.sd, static_cast<u32>(result));
    }
    ADVANCE_AND_DISPATCH();

handle_VcvtIntToS:
    CHECK_COND();
    {
        u32 bits = VFP_S_READ_U32(inst->vfp.sm);
        float result;
        if (inst->vfp.flags & 1) {
            // Unsigned
            result = static_cast<float>(bits);
        } else {
            // Signed
            result = static_cast<float>(static_cast<s32>(bits));
        }
        VFP_S_WRITE(inst->vfp.sd, result);
    }
    ADVANCE_AND_DISPATCH();

handle_VcvtDToS:
    CHECK_COND();
    VFP_S_WRITE(inst->vfp.sd, static_cast<float>(VFP_D_READ(inst->vfp.sm)));
    ADVANCE_AND_DISPATCH();

handle_VcvtSToD:
    CHECK_COND();
    VFP_D_WRITE(inst->vfp.sd, static_cast<double>(VFP_S_READ(inst->vfp.sm)));
    ADVANCE_AND_DISPATCH();

handle_VcvtDToInt:
    CHECK_COND();
    {
        double val = VFP_D_READ(inst->vfp.sm);
        // VCVT truncates towards zero; the cast does that directly. Out-of-range/NaN
        // values rely on ARM64 fcvtzs/fcvtzu saturation, matching VFP semantics.
        s32 result;
        if (inst->vfp.flags & 1) {
            // Unsigned
            result = static_cast<s32>(static_cast<u32>(val));
        } else {
            // Signed
            result = static_cast<s32>(val);
        }
        VFP_S_WRITE_U32(inst->vfp.sd, static_cast<u32>(result));
    }
    ADVANCE_AND_DISPATCH();

handle_VcvtIntToD:
    CHECK_COND();
    {
        u32 bits = VFP_S_READ_U32(inst->vfp.sm);
        double result;
        if (inst->vfp.flags & 1) {
            // Unsigned
            result = static_cast<double>(bits);
        } else {
            // Signed
            result = static_cast<double>(static_cast<s32>(bits));
        }
        VFP_D_WRITE(inst->vfp.sd, result);
    }
    ADVANCE_AND_DISPATCH();

handle_VldrS:
    CHECK_COND();
    {
        u32 addr = state_.regs[inst->vfp.rn] + inst->vfp.offset;
        VFP_S_WRITE_U32(inst->vfp.sd, MemRead32(addr));
    }
    ADVANCE_AND_DISPATCH();

handle_VstrS:
    CHECK_COND();
    {
        u32 addr = state_.regs[inst->vfp.rn] + inst->vfp.offset;
        MemWrite32(addr, VFP_S_READ_U32(inst->vfp.sd));
    }
    ADVANCE_AND_DISPATCH();

handle_VldrD:
    CHECK_COND();
    {
        u32 addr = state_.regs[inst->vfp.rn] + inst->vfp.offset;
        state_.vfp_regs[inst->vfp.sd * 2] = MemRead32(addr);
        state_.vfp_regs[inst->vfp.sd * 2 + 1] = MemRead32(addr + 4);
    }
    ADVANCE_AND_DISPATCH();

handle_VstrD:
    CHECK_COND();
    {
        u32 addr = state_.regs[inst->vfp.rn] + inst->vfp.offset;
        MemWrite32(addr, state_.vfp_regs[inst->vfp.sd * 2]);
        MemWrite32(addr + 4, state_.vfp_regs[inst->vfp.sd * 2 + 1]);
    }
    ADVANCE_AND_DISPATCH();

// vfp.sd = first Sd register, vfp.sn = count, vfp.rn = base register
// vfp.flags: bit 0 = writeback, bit 1 = DB mode
handle_VldmS:
    CHECK_COND();
    {
        u32 count = inst->vfp.sn;
        u32 first_reg = inst->vfp.sd;
        bool writeback = inst->vfp.flags & 1;
        bool decrement = inst->vfp.flags & 2;
        u32 base = state_.regs[inst->vfp.rn];
        u32 addr = decrement ? (base - count * 4) : base;

        for (u32 i = 0; i < count; i++) {
            VFP_S_WRITE_U32(first_reg + i, MemRead32(addr));
            addr += 4;
        }

        if (writeback) {
            state_.regs[inst->vfp.rn] = decrement ? (base - count * 4) : (base + count * 4);
        }
    }
    ADVANCE_AND_DISPATCH();

handle_VstmS:
    CHECK_COND();
    {
        u32 count = inst->vfp.sn;
        u32 first_reg = inst->vfp.sd;
        bool writeback = inst->vfp.flags & 1;
        bool decrement = inst->vfp.flags & 2;
        u32 base = state_.regs[inst->vfp.rn];
        u32 addr = decrement ? (base - count * 4) : base;

        for (u32 i = 0; i < count; i++) {
            MemWrite32(addr, VFP_S_READ_U32(first_reg + i));
            addr += 4;
        }

        if (writeback) {
            state_.regs[inst->vfp.rn] = decrement ? (base - count * 4) : (base + count * 4);
        }
    }
    ADVANCE_AND_DISPATCH();

handle_VldmD:
    CHECK_COND();
    {
        u32 count = inst->vfp.sn;
        u32 first_reg = inst->vfp.sd;
        bool writeback = inst->vfp.flags & 1;
        bool decrement = inst->vfp.flags & 2;
        u32 base = state_.regs[inst->vfp.rn];
        u32 addr = decrement ? (base - count * 8) : base;

        for (u32 i = 0; i < count; i++) {
            state_.vfp_regs[(first_reg + i) * 2] = MemRead32(addr);
            state_.vfp_regs[(first_reg + i) * 2 + 1] = MemRead32(addr + 4);
            addr += 8;
        }

        if (writeback) {
            state_.regs[inst->vfp.rn] = decrement ? (base - count * 8) : (base + count * 8);
        }
    }
    ADVANCE_AND_DISPATCH();

handle_VstmD:
    CHECK_COND();
    {
        u32 count = inst->vfp.sn;
        u32 first_reg = inst->vfp.sd;
        bool writeback = inst->vfp.flags & 1;
        bool decrement = inst->vfp.flags & 2;
        u32 base = state_.regs[inst->vfp.rn];
        u32 addr = decrement ? (base - count * 8) : base;

        for (u32 i = 0; i < count; i++) {
            MemWrite32(addr, state_.vfp_regs[(first_reg + i) * 2]);
            MemWrite32(addr + 4, state_.vfp_regs[(first_reg + i) * 2 + 1]);
            addr += 8;
        }

        if (writeback) {
            state_.regs[inst->vfp.rn] = decrement ? (base - count * 8) : (base + count * 8);
        }
    }
    ADVANCE_AND_DISPATCH();

#undef VFP_S_READ
#undef VFP_S_WRITE
#undef VFP_S_READ_U32
#undef VFP_S_WRITE_U32
#undef VFP_D_READ
#undef VFP_D_WRITE
