// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <dynarmic/interface/A32/coprocessor.h>
#include "common/common_types.h"

struct CP15State {
    u32 cp15_thread_uprw = 0;
    u32 cp15_thread_uro = 0;
    u32 cp15_flush_prefetch_buffer = 0; ///< dummy value
    u32 cp15_data_sync_barrier = 0;     ///< dummy value
    u32 cp15_data_memory_barrier = 0;   ///< dummy value
};

class DynarmicCP15 final : public Dynarmic::A32::Coprocessor {
public:
    using CoprocReg = Dynarmic::A32::CoprocReg;

    explicit DynarmicCP15(CP15State&);
    ~DynarmicCP15() override;

    std::optional<Callback> CompileInternalOperation(bool two, unsigned opc1, CoprocReg CRd,
                                                     CoprocReg CRn, CoprocReg CRm,
                                                     unsigned opc2) override;
    CallbackOrAccessOneWord CompileSendOneWord(bool two, unsigned opc1, CoprocReg CRn,
                                               CoprocReg CRm, unsigned opc2) override;
    CallbackOrAccessTwoWords CompileSendTwoWords(bool two, unsigned opc, CoprocReg CRm) override;
    CallbackOrAccessOneWord CompileGetOneWord(bool two, unsigned opc1, CoprocReg CRn, CoprocReg CRm,
                                              unsigned opc2) override;
    CallbackOrAccessTwoWords CompileGetTwoWords(bool two, unsigned opc, CoprocReg CRm) override;
    std::optional<Callback> CompileLoadWords(bool two, bool long_transfer, CoprocReg CRd,
                                             std::optional<u8> option) override;
    std::optional<Callback> CompileStoreWords(bool two, bool long_transfer, CoprocReg CRd,
                                              std::optional<u8> option) override;

private:
    CP15State& state;
};
