// Copyright 2022 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#version 450 core
#extension GL_ARB_shader_stencil_export : require

layout(binding = 0) uniform sampler2D depth_tex;
layout(binding = 1) uniform usampler2D stencil_tex;

layout(location = 0) in vec2 texcoord;

void main() {
    gl_FragDepth = textureLod(depth_tex, texcoord, 0).r;
    gl_FragStencilRefARB = int(textureLod(stencil_tex, texcoord, 0).r);
}
