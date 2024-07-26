// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

//? #version 430 core
precision mediump float;

layout(location = 0) in vec2 tex_coord;
layout(location = 0) out vec4 frag_color;

layout(binding = 0) uniform sampler2D tex;

#ifdef VULKAN
layout(push_constant, std140) uniform XbrzInfo {
    float scale;
};
#else
layout(location = 2) uniform float scale;
#endif

const int BLEND_NONE = 0;
const int BLEND_NORMAL = 1;
const int BLEND_DOMINANT = 2;
const float LUMINANCE_WEIGHT = 1.0;
const float EQUAL_COLOR_TOLERANCE = 30.0 / 255.0;
const float STEEP_DIRECTION_THRESHOLD = 2.2;
const float DOMINANT_DIRECTION_THRESHOLD = 3.6;

float ColorDist(vec4 a, vec4 b) {
    // https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.2020_conversion
    const vec3 K = vec3(0.2627, 0.6780, 0.0593);
    const mat3 MATRIX = mat3(K, -.5 * K.r / (1.0 - K.b), -.5 * K.g / (1.0 - K.b), .5, .5,
                             -.5 * K.g / (1.0 - K.r), -.5 * K.b / (1.0 - K.r));
    vec4 diff = a - b;
    vec3 YCbCr = diff.rgb * MATRIX;
    // LUMINANCE_WEIGHT is currently 1, otherwise y would be multiplied by it
    float d = length(YCbCr);
    return sqrt(a.a * b.a * d * d + diff.a * diff.a);
}

bool IsPixEqual(const vec4 pixA, const vec4 pixB) {
    return ColorDist(pixA, pixB) < EQUAL_COLOR_TOLERANCE;
}

float GetLeftRatio(vec2 center, vec2 origin, vec2 direction) {
    vec2 P0 = center - origin;
    vec2 proj = direction * (dot(P0, direction) / dot(direction, direction));
    vec2 distv = P0 - proj;
    vec2 orth = vec2(-direction.y, direction.x);
    float side = sign(dot(P0, orth));
    float v = side * length(distv * scale);
    return smoothstep(-sqrt(2.0) / 2.0, sqrt(2.0) / 2.0, v);
}

#define P(x, y) textureOffset(tex, coord, ivec2(x, y))

void main() {
    vec2 source_size = vec2(textureSize(tex, 0));
    vec2 pos = fract(tex_coord * source_size) - vec2(0.5, 0.5);
    vec2 coord = tex_coord - pos / source_size;

    //---------------------------------------
    // Input Pixel Mapping:  -|x|x|x|-
    //                       x|A|B|C|x
    //                       x|D|E|F|x
    //                       x|G|H|I|x
    //                       -|x|x|x|-
    vec4 A = P(-1, -1);
    vec4 B = P(0, -1);
    vec4 C = P(1, -1);
    vec4 D = P(-1, 0);
    vec4 E = P(0, 0);
    vec4 F = P(1, 0);
    vec4 G = P(-1, 1);
    vec4 H = P(0, 1);
    vec4 I = P(1, 1);
    // blendResult Mapping: x|y|
    //                      w|z|
    ivec4 blendResult = ivec4(BLEND_NONE, BLEND_NONE, BLEND_NONE, BLEND_NONE);
    // Preprocess corners
    // Pixel Tap Mapping: -|-|-|-|-
    //                    -|-|B|C|-
    //                    -|D|E|F|x
    //                    -|G|H|I|x
    //                    -|-|x|x|-
    if (!((E == F && H == I) || (E == H && F == I))) {
        float dist_H_F = ColorDist(G, E) + ColorDist(E, C) + ColorDist(P(0, 2), I) +
                         ColorDist(I, P(2, 0)) + (4.0 * ColorDist(H, F));
        float dist_E_I = ColorDist(D, H) + ColorDist(H, P(1, 2)) + ColorDist(B, F) +
                         ColorDist(F, P(2, 1)) + (4.0 * ColorDist(E, I));
        bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_H_F) < dist_E_I;
        blendResult.z = ((dist_H_F < dist_E_I) && E != F && E != H)
                            ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL)
                            : BLEND_NONE;
    }
    // Pixel Tap Mapping: -|-|-|-|-
    //                    -|A|B|-|-
    //                    x|D|E|F|-
    //                    x|G|H|I|-
    //                    -|x|x|-|-
    if (!((D == E && G == H) || (D == G && E == H))) {
        float dist_G_E = ColorDist(P(-2, 1), D) + ColorDist(D, B) + ColorDist(P(-1, 2), H) +
                         ColorDist(H, F) + (4.0 * ColorDist(G, E));
        float dist_D_H = ColorDist(P(-2, 0), G) + ColorDist(G, P(0, 2)) + ColorDist(A, E) +
                         ColorDist(E, I) + (4.0 * ColorDist(D, H));
        bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_D_H) < dist_G_E;
        blendResult.w = ((dist_G_E > dist_D_H) && E != D && E != H)
                            ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL)
                            : BLEND_NONE;
    }
    // Pixel Tap Mapping: -|-|x|x|-
    //                    -|A|B|C|x
    //                    -|D|E|F|x
    //                    -|-|H|I|-
    //                    -|-|-|-|-
    if (!((B == C && E == F) || (B == E && C == F))) {
        float dist_E_C = ColorDist(D, B) + ColorDist(B, P(1, -2)) + ColorDist(H, F) +
                         ColorDist(F, P(2, -1)) + (4.0 * ColorDist(E, C));
        float dist_B_F = ColorDist(A, E) + ColorDist(E, I) + ColorDist(P(0, -2), C) +
                         ColorDist(C, P(2, 0)) + (4.0 * ColorDist(B, F));
        bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_B_F) < dist_E_C;
        blendResult.y = ((dist_E_C > dist_B_F) && E != B && E != F)
                            ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL)
                            : BLEND_NONE;
    }
    // Pixel Tap Mapping: -|x|x|-|-
    //                    x|A|B|C|-
    //                    x|D|E|F|-
    //                    -|G|H|-|-
    //                    -|-|-|-|-
    if (!((A == B && D == E) || (A == D && B == E))) {
        float dist_D_B = ColorDist(P(-2, 0), A) + ColorDist(A, P(0, -2)) + ColorDist(G, E) +
                         ColorDist(E, C) + (4.0 * ColorDist(D, B));
        float dist_A_E = ColorDist(P(-2, -1), D) + ColorDist(D, H) + ColorDist(P(-1, -2), B) +
                         ColorDist(B, F) + (4.0 * ColorDist(A, E));
        bool dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_D_B) < dist_A_E;
        blendResult.x = ((dist_D_B < dist_A_E) && E != D && E != B)
                            ? ((dominantGradient) ? BLEND_DOMINANT : BLEND_NORMAL)
                            : BLEND_NONE;
    }
    vec4 res = E;
    // Pixel Tap Mapping: -|-|-|-|-
    //                    -|-|B|C|-
    //                    -|D|E|F|x
    //                    -|G|H|I|x
    //                    -|-|x|x|-
    if (blendResult.z != BLEND_NONE) {
        float dist_F_G = ColorDist(F, G);
        float dist_H_C = ColorDist(H, C);
        bool doLineBlend = (blendResult.z == BLEND_DOMINANT ||
                            !((blendResult.y != BLEND_NONE && !IsPixEqual(E, G)) ||
                              (blendResult.w != BLEND_NONE && !IsPixEqual(E, C)) ||
                              (IsPixEqual(G, H) && IsPixEqual(H, I) && IsPixEqual(I, F) &&
                               IsPixEqual(F, C) && !IsPixEqual(E, I))));
        vec2 origin = vec2(0.0, 1.0 / sqrt(2.0));
        vec2 direction = vec2(1.0, -1.0);
        if (doLineBlend) {
            bool haveShallowLine =
                (STEEP_DIRECTION_THRESHOLD * dist_F_G <= dist_H_C) && E != G && D != G;
            bool haveSteepLine =
                (STEEP_DIRECTION_THRESHOLD * dist_H_C <= dist_F_G) && E != C && B != C;
            origin = haveShallowLine ? vec2(0.0, 0.25) : vec2(0.0, 0.5);
            direction.x += haveShallowLine ? 1.0 : 0.0;
            direction.y -= haveSteepLine ? 1.0 : 0.0;
        }
        vec4 blendPix = mix(H, F, step(ColorDist(E, F), ColorDist(E, H)));
        res = mix(res, blendPix, GetLeftRatio(pos, origin, direction));
    }
    // Pixel Tap Mapping: -|-|-|-|-
    //                    -|A|B|-|-
    //                    x|D|E|F|-
    //                    x|G|H|I|-
    //                    -|x|x|-|-
    if (blendResult.w != BLEND_NONE) {
        float dist_H_A = ColorDist(H, A);
        float dist_D_I = ColorDist(D, I);
        bool doLineBlend = (blendResult.w == BLEND_DOMINANT ||
                            !((blendResult.z != BLEND_NONE && !IsPixEqual(E, A)) ||
                              (blendResult.x != BLEND_NONE && !IsPixEqual(E, I)) ||
                              (IsPixEqual(A, D) && IsPixEqual(D, G) && IsPixEqual(G, H) &&
                               IsPixEqual(H, I) && !IsPixEqual(E, G))));
        vec2 origin = vec2(-1.0 / sqrt(2.0), 0.0);
        vec2 direction = vec2(1.0, 1.0);
        if (doLineBlend) {
            bool haveShallowLine =
                (STEEP_DIRECTION_THRESHOLD * dist_H_A <= dist_D_I) && E != A && B != A;
            bool haveSteepLine =
                (STEEP_DIRECTION_THRESHOLD * dist_D_I <= dist_H_A) && E != I && F != I;
            origin = haveShallowLine ? vec2(-0.25, 0.0) : vec2(-0.5, 0.0);
            direction.y += haveShallowLine ? 1.0 : 0.0;
            direction.x += haveSteepLine ? 1.0 : 0.0;
        }
        origin = origin;
        direction = direction;
        vec4 blendPix = mix(H, D, step(ColorDist(E, D), ColorDist(E, H)));
        res = mix(res, blendPix, GetLeftRatio(pos, origin, direction));
    }
    // Pixel Tap Mapping: -|-|x|x|-
    //                    -|A|B|C|x
    //                    -|D|E|F|x
    //                    -|-|H|I|-
    //                    -|-|-|-|-
    if (blendResult.y != BLEND_NONE) {
        float dist_B_I = ColorDist(B, I);
        float dist_F_A = ColorDist(F, A);
        bool doLineBlend = (blendResult.y == BLEND_DOMINANT ||
                            !((blendResult.x != BLEND_NONE && !IsPixEqual(E, I)) ||
                              (blendResult.z != BLEND_NONE && !IsPixEqual(E, A)) ||
                              (IsPixEqual(I, F) && IsPixEqual(F, C) && IsPixEqual(C, B) &&
                               IsPixEqual(B, A) && !IsPixEqual(E, C))));
        vec2 origin = vec2(1.0 / sqrt(2.0), 0.0);
        vec2 direction = vec2(-1.0, -1.0);
        if (doLineBlend) {
            bool haveShallowLine =
                (STEEP_DIRECTION_THRESHOLD * dist_B_I <= dist_F_A) && E != I && H != I;
            bool haveSteepLine =
                (STEEP_DIRECTION_THRESHOLD * dist_F_A <= dist_B_I) && E != A && D != A;
            origin = haveShallowLine ? vec2(0.25, 0.0) : vec2(0.5, 0.0);
            direction.y -= haveShallowLine ? 1.0 : 0.0;
            direction.x -= haveSteepLine ? 1.0 : 0.0;
        }
        vec4 blendPix = mix(F, B, step(ColorDist(E, B), ColorDist(E, F)));
        res = mix(res, blendPix, GetLeftRatio(pos, origin, direction));
    }
    // Pixel Tap Mapping: -|x|x|-|-
    //                    x|A|B|C|-
    //                    x|D|E|F|-
    //                    -|G|H|-|-
    //                    -|-|-|-|-
    if (blendResult.x != BLEND_NONE) {
        float dist_D_C = ColorDist(D, C);
        float dist_B_G = ColorDist(B, G);
        bool doLineBlend = (blendResult.x == BLEND_DOMINANT ||
                            !((blendResult.w != BLEND_NONE && !IsPixEqual(E, C)) ||
                              (blendResult.y != BLEND_NONE && !IsPixEqual(E, G)) ||
                              (IsPixEqual(C, B) && IsPixEqual(B, A) && IsPixEqual(A, D) &&
                               IsPixEqual(D, G) && !IsPixEqual(E, A))));
        vec2 origin = vec2(0.0, -1.0 / sqrt(2.0));
        vec2 direction = vec2(-1.0, 1.0);
        if (doLineBlend) {
            bool haveShallowLine =
                (STEEP_DIRECTION_THRESHOLD * dist_D_C <= dist_B_G) && E != C && F != C;
            bool haveSteepLine =
                (STEEP_DIRECTION_THRESHOLD * dist_B_G <= dist_D_C) && E != G && H != G;
            origin = haveShallowLine ? vec2(0.0, -0.25) : vec2(0.0, -0.5);
            direction.x -= haveShallowLine ? 1.0 : 0.0;
            direction.y += haveSteepLine ? 1.0 : 0.0;
        }
        vec4 blendPix = mix(D, B, step(ColorDist(E, B), ColorDist(E, D)));
        res = mix(res, blendPix, GetLeftRatio(pos, origin, direction));
    }
    frag_color = res;
}
