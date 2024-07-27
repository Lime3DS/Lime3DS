// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/dynarmic/arm_dynarmic_cp15.h"
#include "core/arm/skyeye_common/arm_regformat.h"
#include "core/arm/skyeye_common/armstate.h"

using Callback = Dynarmic::A32::Coprocessor::Callback;
using CallbackOrAccessOneWord = Dynarmic::A32::Coprocessor::CallbackOrAccessOneWord;
using CallbackOrAccessTwoWords = Dynarmic::A32::Coprocessor::CallbackOrAccessTwoWords;

DynarmicCP15::DynarmicCP15(CP15State& state) : state(state) {}

DynarmicCP15::~DynarmicCP15() = default;

std::optional<Callback> DynarmicCP15::CompileInternalOperation(bool two, unsigned opc1,
                                                               CoprocReg CRd, CoprocReg CRn,
                                                               CoprocReg CRm, unsigned opc2) {
    return std::nullopt;
}

CallbackOrAccessOneWord DynarmicCP15::CompileSendOneWord(bool two, unsigned opc1, CoprocReg CRn,
                                                         CoprocReg CRm, unsigned opc2) {
    // TODO(merry): Privileged CP15 registers

    if (!two && CRn == CoprocReg::C7 && opc1 == 0 && CRm == CoprocReg::C5 && opc2 == 4) {
        // This is a dummy write, we ignore the value written here.
        return &state.cp15_flush_prefetch_buffer;
    }

    if (!two && CRn == CoprocReg::C7 && opc1 == 0 && CRm == CoprocReg::C10) {
        switch (opc2) {
        case 4:
            // This is a dummy write, we ignore the value written here.
            return &state.cp15_data_sync_barrier;
        case 5:
            // This is a dummy write, we ignore the value written here.
            return &state.cp15_data_memory_barrier;
        default:
            return std::monostate{};
        }
    }

    if (!two && CRn == CoprocReg::C13 && opc1 == 0 && CRm == CoprocReg::C0 && opc2 == 2) {
        return &state.cp15_thread_uprw;
    }

    return std::monostate{};
}

CallbackOrAccessTwoWords DynarmicCP15::CompileSendTwoWords(bool two, unsigned opc, CoprocReg CRm) {
    return std::monostate{};
}

CallbackOrAccessOneWord DynarmicCP15::CompileGetOneWord(bool two, unsigned opc1, CoprocReg CRn,
                                                        CoprocReg CRm, unsigned opc2) {
    // TODO(merry): Privileged CP15 registers

    if (!two && CRn == CoprocReg::C13 && opc1 == 0 && CRm == CoprocReg::C0) {
        switch (opc2) {
        case 2:
            return &state.cp15_thread_uprw;
        case 3:
            return &state.cp15_thread_uro;
        default:
            return std::monostate{};
        }
    }

    return std::monostate{};
}

CallbackOrAccessTwoWords DynarmicCP15::CompileGetTwoWords(bool two, unsigned opc, CoprocReg CRm) {
    return std::monostate{};
}

std::optional<Callback> DynarmicCP15::CompileLoadWords(bool two, bool long_transfer, CoprocReg CRd,
                                                       std::optional<u8> option) {
    return std::nullopt;
}

std::optional<Callback> DynarmicCP15::CompileStoreWords(bool two, bool long_transfer, CoprocReg CRd,
                                                        std::optional<u8> option) {
    return std::nullopt;
}
