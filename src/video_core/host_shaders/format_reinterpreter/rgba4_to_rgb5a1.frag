// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

//? #version 430 core

precision highp int;
precision highp float;

layout(location = 0) in mediump vec2 tex_coord;
layout(location = 0) out lowp vec4 frag_color;

layout(binding = 0) uniform lowp sampler2D source;

void main() {
    mediump vec2 coord = tex_coord * vec2(textureSize(source, 0));
    mediump ivec2 tex_icoord = ivec2(coord);
    lowp ivec4 rgba4 = ivec4(texelFetch(source, tex_icoord, 0) * (exp2(4.0) - 1.0));
    lowp ivec3 rgb5 =
        ((rgba4.rgb << ivec3(1, 2, 3)) | (rgba4.gba >> ivec3(3, 2, 1))) & 0x1F;
    frag_color = vec4(vec3(rgb5) / (exp2(5.0) - 1.0), rgba4.a & 0x01);
}
