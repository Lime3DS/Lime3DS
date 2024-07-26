// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

//? #version 430 core

precision highp int;
precision highp float;

layout(location = 0) in mediump vec2 tex_coord;
layout(location = 0) out lowp vec4 frag_color;

layout(binding = 0) uniform highp sampler2D depth;
layout(binding = 1) uniform lowp usampler2D stencil;

void main() {
    mediump vec2 coord = tex_coord * vec2(textureSize(depth, 0));
    mediump ivec2 tex_icoord = ivec2(coord);
    highp uint depth_val =
        uint(texelFetch(depth, tex_icoord, 0).x * (exp2(32.0) - 1.0));
    lowp uint stencil_val = texelFetch(stencil, tex_icoord, 0).x;
    highp uvec4 components =
        uvec4(stencil_val, (uvec3(depth_val) >> uvec3(24u, 16u, 8u)) & 0x000000FFu);
    frag_color = vec4(components) / (exp2(8.0) - 1.0);
}
