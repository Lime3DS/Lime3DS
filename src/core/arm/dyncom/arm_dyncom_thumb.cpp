// Copyright 2012 Michael Kang, 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>

// We can provide simple Thumb simulation by decoding the Thumb instruction into its corresponding
// ARM instruction, and using the existing ARM simulator.

#include "core/arm/dyncom/arm_dyncom_thumb.h"
#include "core/arm/skyeye_common/armsupp.h"

// Decode a 16bit Thumb instruction.  The instruction is in the low 16-bits of the tinstr field,
// with the following Thumb instruction held in the high 16-bits.  Passing in two Thumb instructions
// allows easier simulation of the special dual BL instruction.

ThumbDecodeStatus TranslateThumbInstruction(u32 addr, u32 instr, u32* ainstr, u32* inst_size) {
    ThumbDecodeStatus valid = ThumbDecodeStatus::UNINITIALIZED;
    u32 tinstr = GetThumbInstruction(instr, addr);

    *ainstr = 0xDEADC0DE; // Debugging to catch non updates

    switch ((tinstr & 0xF800) >> 11) {
    case 0:                                         // LSL
    case 1:                                         // LSR
    case 2:                                         // ASR
        *ainstr = 0xE1B00000                        // base opcode
                  | ((tinstr & 0x1800) >> (11 - 5)) // shift type
                  | ((tinstr & 0x07C0) << (7 - 6))  // imm5
                  | ((tinstr & 0x0038) >> 3)        // Rs
                  | ((tinstr & 0x0007) << 12);      // Rd
        break;

    case 3: // ADD/SUB
    {
        static const u32 subset[4] = {
            0xE0900000, // ADDS Rd,Rs,Rn
            0xE0500000, // SUBS Rd,Rs,Rn
            0xE2900000, // ADDS Rd,Rs,#imm3
            0xE2500000  // SUBS Rd,Rs,#imm3
        };
        // It is quicker indexing into a table, than performing switch or conditionals:
        *ainstr = subset[(tinstr & 0x0600) >> 9]     // base opcode
                  | ((tinstr & 0x01C0) >> 6)         // Rn or imm3
                  | ((tinstr & 0x0038) << (16 - 3))  // Rs
                  | ((tinstr & 0x0007) << (12 - 0)); // Rd
    } break;

    case 4: // MOV
    case 5: // CMP
    case 6: // ADD
    case 7: // SUB
    {
        static const u32 subset[4] = {
            0xE3B00000, // MOVS Rd,#imm8
            0xE3500000, // CMP  Rd,#imm8
            0xE2900000, // ADDS Rd,Rd,#imm8
            0xE2500000, // SUBS Rd,Rd,#imm8
        };

        *ainstr = subset[(tinstr & 0x1800) >> 11]    // base opcode
                  | ((tinstr & 0x00FF) >> 0)         // imm8
                  | ((tinstr & 0x0700) << (16 - 8))  // Rn
                  | ((tinstr & 0x0700) << (12 - 8)); // Rd
    } break;

    case 8: // Arithmetic and high register transfers

        // TODO: Since the subsets for both Format 4 and Format 5 instructions are made up of
        // different ARM encodings, we could save the following conditional, and just have one
        // large subset

        if ((tinstr & (1 << 10)) == 0) {
            enum otype { t_norm, t_shift, t_neg, t_mul };

            static const struct {
                u32 opcode;
                otype type;
            } subset[16] = {
                {0xE0100000, t_norm},  // ANDS Rd,Rd,Rs
                {0xE0300000, t_norm},  // EORS Rd,Rd,Rs
                {0xE1B00010, t_shift}, // MOVS Rd,Rd,LSL Rs
                {0xE1B00030, t_shift}, // MOVS Rd,Rd,LSR Rs
                {0xE1B00050, t_shift}, // MOVS Rd,Rd,ASR Rs
                {0xE0B00000, t_norm},  // ADCS Rd,Rd,Rs
                {0xE0D00000, t_norm},  // SBCS Rd,Rd,Rs
                {0xE1B00070, t_shift}, // MOVS Rd,Rd,ROR Rs
                {0xE1100000, t_norm},  // TST  Rd,Rs
                {0xE2700000, t_neg},   // RSBS Rd,Rs,#0
                {0xE1500000, t_norm},  // CMP  Rd,Rs
                {0xE1700000, t_norm},  // CMN  Rd,Rs
                {0xE1900000, t_norm},  // ORRS Rd,Rd,Rs
                {0xE0100090, t_mul},   // MULS Rd,Rd,Rs
                {0xE1D00000, t_norm},  // BICS Rd,Rd,Rs
                {0xE1F00000, t_norm}   // MVNS Rd,Rs
            };

            *ainstr = subset[(tinstr & 0x03C0) >> 6].opcode; // base

            switch (subset[(tinstr & 0x03C0) >> 6].type) {
            case t_norm:
                *ainstr |= ((tinstr & 0x0007) << 16)   // Rn
                           | ((tinstr & 0x0007) << 12) // Rd
                           | ((tinstr & 0x0038) >> 3); // Rs
                break;
            case t_shift:
                *ainstr |= ((tinstr & 0x0007) << 12)         // Rd
                           | ((tinstr & 0x0007) >> 0)        // Rm
                           | ((tinstr & 0x0038) << (8 - 3)); // Rs
                break;
            case t_neg:
                *ainstr |= ((tinstr & 0x0007) << 12)          // Rd
                           | ((tinstr & 0x0038) << (16 - 3)); // Rn
                break;
            case t_mul:
                *ainstr |= ((tinstr & 0x0007) << 16)   // Rd
                           | ((tinstr & 0x0007) << 8)  // Rs
                           | ((tinstr & 0x0038) >> 3); // Rm
                break;
            }
        } else {
            u32 Rd = ((tinstr & 0x0007) >> 0);
            u32 Rs = ((tinstr & 0x0078) >> 3);

            if (tinstr & (1 << 7))
                Rd += 8;

            switch ((tinstr & 0x03C0) >> 6) {
            case 0x0:                  // ADD Rd,Rd,Rs
            case 0x1:                  // ADD Rd,Rd,Hs
            case 0x2:                  // ADD Hd,Hd,Rs
            case 0x3:                  // ADD Hd,Hd,Hs
                *ainstr = 0xE0800000   // base
                          | (Rd << 16) // Rn
                          | (Rd << 12) // Rd
                          | (Rs << 0); // Rm
                break;
            case 0x4:                  // CMP Rd,Rs
            case 0x5:                  // CMP Rd,Hs
            case 0x6:                  // CMP Hd,Rs
            case 0x7:                  // CMP Hd,Hs
                *ainstr = 0xE1500000   // base
                          | (Rd << 16) // Rn
                          | (Rs << 0); // Rm
                break;
            case 0x8:                  // MOV Rd,Rs
            case 0x9:                  // MOV Rd,Hs
            case 0xA:                  // MOV Hd,Rs
            case 0xB:                  // MOV Hd,Hs
                *ainstr = 0xE1A00000   // base
                          | (Rd << 12) // Rd
                          | (Rs << 0); // Rm
                break;
            case 0xC:                                 // BX Rs
            case 0xD:                                 // BX Hs
                *ainstr = 0xE12FFF10                  // base
                          | ((tinstr & 0x0078) >> 3); // Rd
                break;
            case 0xE:                  // BLX
            case 0xF:                  // BLX
                *ainstr = 0xE1200030   // base
                          | (Rs << 0); // Rm
                break;
            }
        }
        break;

    case 9:                                         // LDR Rd,[PC,#imm8]
        *ainstr = 0xE59F0000                        // base
                  | ((tinstr & 0x0700) << (12 - 8)) // Rd
                  | ((tinstr & 0x00FF) << (2 - 0)); // off8
        break;

    case 10:
    case 11: {
        static const u32 subset[8] = {
            0xE7800000, // STR   Rd,[Rb,Ro]
            0xE18000B0, // STRH  Rd,[Rb,Ro]
            0xE7C00000, // STRB  Rd,[Rb,Ro]
            0xE19000D0, // LDRSB Rd,[Rb,Ro]
            0xE7900000, // LDR   Rd,[Rb,Ro]
            0xE19000B0, // LDRH  Rd,[Rb,Ro]
            0xE7D00000, // LDRB  Rd,[Rb,Ro]
            0xE19000F0  // LDRSH Rd,[Rb,Ro]
        };

        *ainstr = subset[(tinstr & 0xE00) >> 9]     // base
                  | ((tinstr & 0x0007) << (12 - 0)) // Rd
                  | ((tinstr & 0x0038) << (16 - 3)) // Rb
                  | ((tinstr & 0x01C0) >> 6);       // Ro
    } break;

    case 12: // STR Rd,[Rb,#imm5]
    case 13: // LDR Rd,[Rb,#imm5]
    case 14: // STRB Rd,[Rb,#imm5]
    case 15: // LDRB Rd,[Rb,#imm5]
    {
        static const u32 subset[4] = {
            0xE5800000, // STR  Rd,[Rb,#imm5]
            0xE5900000, // LDR  Rd,[Rb,#imm5]
            0xE5C00000, // STRB Rd,[Rb,#imm5]
            0xE5D00000  // LDRB Rd,[Rb,#imm5]
        };
        // The offset range defends on whether we are transferring a byte or word value:
        *ainstr = subset[(tinstr & 0x1800) >> 11]                                // base
                  | ((tinstr & 0x0007) << (12 - 0))                              // Rd
                  | ((tinstr & 0x0038) << (16 - 3))                              // Rb
                  | ((tinstr & 0x07C0) >> (6 - ((tinstr & (1 << 12)) ? 0 : 2))); // off5
    } break;

    case 16:                                        // STRH Rd,[Rb,#imm5]
    case 17:                                        // LDRH Rd,[Rb,#imm5]
        *ainstr = ((tinstr & (1 << 11))             // base
                       ? 0xE1D000B0                 // LDRH
                       : 0xE1C000B0)                // STRH
                  | ((tinstr & 0x0007) << (12 - 0)) // Rd
                  | ((tinstr & 0x0038) << (16 - 3)) // Rb
                  | ((tinstr & 0x01C0) >> (6 - 1))  // off5, low nibble
                  | ((tinstr & 0x0600) >> (9 - 8)); // off5, high nibble
        break;

    case 18:                                        // STR Rd,[SP,#imm8]
    case 19:                                        // LDR Rd,[SP,#imm8]
        *ainstr = ((tinstr & (1 << 11))             // base
                       ? 0xE59D0000                 // LDR
                       : 0xE58D0000)                // STR
                  | ((tinstr & 0x0700) << (12 - 8)) // Rd
                  | ((tinstr & 0x00FF) << 2);       // off8
        break;

    case 20: // ADD Rd,PC,#imm8
    case 21: // ADD Rd,SP,#imm8

        if ((tinstr & (1 << 11)) == 0) {

            // NOTE: The PC value used here should by word aligned. We encode shift-left-by-2 in the
            // rotate immediate field, so no shift of off8 is needed.

            *ainstr = 0xE28F0F00                        // base
                      | ((tinstr & 0x0700) << (12 - 8)) // Rd
                      | (tinstr & 0x00FF);              // off8
        } else {
            // We encode shift-left-by-2 in the rotate immediate field, so no shift of off8 is
            // needed.
            *ainstr = 0xE28D0F00                        // base
                      | ((tinstr & 0x0700) << (12 - 8)) // Rd
                      | (tinstr & 0x00FF);              // off8
        }
        break;

    case 22:
    case 23:
        if ((tinstr & 0x0F00) == 0x0000) {
            // NOTE: The instruction contains a shift left of 2 equivalent (implemented as ROR #30):
            *ainstr = ((tinstr & (1 << 7)) // base
                           ? 0xE24DDF00    // SUB
                           : 0xE28DDF00)   // ADD
                      | (tinstr & 0x007F); // off7
        } else if ((tinstr & 0x0F00) == 0x0e00) {
            // BKPT
            *ainstr = 0xEF000000                   // base
                      | BITS(tinstr, 0, 3)         // imm4 field;
                      | (BITS(tinstr, 4, 7) << 8); // beginning 4 bits of imm12
        } else if ((tinstr & 0x0F00) == 0x0200) {
            static const u32 subset[4] = {
                0xE6BF0070, // SXTH
                0xE6AF0070, // SXTB
                0xE6FF0070, // UXTH
                0xE6EF0070, // UXTB
            };

            *ainstr = subset[BITS(tinstr, 6, 7)]   // base
                      | (BITS(tinstr, 0, 2) << 12) // Rd
                      | BITS(tinstr, 3, 5);        // Rm
        } else if ((tinstr & 0x0F00) == 0x600) {
            if (BIT(tinstr, 5) == 0) {
                // SETEND
                *ainstr = 0xF1010000               // base
                          | (BIT(tinstr, 3) << 9); // endian specifier
            } else {
                // CPS
                *ainstr = 0xF1080000                // base
                          | (BIT(tinstr, 0) << 6)   // fiq bit
                          | (BIT(tinstr, 1) << 7)   // irq bit
                          | (BIT(tinstr, 2) << 8)   // abort bit
                          | (BIT(tinstr, 4) << 18); // enable bit
            }
        } else if ((tinstr & 0x0F00) == 0x0a00) {
            static const u32 subset[4] = {
                0xE6BF0F30, // REV
                0xE6BF0FB0, // REV16
                0,          // undefined
                0xE6FF0FB0, // REVSH
            };

            std::size_t subset_index = BITS(tinstr, 6, 7);

            if (subset_index == 2) {
                valid = ThumbDecodeStatus::UNDEFINED;
            } else {
                *ainstr = subset[subset_index]         // base
                          | (BITS(tinstr, 0, 2) << 12) // Rd
                          | BITS(tinstr, 3, 5);        // Rm
            }
        } else {
            static const u32 subset[4] = {
                0xE92D0000, // STMDB sp!,{rlist}
                0xE92D4000, // STMDB sp!,{rlist,lr}
                0xE8BD0000, // LDMIA sp!,{rlist}
                0xE8BD8000  // LDMIA sp!,{rlist,pc}
            };
            *ainstr = subset[((tinstr & (1 << 11)) >> 10) | ((tinstr & (1 << 8)) >> 8)] // base
                      | (tinstr & 0x00FF);                                              // mask8
        }
        break;

    case 24: //  STMIA
    case 25: //  LDMIA
        if (tinstr & (1 << 11)) {
            unsigned int base = 0xE8900000;
            unsigned int rn = BITS(tinstr, 8, 10);

            // Writeback
            if ((tinstr & (1 << rn)) == 0)
                base |= (1 << 21);

            *ainstr = base                 // base (LDMIA)
                      | (rn << 16)         // Rn
                      | (tinstr & 0x00FF); // Register list
        } else {
            *ainstr = 0xE8A00000                    // base (STMIA)
                      | (BITS(tinstr, 8, 10) << 16) // Rn
                      | (tinstr & 0x00FF);          // Register list
        }
        break;

    case 26: // Bcc
    case 27: // Bcc/SWI
        if ((tinstr & 0x0F00) == 0x0F00) {
            // Format 17 : SWI
            *ainstr = 0xEF000000;
            // Breakpoint must be handled specially.
            if ((tinstr & 0x00FF) == 0x18)
                *ainstr |= ((tinstr & 0x00FF) << 16);
            // New breakpoint value.  See gdb/arm-tdep.c
            else if ((tinstr & 0x00FF) == 0xFE)
                *ainstr |= 0x180000; // base |= BKPT mask
            else
                *ainstr |= (tinstr & 0x00FF);
        } else if ((tinstr & 0x0F00) != 0x0E00)
            valid = ThumbDecodeStatus::BRANCH;
        else //  UNDEFINED : cc=1110(AL) uses different format
            valid = ThumbDecodeStatus::UNDEFINED;

        break;

    case 28: // B
        valid = ThumbDecodeStatus::BRANCH;
        break;

    case 29:
        if (tinstr & 0x1)
            valid = ThumbDecodeStatus::UNDEFINED;
        else
            valid = ThumbDecodeStatus::BRANCH;
        break;

    case 30: // BL instruction 1

        // There is no single ARM instruction equivalent for this Thumb instruction. To keep the
        // simulation simple (from the user perspective) we check if the following instruction is
        // the second half of this BL, and if it is we simulate it immediately

        valid = ThumbDecodeStatus::BRANCH;
        break;

    case 31: // BL instruction 2

        // There is no single ARM instruction equivalent for this instruction. Also, it should only
        // ever be matched with the fmt19 "BL instruction 1" instruction. However, we do allow the
        // simulation of it on its own, with undefined results if r14 is not suitably initialised.

        valid = ThumbDecodeStatus::BRANCH;
        break;
    }

    *inst_size = 2;

    return valid;
}
