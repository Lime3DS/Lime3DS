// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Pica::Shader {

struct Profile {
    bool has_separable_shaders{};
    bool has_clip_planes{};
    bool has_geometry_shader{};
    bool has_custom_border_color{};
    bool has_fragment_shader_interlock{};
    bool has_fragment_shader_barycentric{};
    bool has_blend_minmax_factor{};
    bool has_minus_one_to_one_range{};
    bool has_logic_op{};
    bool has_gl_ext_framebuffer_fetch{};
    bool has_gl_arm_framebuffer_fetch{};
    bool has_gl_nv_fragment_shader_interlock{};
    bool has_gl_intel_fragment_shader_ordering{};
    bool has_gl_nv_fragment_shader_barycentric{};
    bool is_vulkan{};
};

} // namespace Pica::Shader
