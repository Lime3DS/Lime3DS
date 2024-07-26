// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cmath>
#include "video_core/renderer_software/sw_proctex.h"

namespace SwRenderer {

namespace {
using ProcTexClamp = Pica::TexturingRegs::ProcTexClamp;
using ProcTexShift = Pica::TexturingRegs::ProcTexShift;
using ProcTexCombiner = Pica::TexturingRegs::ProcTexCombiner;
using ProcTexFilter = Pica::TexturingRegs::ProcTexFilter;
using Pica::f16;

float LookupLUT(const std::array<Pica::PicaCore::ProcTex::ValueEntry, 128>& lut, float coord) {
    // For NoiseLUT/ColorMap/AlphaMap, coord=0.0 is lut[0], coord=127.0/128.0 is lut[127] and
    // coord=1.0 is lut[127]+lut_diff[127]. For other indices, the result is interpolated using
    // value entries and difference entries.
    coord *= 128;
    const int index_int = std::min(static_cast<int>(coord), 127);
    const float frac = coord - index_int;
    return lut[index_int].ToFloat() + frac * lut[index_int].DiffToFloat();
}

// These function are used to generate random noise for procedural texture. Their results are
// verified against real hardware, but it's not known if the algorithm is the same as hardware.
unsigned int NoiseRand1D(unsigned int v) {
    static constexpr std::array<unsigned int, 16> table{
        {0, 4, 10, 8, 4, 9, 7, 12, 5, 15, 13, 14, 11, 15, 2, 11}};
    return ((v % 9 + 2) * 3 & 0xF) ^ table[(v / 9) & 0xF];
}

float NoiseRand2D(unsigned int x, unsigned int y) {
    static constexpr std::array<unsigned int, 16> table{
        {10, 2, 15, 8, 0, 7, 4, 5, 5, 13, 2, 6, 13, 9, 3, 14}};
    unsigned int u2 = NoiseRand1D(x);
    unsigned int v2 = NoiseRand1D(y);
    v2 += ((u2 & 3) == 1) ? 4 : 0;
    v2 ^= (u2 & 1) * 6;
    v2 += 10 + u2;
    v2 &= 0xF;
    v2 ^= table[u2];
    return -1.0f + v2 * 2.0f / 15.0f;
}

float NoiseCoef(float u, float v, const Pica::TexturingRegs& regs,
                const Pica::PicaCore::ProcTex& state) {
    const float freq_u = f16::FromRaw(regs.proctex_noise_frequency.u).ToFloat32();
    const float freq_v = f16::FromRaw(regs.proctex_noise_frequency.v).ToFloat32();
    const float phase_u = f16::FromRaw(regs.proctex_noise_u.phase).ToFloat32();
    const float phase_v = f16::FromRaw(regs.proctex_noise_v.phase).ToFloat32();
    const float x = 9 * freq_u * std::abs(u + phase_u);
    const float y = 9 * freq_v * std::abs(v + phase_v);
    const int x_int = static_cast<int>(x);
    const int y_int = static_cast<int>(y);
    const float x_frac = x - x_int;
    const float y_frac = y - y_int;

    const float g0 = NoiseRand2D(x_int, y_int) * (x_frac + y_frac);
    const float g1 = NoiseRand2D(x_int + 1, y_int) * (x_frac + y_frac - 1);
    const float g2 = NoiseRand2D(x_int, y_int + 1) * (x_frac + y_frac - 1);
    const float g3 = NoiseRand2D(x_int + 1, y_int + 1) * (x_frac + y_frac - 2);
    const float x_noise = LookupLUT(state.noise_table, x_frac);
    const float y_noise = LookupLUT(state.noise_table, y_frac);
    return Common::BilinearInterp(g0, g1, g2, g3, x_noise, y_noise);
}

float GetShiftOffset(float v, ProcTexShift mode, ProcTexClamp clamp_mode) {
    const float offset = (clamp_mode == ProcTexClamp::MirroredRepeat) ? 1 : 0.5f;
    switch (mode) {
    case ProcTexShift::None:
        return 0;
    case ProcTexShift::Odd:
        return offset * (((int)v / 2) % 2);
    case ProcTexShift::Even:
        return offset * ((((int)v + 1) / 2) % 2);
    default:
        LOG_CRITICAL(HW_GPU, "Unknown shift mode {}", mode);
        return 0;
    }
};

void ClampCoord(float& coord, ProcTexClamp mode) {
    switch (mode) {
    case ProcTexClamp::ToZero:
        if (coord > 1.0f)
            coord = 0.0f;
        break;
    case ProcTexClamp::ToEdge:
        coord = std::min(coord, 1.0f);
        break;
    case ProcTexClamp::SymmetricalRepeat:
        coord = coord - std::floor(coord);
        break;
    case ProcTexClamp::MirroredRepeat: {
        int integer = static_cast<int>(coord);
        float frac = coord - integer;
        coord = (integer % 2) == 0 ? frac : (1.0f - frac);
        break;
    }
    case ProcTexClamp::Pulse:
        if (coord <= 0.5f)
            coord = 0.0f;
        else
            coord = 1.0f;
        break;
    default:
        LOG_CRITICAL(HW_GPU, "Unknown clamp mode {}", mode);
        coord = std::min(coord, 1.0f);
        break;
    }
}

float CombineAndMap(float u, float v, ProcTexCombiner combiner,
                    const std::array<Pica::PicaCore::ProcTex::ValueEntry, 128>& map_table) {
    float f;
    switch (combiner) {
    case ProcTexCombiner::U:
        f = u;
        break;
    case ProcTexCombiner::U2:
        f = u * u;
        break;
    case ProcTexCombiner::V:
        f = v;
        break;
    case ProcTexCombiner::V2:
        f = v * v;
        break;
    case ProcTexCombiner::Add:
        f = (u + v) * 0.5f;
        break;
    case ProcTexCombiner::Add2:
        f = (u * u + v * v) * 0.5f;
        break;
    case ProcTexCombiner::SqrtAdd2:
        f = std::min(std::sqrt(u * u + v * v), 1.0f);
        break;
    case ProcTexCombiner::Min:
        f = std::min(u, v);
        break;
    case ProcTexCombiner::Max:
        f = std::max(u, v);
        break;
    case ProcTexCombiner::RMax:
        f = std::min(((u + v) * 0.5f + std::sqrt(u * u + v * v)) * 0.5f, 1.0f);
        break;
    default:
        LOG_CRITICAL(HW_GPU, "Unknown combiner {}", combiner);
        f = 0.0f;
        break;
    }
    return LookupLUT(map_table, f);
}
} // Anonymous namespace

Common::Vec4<u8> ProcTex(float u, float v, const Pica::TexturingRegs& regs,
                         const Pica::PicaCore::ProcTex& state) {
    u = std::abs(u);
    v = std::abs(v);

    // Get shift offset before noise generation
    const float u_shift = GetShiftOffset(v, regs.proctex.u_shift, regs.proctex.u_clamp);
    const float v_shift = GetShiftOffset(u, regs.proctex.v_shift, regs.proctex.v_clamp);

    // Generate noise
    if (regs.proctex.noise_enable) {
        float noise = NoiseCoef(u, v, regs, state);
        u += noise * regs.proctex_noise_u.amplitude / 4095.0f;
        v += noise * regs.proctex_noise_v.amplitude / 4095.0f;
        u = std::abs(u);
        v = std::abs(v);
    }

    // Shift
    u += u_shift;
    v += v_shift;

    // Clamp
    ClampCoord(u, regs.proctex.u_clamp);
    ClampCoord(v, regs.proctex.v_clamp);

    // Combine and map
    const float lut_coord = CombineAndMap(u, v, regs.proctex.color_combiner, state.color_map_table);

    // Look up the color
    // For the color lut, coord=0.0 is lut[offset] and coord=1.0 is lut[offset+width-1]
    const u32 offset = regs.proctex_lut_offset.level0;
    const u32 width = regs.proctex_lut.width;
    const float index = offset + (lut_coord * (width - 1));
    Common::Vec4<u8> final_color;
    // TODO(wwylele): implement mipmap
    switch (regs.proctex_lut.filter) {
    case ProcTexFilter::Linear:
    case ProcTexFilter::LinearMipmapLinear:
    case ProcTexFilter::LinearMipmapNearest: {
        const int index_int = static_cast<int>(index);
        const float frac = index - index_int;
        const auto color_value = state.color_table[index_int].ToVector().Cast<float>();
        const auto color_diff = state.color_diff_table[index_int].ToVector().Cast<float>();
        final_color = (color_value + frac * color_diff).Cast<u8>();
        break;
    }
    case ProcTexFilter::Nearest:
    case ProcTexFilter::NearestMipmapLinear:
    case ProcTexFilter::NearestMipmapNearest:
        final_color = state.color_table[static_cast<int>(std::round(index))].ToVector();
        break;
    }

    if (regs.proctex.separate_alpha) {
        // Note: in separate alpha mode, the alpha channel skips the color LUT look up stage. It
        // uses the output of CombineAndMap directly instead.
        const float final_alpha =
            CombineAndMap(u, v, regs.proctex.alpha_combiner, state.alpha_map_table);
        return Common::MakeVec<u8>(final_color.rgb(), static_cast<u8>(final_alpha * 255));
    } else {
        return final_color;
    }
}

} // namespace SwRenderer
