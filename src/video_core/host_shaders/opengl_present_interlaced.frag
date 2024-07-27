// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

//? #version 430 core

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 color;

layout(binding = 0) uniform sampler2D color_texture;
layout(binding = 1) uniform sampler2D color_texture_r;

uniform vec4 o_resolution;
uniform int reverse_interlaced;

void main() {
    float screen_row = o_resolution.x * frag_tex_coord.x;
    if (int(screen_row) % 2 == reverse_interlaced)
        color = texture(color_texture, frag_tex_coord);
    else
        color = texture(color_texture_r, frag_tex_coord);
}
