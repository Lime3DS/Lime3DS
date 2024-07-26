// Copyright 2012 Michael Kang, 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

/* Notice: this file should not be compiled as is, and is meant to be
   included in other files only. */

/* ----------------------------------------------------------------------- */
/* CDP instructions */
/* cond 1110 opc1 CRn- CRd- copr op20 CRm- CDP */

/* ----------------------------------------------------------------------- */
/* VMLA */
/* cond 1110 0D00 Vn-- Vd-- 101X N0M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
struct vmla_inst {
    unsigned int instr;
    unsigned int dp_operation;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vmla)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vmla_inst));
    vmla_inst* inst_cream = (vmla_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMLA_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vmla_inst* inst_cream = (vmla_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vmla_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VNMLS */
/* cond 1110 0D00 Vn-- Vd-- 101X N1M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
struct vmls_inst {
    unsigned int instr;
    unsigned int dp_operation;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vmls)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vmls_inst));
    vmls_inst* inst_cream = (vmls_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMLS_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vmls_inst* inst_cream = (vmls_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vmls_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VNMLA */
/* cond 1110 0D01 Vn-- Vd-- 101X N1M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
struct vnmla_inst {
    unsigned int instr;
    unsigned int dp_operation;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vnmla)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vnmla_inst));
    vnmla_inst* inst_cream = (vnmla_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VNMLA_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vnmla_inst* inst_cream = (vnmla_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vnmla_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VNMLS */
/* cond 1110 0D01 Vn-- Vd-- 101X N0M0 Vm-- */

#ifdef VFP_INTERPRETER_STRUCT
struct vnmls_inst {
    unsigned int instr;
    unsigned int dp_operation;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vnmls)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vnmls_inst));
    vnmls_inst* inst_cream = (vnmls_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VNMLS_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vnmls_inst* inst_cream = (vnmls_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vnmls_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VNMUL */
/* cond 1110 0D10 Vn-- Vd-- 101X N0M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
struct vnmul_inst {
    unsigned int instr;
    unsigned int dp_operation;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vnmul)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vnmul_inst));
    vnmul_inst* inst_cream = (vnmul_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VNMUL_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vnmul_inst* inst_cream = (vnmul_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vnmul_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VMUL */
/* cond 1110 0D10 Vn-- Vd-- 101X N0M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
struct vmul_inst {
    unsigned int instr;
    unsigned int dp_operation;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vmul)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vmul_inst));
    vmul_inst* inst_cream = (vmul_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMUL_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vmul_inst* inst_cream = (vmul_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vmul_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VADD */
/* cond 1110 0D11 Vn-- Vd-- 101X N0M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
struct vadd_inst {
    unsigned int instr;
    unsigned int dp_operation;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vadd)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vadd_inst));
    vadd_inst* inst_cream = (vadd_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VADD_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vadd_inst* inst_cream = (vadd_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vadd_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VSUB */
/* cond 1110 0D11 Vn-- Vd-- 101X N1M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
struct vsub_inst {
    unsigned int instr;
    unsigned int dp_operation;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vsub)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vsub_inst));
    vsub_inst* inst_cream = (vsub_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VSUB_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vsub_inst* inst_cream = (vsub_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vsub_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VDIV */
/* cond 1110 1D00 Vn-- Vd-- 101X N0M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
struct vdiv_inst {
    unsigned int instr;
    unsigned int dp_operation;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vdiv)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vdiv_inst));
    vdiv_inst* inst_cream = (vdiv_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VDIV_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vdiv_inst* inst_cream = (vdiv_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vdiv_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VMOVI move immediate */
/* cond 1110 1D11 im4H Vd-- 101X 0000 im4L */
/* cond 1110 opc1 CRn- CRd- copr op20 CRm- CDP */
#ifdef VFP_INTERPRETER_STRUCT
struct vmovi_inst {
    unsigned int single;
    unsigned int d;
    unsigned int imm;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vmovi)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vmovi_inst));
    vmovi_inst* inst_cream = (vmovi_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->single = BIT(inst, 8) == 0;
    inst_cream->d = (inst_cream->single ? BITS(inst, 12, 15) << 1 | BIT(inst, 22)
                                        : BITS(inst, 12, 15) | BIT(inst, 22) << 4);
    unsigned int imm8 = BITS(inst, 16, 19) << 4 | BITS(inst, 0, 3);
    if (inst_cream->single)
        inst_cream->imm = BIT(imm8, 7) << 31 | (BIT(imm8, 6) == 0) << 30 |
                          (BIT(imm8, 6) ? 0x1f : 0) << 25 | BITS(imm8, 0, 5) << 19;
    else
        inst_cream->imm = BIT(imm8, 7) << 31 | (BIT(imm8, 6) == 0) << 30 |
                          (BIT(imm8, 6) ? 0xff : 0) << 22 | BITS(imm8, 0, 5) << 16;
    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMOVI_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vmovi_inst* inst_cream = (vmovi_inst*)inst_base->component;

        VMOVI(cpu, inst_cream->single, inst_cream->d, inst_cream->imm);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vmovi_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VMOVR move register */
/* cond 1110 1D11 0000 Vd-- 101X 01M0 Vm-- */
/* cond 1110 opc1 CRn- CRd- copr op20 CRm- CDP */
#ifdef VFP_INTERPRETER_STRUCT
struct vmovr_inst {
    unsigned int single;
    unsigned int d;
    unsigned int m;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vmovr)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vmovr_inst));
    vmovr_inst* inst_cream = (vmovr_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->single = BIT(inst, 8) == 0;
    inst_cream->d = (inst_cream->single ? BITS(inst, 12, 15) << 1 | BIT(inst, 22)
                                        : BITS(inst, 12, 15) | BIT(inst, 22) << 4);
    inst_cream->m = (inst_cream->single ? BITS(inst, 0, 3) << 1 | BIT(inst, 5)
                                        : BITS(inst, 0, 3) | BIT(inst, 5) << 4);
    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMOVR_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vmovr_inst* inst_cream = (vmovr_inst*)inst_base->component;

        VMOVR(cpu, inst_cream->single, inst_cream->d, inst_cream->m);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vmovr_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VABS */
/* cond 1110 1D11 0000 Vd-- 101X 11M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vabs_inst {
    unsigned int instr;
    unsigned int dp_operation;
} vabs_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vabs)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vabs_inst));
    vabs_inst* inst_cream = (vabs_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VABS_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vabs_inst* inst_cream = (vabs_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vabs_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VNEG */
/* cond 1110 1D11 0001 Vd-- 101X 11M0 Vm-- */

#ifdef VFP_INTERPRETER_STRUCT
struct vneg_inst {
    unsigned int instr;
    unsigned int dp_operation;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vneg)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vneg_inst));
    vneg_inst* inst_cream = (vneg_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VNEG_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vneg_inst* inst_cream = (vneg_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vneg_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VSQRT */
/* cond 1110 1D11 0001 Vd-- 101X 11M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
struct vsqrt_inst {
    unsigned int instr;
    unsigned int dp_operation;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vsqrt)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vsqrt_inst));
    vsqrt_inst* inst_cream = (vsqrt_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VSQRT_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vsqrt_inst* inst_cream = (vsqrt_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vsqrt_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VCMP VCMPE */
/* cond 1110 1D11 0100 Vd-- 101X E1M0 Vm-- Encoding 1 */
#ifdef VFP_INTERPRETER_STRUCT
struct vcmp_inst {
    unsigned int instr;
    unsigned int dp_operation;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vcmp)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vcmp_inst));
    vcmp_inst* inst_cream = (vcmp_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VCMP_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vcmp_inst* inst_cream = (vcmp_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vcmp_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VCMP VCMPE */
/* cond 1110 1D11 0100 Vd-- 101X E1M0 Vm-- Encoding 2 */
#ifdef VFP_INTERPRETER_STRUCT
struct vcmp2_inst {
    unsigned int instr;
    unsigned int dp_operation;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vcmp2)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vcmp2_inst));
    vcmp2_inst* inst_cream = (vcmp2_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VCMP2_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vcmp2_inst* inst_cream = (vcmp2_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vcmp2_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VCVTBDS between double and single */
/* cond 1110 1D11 0111 Vd-- 101X 11M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
struct vcvtbds_inst {
    unsigned int instr;
    unsigned int dp_operation;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vcvtbds)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vcvtbds_inst));
    vcvtbds_inst* inst_cream = (vcvtbds_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VCVTBDS_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vcvtbds_inst* inst_cream = (vcvtbds_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vcvtbds_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VCVTBFF between floating point and fixed point */
/* cond 1110 1D11 1op2 Vd-- 101X X1M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
struct vcvtbff_inst {
    unsigned int instr;
    unsigned int dp_operation;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vcvtbff)(unsigned int inst, int index) {
    VFP_DEBUG_UNTESTED(VCVTBFF);

    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vcvtbff_inst));
    vcvtbff_inst* inst_cream = (vcvtbff_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VCVTBFF_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vcvtbff_inst* inst_cream = (vcvtbff_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vcvtbff_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VCVTBFI between floating point and integer */
/* cond 1110 1D11 1op2 Vd-- 101X X1M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
struct vcvtbfi_inst {
    unsigned int instr;
    unsigned int dp_operation;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vcvtbfi)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vcvtbfi_inst));
    vcvtbfi_inst* inst_cream = (vcvtbfi_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->dp_operation = BIT(inst, 8);
    inst_cream->instr = inst;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VCVTBFI_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vcvtbfi_inst* inst_cream = (vcvtbfi_inst*)inst_base->component;

        int ret;

        if (inst_cream->dp_operation)
            ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);
        else
            ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

        CHECK_VFP_CDP_RET;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vcvtbfi_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* MRC / MCR instructions */
/* cond 1110 AAAL XXXX XXXX 101C XBB1 XXXX */
/* cond 1110 op11 CRn- Rt-- copr op21 CRm- */

/* ----------------------------------------------------------------------- */
/* VMOVBRS between register and single precision */
/* cond 1110 000o Vn-- Rt-- 1010 N001 0000 */
/* cond 1110 op11 CRn- Rt-- copr op21 CRm- MRC */
#ifdef VFP_INTERPRETER_STRUCT
struct vmovbrs_inst {
    unsigned int to_arm;
    unsigned int t;
    unsigned int n;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vmovbrs)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vmovbrs_inst));
    vmovbrs_inst* inst_cream = (vmovbrs_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->to_arm = BIT(inst, 20) == 1;
    inst_cream->t = BITS(inst, 12, 15);
    inst_cream->n = BIT(inst, 7) | BITS(inst, 16, 19) << 1;

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMOVBRS_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vmovbrs_inst* inst_cream = (vmovbrs_inst*)inst_base->component;

        VMOVBRS(cpu, inst_cream->to_arm, inst_cream->t, inst_cream->n, &(cpu->Reg[inst_cream->t]));
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vmovbrs_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VMSR */
/* cond 1110 1110 reg- Rt-- 1010 0001 0000 */
/* cond 1110 op10 CRn- Rt-- copr op21 CRm- MCR */
#ifdef VFP_INTERPRETER_STRUCT
struct vmsr_inst {
    unsigned int reg;
    unsigned int Rt;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vmsr)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vmsr_inst));
    vmsr_inst* inst_cream = (vmsr_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->reg = BITS(inst, 16, 19);
    inst_cream->Rt = BITS(inst, 12, 15);

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMSR_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        /* FIXME: special case for access to FPSID and FPEXC, VFP must be disabled ,
           and in privileged mode */
        /* Exceptions must be checked, according to v7 ref manual */
        CHECK_VFP_ENABLED;

        vmsr_inst* const inst_cream = (vmsr_inst*)inst_base->component;

        unsigned int reg = inst_cream->reg;
        unsigned int rt = inst_cream->Rt;

        if (reg == 1) {
            cpu->VFP[VFP_FPSCR] = cpu->Reg[rt];
        } else if (cpu->InAPrivilegedMode()) {
            if (reg == 8)
                cpu->VFP[VFP_FPEXC] = cpu->Reg[rt];
            else if (reg == 9)
                cpu->VFP[VFP_FPINST] = cpu->Reg[rt];
            else if (reg == 10)
                cpu->VFP[VFP_FPINST2] = cpu->Reg[rt];
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vmsr_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VMOVBRC register to scalar */
/* cond 1110 0XX0 Vd-- Rt-- 1011 DXX1 0000 */
/* cond 1110 op10 CRn- Rt-- copr op21 CRm- MCR */
#ifdef VFP_INTERPRETER_STRUCT
struct vmovbrc_inst {
    unsigned int esize;
    unsigned int index;
    unsigned int d;
    unsigned int t;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vmovbrc)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vmovbrc_inst));
    vmovbrc_inst* inst_cream = (vmovbrc_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->d = BITS(inst, 16, 19) | BIT(inst, 7) << 4;
    inst_cream->t = BITS(inst, 12, 15);
    /* VFP variant of instruction */
    inst_cream->esize = 32;
    inst_cream->index = BIT(inst, 21);

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMOVBRC_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vmovbrc_inst* const inst_cream = (vmovbrc_inst*)inst_base->component;

        cpu->ExtReg[(2 * inst_cream->d) + inst_cream->index] = cpu->Reg[inst_cream->t];
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vmovbrc_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VMRS */
/* cond 1110 1111 CRn- Rt-- 1010 0001 0000 */
/* cond 1110 op11 CRn- Rt-- copr op21 CRm- MRC */
#ifdef VFP_INTERPRETER_STRUCT
struct vmrs_inst {
    unsigned int reg;
    unsigned int Rt;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vmrs)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vmrs_inst));
    vmrs_inst* inst_cream = (vmrs_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->reg = BITS(inst, 16, 19);
    inst_cream->Rt = BITS(inst, 12, 15);

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMRS_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        /* FIXME: special case for access to FPSID and FPEXC, VFP must be disabled,
           and in privileged mode */
        /* Exceptions must be checked, according to v7 ref manual */
        CHECK_VFP_ENABLED;

        vmrs_inst* const inst_cream = (vmrs_inst*)inst_base->component;

        unsigned int reg = inst_cream->reg;
        unsigned int rt = inst_cream->Rt;

        if (reg == 1) // FPSCR
        {
            if (rt != 15) {
                cpu->Reg[rt] = cpu->VFP[VFP_FPSCR];
            } else {
                cpu->NFlag = (cpu->VFP[VFP_FPSCR] >> 31) & 1;
                cpu->ZFlag = (cpu->VFP[VFP_FPSCR] >> 30) & 1;
                cpu->CFlag = (cpu->VFP[VFP_FPSCR] >> 29) & 1;
                cpu->VFlag = (cpu->VFP[VFP_FPSCR] >> 28) & 1;
            }
        } else if (reg == 0) {
            cpu->Reg[rt] = cpu->VFP[VFP_FPSID];
        } else if (reg == 6) {
            cpu->Reg[rt] = cpu->VFP[VFP_MVFR1];
        } else if (reg == 7) {
            cpu->Reg[rt] = cpu->VFP[VFP_MVFR0];
        } else if (cpu->InAPrivilegedMode()) {
            if (reg == 8)
                cpu->Reg[rt] = cpu->VFP[VFP_FPEXC];
            else if (reg == 9)
                cpu->Reg[rt] = cpu->VFP[VFP_FPINST];
            else if (reg == 10)
                cpu->Reg[rt] = cpu->VFP[VFP_FPINST2];
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vmrs_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VMOVBCR scalar to register */
/* cond 1110 XXX1 Vd-- Rt-- 1011 NXX1 0000 */
/* cond 1110 op11 CRn- Rt-- copr op21 CRm- MCR */
#ifdef VFP_INTERPRETER_STRUCT
struct vmovbcr_inst {
    unsigned int esize;
    unsigned int index;
    unsigned int d;
    unsigned int t;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vmovbcr)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vmovbcr_inst));
    vmovbcr_inst* inst_cream = (vmovbcr_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->d = BITS(inst, 16, 19) | BIT(inst, 7) << 4;
    inst_cream->t = BITS(inst, 12, 15);
    /* VFP variant of instruction */
    inst_cream->esize = 32;
    inst_cream->index = BIT(inst, 21);

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMOVBCR_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vmovbcr_inst* const inst_cream = (vmovbcr_inst*)inst_base->component;

        cpu->Reg[inst_cream->t] = cpu->ExtReg[(2 * inst_cream->d) + inst_cream->index];
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vmovbcr_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* MRRC / MCRR instructions */
/* cond 1100 0101 Rt2- Rt-- copr opc1 CRm- MRRC */
/* cond 1100 0100 Rt2- Rt-- copr opc1 CRm- MCRR */

/* ----------------------------------------------------------------------- */
/* VMOVBRRSS between 2 registers to 2 singles */
/* cond 1100 010X Rt2- Rt-- 1010 00X1 Vm-- */
/* cond 1100 0101 Rt2- Rt-- copr opc1 CRm- MRRC */
#ifdef VFP_INTERPRETER_STRUCT
struct vmovbrrss_inst {
    unsigned int to_arm;
    unsigned int t;
    unsigned int t2;
    unsigned int m;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vmovbrrss)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vmovbrrss_inst));
    vmovbrrss_inst* inst_cream = (vmovbrrss_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->to_arm = BIT(inst, 20) == 1;
    inst_cream->t = BITS(inst, 12, 15);
    inst_cream->t2 = BITS(inst, 16, 19);
    inst_cream->m = BITS(inst, 0, 3) << 1 | BIT(inst, 5);

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMOVBRRSS_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vmovbrrss_inst* const inst_cream = (vmovbrrss_inst*)inst_base->component;

        VMOVBRRSS(cpu, inst_cream->to_arm, inst_cream->t, inst_cream->t2, inst_cream->m,
                  &cpu->Reg[inst_cream->t], &cpu->Reg[inst_cream->t2]);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vmovbrrss_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VMOVBRRD between 2 registers and 1 double */
/* cond 1100 010X Rt2- Rt-- 1011 00X1 Vm-- */
/* cond 1100 0101 Rt2- Rt-- copr opc1 CRm- MRRC */
#ifdef VFP_INTERPRETER_STRUCT
struct vmovbrrd_inst {
    unsigned int to_arm;
    unsigned int t;
    unsigned int t2;
    unsigned int m;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vmovbrrd)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vmovbrrd_inst));
    vmovbrrd_inst* inst_cream = (vmovbrrd_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->to_arm = BIT(inst, 20) == 1;
    inst_cream->t = BITS(inst, 12, 15);
    inst_cream->t2 = BITS(inst, 16, 19);
    inst_cream->m = BIT(inst, 5) << 4 | BITS(inst, 0, 3);

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMOVBRRD_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vmovbrrd_inst* inst_cream = (vmovbrrd_inst*)inst_base->component;

        VMOVBRRD(cpu, inst_cream->to_arm, inst_cream->t, inst_cream->t2, inst_cream->m,
                 &(cpu->Reg[inst_cream->t]), &(cpu->Reg[inst_cream->t2]));
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vmovbrrd_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* LDC/STC between 2 registers and 1 double */
/* cond 110X XXX1 Rn-- CRd- copr imm- imm- LDC */
/* cond 110X XXX0 Rn-- CRd- copr imm8 imm8 STC */

/* ----------------------------------------------------------------------- */
/* VSTR */
/* cond 1101 UD00 Rn-- Vd-- 101X imm8 imm8 */
#ifdef VFP_INTERPRETER_STRUCT
struct vstr_inst {
    unsigned int single;
    unsigned int n;
    unsigned int d;
    unsigned int imm32;
    unsigned int add;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vstr)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vstr_inst));
    vstr_inst* inst_cream = (vstr_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->single = BIT(inst, 8) == 0;
    inst_cream->add = BIT(inst, 23);
    inst_cream->imm32 = BITS(inst, 0, 7) << 2;
    inst_cream->d = (inst_cream->single ? BITS(inst, 12, 15) << 1 | BIT(inst, 22)
                                        : BITS(inst, 12, 15) | BIT(inst, 22) << 4);
    inst_cream->n = BITS(inst, 16, 19);

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VSTR_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vstr_inst* inst_cream = (vstr_inst*)inst_base->component;

        unsigned int base = (inst_cream->n == 15 ? (cpu->Reg[inst_cream->n] & 0xFFFFFFFC) + 8
                                                 : cpu->Reg[inst_cream->n]);
        addr = (inst_cream->add ? base + inst_cream->imm32 : base - inst_cream->imm32);

        if (inst_cream->single) {
            cpu->WriteMemory32(addr, cpu->ExtReg[inst_cream->d]);
        } else {
            const u32 word1 = cpu->ExtReg[inst_cream->d * 2 + 0];
            const u32 word2 = cpu->ExtReg[inst_cream->d * 2 + 1];

            if (cpu->InBigEndianMode()) {
                cpu->WriteMemory32(addr + 0, word2);
                cpu->WriteMemory32(addr + 4, word1);
            } else {
                cpu->WriteMemory32(addr + 0, word1);
                cpu->WriteMemory32(addr + 4, word2);
            }
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vstr_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VPUSH */
/* cond 1101 0D10 1101 Vd-- 101X imm8 imm8 */
#ifdef VFP_INTERPRETER_STRUCT
struct vpush_inst {
    unsigned int single;
    unsigned int d;
    unsigned int imm32;
    unsigned int regs;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vpush)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vpush_inst));
    vpush_inst* inst_cream = (vpush_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->single = BIT(inst, 8) == 0;
    inst_cream->d = (inst_cream->single ? BITS(inst, 12, 15) << 1 | BIT(inst, 22)
                                        : BITS(inst, 12, 15) | BIT(inst, 22) << 4);
    inst_cream->imm32 = BITS(inst, 0, 7) << 2;
    inst_cream->regs = (inst_cream->single ? BITS(inst, 0, 7) : BITS(inst, 1, 7));

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VPUSH_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vpush_inst* inst_cream = (vpush_inst*)inst_base->component;

        addr = cpu->Reg[R13] - inst_cream->imm32;

        for (unsigned int i = 0; i < inst_cream->regs; i++) {
            if (inst_cream->single) {
                cpu->WriteMemory32(addr, cpu->ExtReg[inst_cream->d + i]);
                addr += 4;
            } else {
                const u32 word1 = cpu->ExtReg[(inst_cream->d + i) * 2 + 0];
                const u32 word2 = cpu->ExtReg[(inst_cream->d + i) * 2 + 1];

                if (cpu->InBigEndianMode()) {
                    cpu->WriteMemory32(addr + 0, word2);
                    cpu->WriteMemory32(addr + 4, word1);
                } else {
                    cpu->WriteMemory32(addr + 0, word1);
                    cpu->WriteMemory32(addr + 4, word2);
                }

                addr += 8;
            }
        }

        cpu->Reg[R13] -= inst_cream->imm32;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vpush_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VSTM */
/* cond 110P UDW0 Rn-- Vd-- 101X imm8 imm8 */
#ifdef VFP_INTERPRETER_STRUCT
struct vstm_inst {
    unsigned int single;
    unsigned int add;
    unsigned int wback;
    unsigned int d;
    unsigned int n;
    unsigned int imm32;
    unsigned int regs;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vstm)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vstm_inst));
    vstm_inst* inst_cream = (vstm_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->single = BIT(inst, 8) == 0;
    inst_cream->add = BIT(inst, 23);
    inst_cream->wback = BIT(inst, 21);
    inst_cream->d = (inst_cream->single ? BITS(inst, 12, 15) << 1 | BIT(inst, 22)
                                        : BITS(inst, 12, 15) | BIT(inst, 22) << 4);
    inst_cream->n = BITS(inst, 16, 19);
    inst_cream->imm32 = BITS(inst, 0, 7) << 2;
    inst_cream->regs = (inst_cream->single ? BITS(inst, 0, 7) : BITS(inst, 1, 7));

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VSTM_INST : /* encoding 1 */
{
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vstm_inst* inst_cream = (vstm_inst*)inst_base->component;

        u32 address = cpu->Reg[inst_cream->n];

        // Only possible in ARM mode, where PC accesses have an 8 byte offset.
        if (inst_cream->n == 15)
            address += 8;

        if (inst_cream->add == 0)
            address -= inst_cream->imm32;

        for (unsigned int i = 0; i < inst_cream->regs; i++) {
            if (inst_cream->single) {
                cpu->WriteMemory32(address, cpu->ExtReg[inst_cream->d + i]);
                address += 4;
            } else {
                const u32 word1 = cpu->ExtReg[(inst_cream->d + i) * 2 + 0];
                const u32 word2 = cpu->ExtReg[(inst_cream->d + i) * 2 + 1];

                if (cpu->InBigEndianMode()) {
                    cpu->WriteMemory32(address + 0, word2);
                    cpu->WriteMemory32(address + 4, word1);
                } else {
                    cpu->WriteMemory32(address + 0, word1);
                    cpu->WriteMemory32(address + 4, word2);
                }

                address += 8;
            }
        }
        if (inst_cream->wback) {
            cpu->Reg[inst_cream->n] =
                (inst_cream->add ? cpu->Reg[inst_cream->n] + inst_cream->imm32
                                 : cpu->Reg[inst_cream->n] - inst_cream->imm32);
        }
    }
    cpu->Reg[15] += 4;
    INC_PC(sizeof(vstm_inst));

    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VPOP */
/* cond 1100 1D11 1101 Vd-- 101X imm8 imm8 */
#ifdef VFP_INTERPRETER_STRUCT
struct vpop_inst {
    unsigned int single;
    unsigned int d;
    unsigned int imm32;
    unsigned int regs;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vpop)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vpop_inst));
    vpop_inst* inst_cream = (vpop_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->single = BIT(inst, 8) == 0;
    inst_cream->d = (inst_cream->single ? (BITS(inst, 12, 15) << 1) | BIT(inst, 22)
                                        : BITS(inst, 12, 15) | (BIT(inst, 22) << 4));
    inst_cream->imm32 = BITS(inst, 0, 7) << 2;
    inst_cream->regs = (inst_cream->single ? BITS(inst, 0, 7) : BITS(inst, 1, 7));

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VPOP_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vpop_inst* inst_cream = (vpop_inst*)inst_base->component;

        addr = cpu->Reg[R13];

        for (unsigned int i = 0; i < inst_cream->regs; i++) {
            if (inst_cream->single) {
                cpu->ExtReg[inst_cream->d + i] = cpu->ReadMemory32(addr);
                addr += 4;
            } else {
                const u32 word1 = cpu->ReadMemory32(addr + 0);
                const u32 word2 = cpu->ReadMemory32(addr + 4);

                if (cpu->InBigEndianMode()) {
                    cpu->ExtReg[(inst_cream->d + i) * 2 + 0] = word2;
                    cpu->ExtReg[(inst_cream->d + i) * 2 + 1] = word1;
                } else {
                    cpu->ExtReg[(inst_cream->d + i) * 2 + 0] = word1;
                    cpu->ExtReg[(inst_cream->d + i) * 2 + 1] = word2;
                }

                addr += 8;
            }
        }
        cpu->Reg[R13] += inst_cream->imm32;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vpop_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VLDR */
/* cond 1101 UD01 Rn-- Vd-- 101X imm8 imm8 */
#ifdef VFP_INTERPRETER_STRUCT
struct vldr_inst {
    unsigned int single;
    unsigned int n;
    unsigned int d;
    unsigned int imm32;
    unsigned int add;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vldr)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vldr_inst));
    vldr_inst* inst_cream = (vldr_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->single = BIT(inst, 8) == 0;
    inst_cream->add = BIT(inst, 23);
    inst_cream->imm32 = BITS(inst, 0, 7) << 2;
    inst_cream->d = (inst_cream->single ? BITS(inst, 12, 15) << 1 | BIT(inst, 22)
                                        : BITS(inst, 12, 15) | BIT(inst, 22) << 4);
    inst_cream->n = BITS(inst, 16, 19);

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VLDR_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vldr_inst* inst_cream = (vldr_inst*)inst_base->component;

        unsigned int base = (inst_cream->n == 15 ? (cpu->Reg[inst_cream->n] & 0xFFFFFFFC) + 8
                                                 : cpu->Reg[inst_cream->n]);
        addr = (inst_cream->add ? base + inst_cream->imm32 : base - inst_cream->imm32);

        if (inst_cream->single) {
            cpu->ExtReg[inst_cream->d] = cpu->ReadMemory32(addr);
        } else {
            const u32 word1 = cpu->ReadMemory32(addr + 0);
            const u32 word2 = cpu->ReadMemory32(addr + 4);

            if (cpu->InBigEndianMode()) {
                cpu->ExtReg[inst_cream->d * 2 + 0] = word2;
                cpu->ExtReg[inst_cream->d * 2 + 1] = word1;
            } else {
                cpu->ExtReg[inst_cream->d * 2 + 0] = word1;
                cpu->ExtReg[inst_cream->d * 2 + 1] = word2;
            }
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vldr_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif

/* ----------------------------------------------------------------------- */
/* VLDM */
/* cond 110P UDW1 Rn-- Vd-- 101X imm8 imm8 */
#ifdef VFP_INTERPRETER_STRUCT
struct vldm_inst {
    unsigned int single;
    unsigned int add;
    unsigned int wback;
    unsigned int d;
    unsigned int n;
    unsigned int imm32;
    unsigned int regs;
};
#endif
#ifdef VFP_INTERPRETER_TRANS
static ARM_INST_PTR INTERPRETER_TRANSLATE(vldm)(unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(vldm_inst));
    vldm_inst* inst_cream = (vldm_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->single = BIT(inst, 8) == 0;
    inst_cream->add = BIT(inst, 23);
    inst_cream->wback = BIT(inst, 21);
    inst_cream->d = (inst_cream->single ? BITS(inst, 12, 15) << 1 | BIT(inst, 22)
                                        : BITS(inst, 12, 15) | BIT(inst, 22) << 4);
    inst_cream->n = BITS(inst, 16, 19);
    inst_cream->imm32 = BITS(inst, 0, 7) << 2;
    inst_cream->regs = (inst_cream->single ? BITS(inst, 0, 7) : BITS(inst, 1, 7));

    return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VLDM_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        CHECK_VFP_ENABLED;

        vldm_inst* inst_cream = (vldm_inst*)inst_base->component;

        u32 address = cpu->Reg[inst_cream->n];

        // Only possible in ARM mode, where PC accesses have an 8 byte offset.
        if (inst_cream->n == 15)
            address += 8;

        if (inst_cream->add == 0)
            address -= inst_cream->imm32;

        for (unsigned int i = 0; i < inst_cream->regs; i++) {
            if (inst_cream->single) {
                cpu->ExtReg[inst_cream->d + i] = cpu->ReadMemory32(address);
                address += 4;
            } else {
                const u32 word1 = cpu->ReadMemory32(address + 0);
                const u32 word2 = cpu->ReadMemory32(address + 4);

                if (cpu->InBigEndianMode()) {
                    cpu->ExtReg[(inst_cream->d + i) * 2 + 0] = word2;
                    cpu->ExtReg[(inst_cream->d + i) * 2 + 1] = word1;
                } else {
                    cpu->ExtReg[(inst_cream->d + i) * 2 + 0] = word1;
                    cpu->ExtReg[(inst_cream->d + i) * 2 + 1] = word2;
                }

                address += 8;
            }
        }
        if (inst_cream->wback) {
            cpu->Reg[inst_cream->n] =
                (inst_cream->add ? cpu->Reg[inst_cream->n] + inst_cream->imm32
                                 : cpu->Reg[inst_cream->n] - inst_cream->imm32);
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(vldm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
#endif
