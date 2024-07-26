// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstring>
#include <type_traits>
#include "common/common_types.h"

namespace Kernel {

/*
 * This class defines the SVC ABI, and provides a wrapper template for translating between ARM
 * registers and SVC parameters and return value. The SVC context class should inherit from this
 * class using CRTP (`class SVC : public SVCWrapper<SVC> {...}`), and use the Wrap() template to
 * convert a SVC function interface to a void()-type function that interacts with registers
 * interface GetReg() and SetReg().
 */
template <typename Context>
class SVCWrapper {
protected:
    template <auto F>
    void Wrap() {
        WrapHelper<decltype(F)>::Call(*static_cast<Context*>(this), F);
    };

private:
    // A special param index represents the return value
    static constexpr std::size_t INDEX_RETURN = ~(std::size_t)0;

    struct ParamSlot {
        // whether the slot is used for a param
        bool used;

        // index of a param in the function signature, starting from 0, or special value
        // INDEX_RETURN
        std::size_t param_index;

        // index of a 32-bit word within the param
        std::size_t word_index;
    };

    // Size in words of the given type in ARM ABI
    template <typename T>
    static constexpr std::size_t WordSize() {
        static_assert(std::is_trivially_copyable_v<T>);
        if constexpr (std::is_pointer_v<T>) {
            return 1;
        } else if constexpr (std::is_same_v<T, bool>) {
            return 1;
        } else {
            return (sizeof(T) - 1) / 4 + 1;
        }
    }

    // Size in words of the given type in ARM ABI with pointer removed
    template <typename T>
    static constexpr std::size_t OutputWordSize() {
        if constexpr (std::is_pointer_v<T>) {
            return WordSize<std::remove_pointer_t<T>>();
        } else {
            return 0;
        }
    }

    // Describes the register assignments of a SVC
    struct SVCABI {
        static constexpr std::size_t RegCount = 8;

        // Register assignments for input params.
        // For example, in[0] records which input param and word r0 stores
        std::array<ParamSlot, RegCount> in{};

        // Register assignments for output params.
        // For example, out[0] records which output param (with pointer removed) and word r0 stores
        std::array<ParamSlot, RegCount> out{};

        // Search the register assignments for a word of a param
        constexpr std::size_t GetRegisterIndex(std::size_t param_index,
                                               std::size_t word_index) const {
            for (std::size_t slot = 0; slot < RegCount; ++slot) {
                if (in[slot].used && in[slot].param_index == param_index &&
                    in[slot].word_index == word_index) {
                    return slot;
                }
                if (out[slot].used && out[slot].param_index == param_index &&
                    out[slot].word_index == word_index) {
                    return slot;
                }
            }
            // should throw, but gcc bugs out here
            return 0x12345678;
        }
    };

    // Generates ABI info for a given SVC signature R(Context::)(T...)
    template <typename R, typename... T>
    static constexpr SVCABI GetSVCABI() {
        constexpr std::size_t param_count = sizeof...(T);
        std::array<bool, param_count> param_is_output{{std::is_pointer_v<T>...}};
        std::array<std::size_t, param_count> param_size{{WordSize<T>()...}};
        std::array<std::size_t, param_count> output_param_size{{OutputWordSize<T>()...}};
        std::array<bool, param_count> param_need_align{
            {(std::is_same_v<T, u64> || std::is_same_v<T, s64>)...}};

        // derives ARM ABI, which assigns all params to r0~r3 and then stack
        std::array<ParamSlot, 4> armabi_reg{};
        std::array<ParamSlot, 4> armabi_stack{};
        std::size_t reg_pos = 0;
        std::size_t stack_pos = 0;
        for (std::size_t i = 0; i < param_count; ++i) {
            if (param_need_align[i]) {
                reg_pos += reg_pos % 2;
            }
            for (std::size_t j = 0; j < param_size[i]; ++j) {
                if (reg_pos == 4) {
                    armabi_stack[stack_pos++] = ParamSlot{true, i, j};
                } else {
                    armabi_reg[reg_pos++] = ParamSlot{true, i, j};
                }
            }
        }

        // now derives SVC ABI which is a modified version of ARM ABI

        SVCABI mod_abi{};
        std::size_t out_pos = 0; // next available output register

        // assign return value to output registers
        if constexpr (!std::is_void_v<R>) {
            for (std::size_t j = 0; j < WordSize<R>(); ++j) {
                mod_abi.out[out_pos++] = {true, INDEX_RETURN, j};
            }
        }

        // assign output params (pointer-type params) to output registers
        for (std::size_t i = 0; i < param_count; ++i) {
            if (param_is_output[i]) {
                for (std::size_t j = 0; j < output_param_size[i]; ++j) {
                    mod_abi.out[out_pos++] = ParamSlot{true, i, j};
                }
            }
        }

        // assign input params (non-pointer-type params) to input registers
        stack_pos = 0; // next available stack param
        for (std::size_t k = 0; k < mod_abi.in.size(); ++k) {
            if (k < armabi_reg.size() && armabi_reg[k].used &&
                !param_is_output[armabi_reg[k].param_index]) {
                // If this is within the ARM ABI register range and it is a used input param, use
                // the same register position
                mod_abi.in[k] = armabi_reg[k];
            } else {
                // Otherwise, assign it with the next available stack input
                // If all stack inputs have been allocated, this would do nothing
                // and leaves the slot unused.
                // Loop until an input stack param is found
                while (stack_pos < armabi_stack.size() &&
                       (!armabi_stack[stack_pos].used ||
                        param_is_output[armabi_stack[stack_pos].param_index])) {
                    ++stack_pos;
                }
                // Reassign the stack param to the free register
                if (stack_pos < armabi_stack.size()) {
                    mod_abi.in[k] = armabi_stack[stack_pos++];
                }
            }
        }

        return mod_abi;
    }

    template <std::size_t param_index, std::size_t word_size, std::size_t... indices>
    static constexpr std::array<std::size_t, word_size> GetRegIndicesImpl(
        SVCABI abi, std::index_sequence<indices...>) {
        return {{abi.GetRegisterIndex(param_index, indices)...}};
    }

    // Gets assigned register index for the param_index-th param of word_size in a function with
    // signature R(Context::)(Ts...)
    template <std::size_t param_index, std::size_t word_size, typename R, typename... Ts>
    static constexpr std::array<std::size_t, word_size> GetRegIndices() {
        constexpr SVCABI abi = GetSVCABI<R, Ts...>();
        return GetRegIndicesImpl<param_index, word_size>(abi,
                                                         std::make_index_sequence<word_size>{});
    }

    // Gets the value for the param_index-th param of word_size in a function with signature
    // R(Context::)(Ts...)
    // T is the param type, which must be a non-pointer as it is an input param.
    // Must hold: Ts[param_index] == T
    template <std::size_t param_index, typename T, typename R, typename... Ts>
    static void GetParam(Context& context, T& value) {
        static_assert(!std::is_pointer_v<T>, "T should not be a pointer");
        constexpr auto regi = GetRegIndices<param_index, WordSize<T>(), R, Ts...>();
        if constexpr (std::is_class_v<T> || std::is_union_v<T>) {
            std::array<u32, WordSize<T>()> buf;
            for (std::size_t i = 0; i < WordSize<T>(); ++i) {
                buf[i] = context.GetReg(regi[i]);
            }
            std::memcpy(&value, &buf, sizeof(T));
        } else if constexpr (WordSize<T>() == 2) {
            u64 l = context.GetReg(regi[0]);
            u64 h = context.GetReg(regi[1]);
            value = static_cast<T>(l | (h << 32));
        } else if constexpr (std::is_same_v<T, bool>) {
            // TODO: Is this correct or should only test the lowest byte?
            value = context.GetReg(regi[0]) != 0;
        } else {
            value = static_cast<T>(context.GetReg(regi[0]));
        }
    }

    // Sets the value for the param_index-th param of word_size in a function with signature
    // R(Context::)(Ts...)
    // T is the param type with pointer removed, which was originally a pointer-type output param
    // Must hold: (Ts[param_index] == T*) || (R==T && param_index == INDEX_RETURN)
    template <std::size_t param_index, typename T, typename R, typename... Ts>
    static void SetParam(Context& context, const T& value) {
        static_assert(!std::is_pointer_v<T>, "T should have pointer removed before passing in");
        constexpr auto regi = GetRegIndices<param_index, WordSize<T>(), R, Ts...>();
        if constexpr (std::is_class_v<T> || std::is_union_v<T>) {
            std::array<u32, WordSize<T>()> buf;
            std::memcpy(&buf, &value, sizeof(T));
            for (std::size_t i = 0; i < WordSize<T>(); ++i) {
                context.SetReg(regi[i], buf[i]);
            }
        } else if constexpr (WordSize<T>() == 2) {
            u64 u = static_cast<u64>(value);
            context.SetReg(regi[0], static_cast<u32>(u & 0xFFFFFFFF));
            context.SetReg(regi[1], static_cast<u32>(u >> 32));
        } else {
            u32 u = static_cast<u32>(value);
            context.SetReg(regi[0], u);
        }
    }

    template <typename SVCT, typename R, typename... Ts>
    struct WrapPass;

    template <typename SVCT, typename R, typename T, typename... Ts>
    struct WrapPass<SVCT, R, T, Ts...> {
        // Do I/O for the param T in function R(Context::svc)(Us..., T, Ts...) and then move on to
        // the next param.

        // Us are params whose I/O is already handled.
        // T is the current param to do I/O.
        // Ts are params whose I/O is not handled yet.
        template <typename... Us>
        static void Call(Context& context, SVCT svc, Us... u) {
            static_assert(std::is_same_v<SVCT, R (Context::*)(Us..., T, Ts...)>);
            constexpr std::size_t current_param_index = sizeof...(Us);
            if constexpr (std::is_pointer_v<T>) {
                using OutputT = std::remove_pointer_t<T>;
                OutputT output;
                WrapPass<SVCT, R, Ts...>::Call(context, svc, u..., &output);
                SetParam<current_param_index, OutputT, R, Us..., T, Ts...>(context, output);
            } else {
                T input;
                GetParam<current_param_index, T, R, Us..., T, Ts...>(context, input);
                WrapPass<SVCT, R, Ts...>::Call(context, svc, u..., input);
            }
        }
    };

    template <typename SVCT, typename R>
    struct WrapPass<SVCT, R /*empty for T, Ts...*/> {
        // Call function R(Context::svc)(Us...) and transfer the return value to registers
        template <typename... Us>
        static void Call(Context& context, SVCT svc, Us... u) {
            static_assert(std::is_same_v<SVCT, R (Context::*)(Us...)>);
            if constexpr (std::is_void_v<R>) {
                (context.*svc)(u...);
            } else {
                R r = (context.*svc)(u...);
                SetParam<INDEX_RETURN, R, R, Us...>(context, r);
            }
        }
    };

    template <typename SVCT>
    struct WrapPass<SVCT, Result /*empty for T, Ts...*/> {
        // Call function R(Context::svc)(Us...) and transfer the return value to registers
        template <typename... Us>
        static void Call(Context& context, SVCT svc, Us... u) {
            static_assert(std::is_same_v<SVCT, Result (Context::*)(Us...)>);
            if constexpr (std::is_void_v<Result>) {
                (context.*svc)(u...);
            } else {
                Result r = (context.*svc)(u...);
                if (r.IsError()) {
                    LOG_ERROR(Kernel_SVC, "level={} summary={} module={} description={}",
                              r.level.ExtractValue(r.raw), r.summary.ExtractValue(r.raw),
                              r.module.ExtractValue(r.raw), r.description.ExtractValue(r.raw));
                }
                SetParam<INDEX_RETURN, Result, Result, Us...>(context, r);
            }
        }
    };

    template <typename T>
    struct WrapHelper;

    template <typename R, typename... T>
    struct WrapHelper<R (Context::*)(T...)> {
        static void Call(Context& context, R (Context::*svc)(T...)) {
            WrapPass<decltype(svc), R, T...>::Call(context, svc /*Empty for Us*/);
        }
    };
};

} // namespace Kernel
