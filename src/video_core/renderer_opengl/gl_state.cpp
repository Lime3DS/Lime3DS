// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/gl_vars.h"

namespace OpenGL {

OpenGLState OpenGLState::cur_state;

OpenGLState::OpenGLState() {
    // These all match default OpenGL values
    cull.enabled = false;
    cull.mode = GL_BACK;
    cull.front_face = GL_CCW;

    depth.test_enabled = false;
    depth.test_func = GL_LESS;
    depth.write_mask = GL_TRUE;

    color_mask.red_enabled = GL_TRUE;
    color_mask.green_enabled = GL_TRUE;
    color_mask.blue_enabled = GL_TRUE;
    color_mask.alpha_enabled = GL_TRUE;

    stencil.test_enabled = false;
    stencil.test_func = GL_ALWAYS;
    stencil.test_ref = 0;
    stencil.test_mask = 0xFF;
    stencil.write_mask = 0xFF;
    stencil.action_depth_fail = GL_KEEP;
    stencil.action_depth_pass = GL_KEEP;
    stencil.action_stencil_fail = GL_KEEP;

    blend.enabled = true;
    blend.rgb_equation = GL_FUNC_ADD;
    blend.a_equation = GL_FUNC_ADD;
    blend.src_rgb_func = GL_ONE;
    blend.dst_rgb_func = GL_ZERO;
    blend.src_a_func = GL_ONE;
    blend.dst_a_func = GL_ZERO;
    blend.color.red = 0.0f;
    blend.color.green = 0.0f;
    blend.color.blue = 0.0f;
    blend.color.alpha = 0.0f;

    logic_op = GL_COPY;

    for (auto& texture_unit : texture_units) {
        texture_unit.texture_2d = 0;
        texture_unit.target = GL_TEXTURE_2D;
        texture_unit.sampler = 0;
    }

    color_buffer.texture_2d = 0;

    texture_buffer_lut_lf.texture_buffer = 0;
    texture_buffer_lut_rg.texture_buffer = 0;
    texture_buffer_lut_rgba.texture_buffer = 0;

    image_shadow_buffer = 0;
    image_shadow_texture_px = 0;
    image_shadow_texture_nx = 0;
    image_shadow_texture_py = 0;
    image_shadow_texture_ny = 0;
    image_shadow_texture_pz = 0;
    image_shadow_texture_nz = 0;

    draw.read_framebuffer = 0;
    draw.draw_framebuffer = 0;
    draw.vertex_array = 0;
    draw.vertex_buffer = 0;
    draw.uniform_buffer = 0;
    draw.shader_program = 0;
    draw.program_pipeline = 0;

    scissor.enabled = false;
    scissor.x = 0;
    scissor.y = 0;
    scissor.width = 0;
    scissor.height = 0;

    viewport.x = 0;
    viewport.y = 0;
    viewport.width = 0;
    viewport.height = 0;

    clip_distance = {};

    renderbuffer = 0;
}

void OpenGLState::Apply() const {
    // Culling
    if (cull.enabled != cur_state.cull.enabled) {
        if (cull.enabled) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
    }

    if (cull.mode != cur_state.cull.mode) {
        glCullFace(cull.mode);
    }

    if (cull.front_face != cur_state.cull.front_face) {
        glFrontFace(cull.front_face);
    }

    // Depth test
    if (depth.test_enabled != cur_state.depth.test_enabled) {
        if (depth.test_enabled) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
    }

    if (depth.test_func != cur_state.depth.test_func) {
        glDepthFunc(depth.test_func);
    }

    // Depth mask
    if (depth.write_mask != cur_state.depth.write_mask) {
        glDepthMask(depth.write_mask);
    }

    // Color mask
    if (color_mask.red_enabled != cur_state.color_mask.red_enabled ||
        color_mask.green_enabled != cur_state.color_mask.green_enabled ||
        color_mask.blue_enabled != cur_state.color_mask.blue_enabled ||
        color_mask.alpha_enabled != cur_state.color_mask.alpha_enabled) {
        glColorMask(color_mask.red_enabled, color_mask.green_enabled, color_mask.blue_enabled,
                    color_mask.alpha_enabled);
    }

    // Stencil test
    if (stencil.test_enabled != cur_state.stencil.test_enabled) {
        if (stencil.test_enabled) {
            glEnable(GL_STENCIL_TEST);
        } else {
            glDisable(GL_STENCIL_TEST);
        }
    }

    if (stencil.test_func != cur_state.stencil.test_func ||
        stencil.test_ref != cur_state.stencil.test_ref ||
        stencil.test_mask != cur_state.stencil.test_mask) {
        glStencilFunc(stencil.test_func, stencil.test_ref, stencil.test_mask);
    }

    if (stencil.action_depth_fail != cur_state.stencil.action_depth_fail ||
        stencil.action_depth_pass != cur_state.stencil.action_depth_pass ||
        stencil.action_stencil_fail != cur_state.stencil.action_stencil_fail) {
        glStencilOp(stencil.action_stencil_fail, stencil.action_depth_fail,
                    stencil.action_depth_pass);
    }

    // Stencil mask
    if (stencil.write_mask != cur_state.stencil.write_mask) {
        glStencilMask(stencil.write_mask);
    }

    // Blending
    if (blend.enabled != cur_state.blend.enabled) {
        if (blend.enabled) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }

        // GLES does not support glLogicOp
        if (!GLES) {
            if (blend.enabled) {
                glDisable(GL_COLOR_LOGIC_OP);
            } else {
                glEnable(GL_COLOR_LOGIC_OP);
            }
        }
    }

    if (blend.color.red != cur_state.blend.color.red ||
        blend.color.green != cur_state.blend.color.green ||
        blend.color.blue != cur_state.blend.color.blue ||
        blend.color.alpha != cur_state.blend.color.alpha) {
        glBlendColor(blend.color.red, blend.color.green, blend.color.blue, blend.color.alpha);
    }

    if (blend.src_rgb_func != cur_state.blend.src_rgb_func ||
        blend.dst_rgb_func != cur_state.blend.dst_rgb_func ||
        blend.src_a_func != cur_state.blend.src_a_func ||
        blend.dst_a_func != cur_state.blend.dst_a_func) {
        glBlendFuncSeparate(blend.src_rgb_func, blend.dst_rgb_func, blend.src_a_func,
                            blend.dst_a_func);
    }

    if (blend.rgb_equation != cur_state.blend.rgb_equation ||
        blend.a_equation != cur_state.blend.a_equation) {
        glBlendEquationSeparate(blend.rgb_equation, blend.a_equation);
    }

    // GLES does not support glLogicOp
    if (!GLES) {
        if (logic_op != cur_state.logic_op) {
            glLogicOp(logic_op);
        }
    }

    // Textures
    for (u32 i = 0; i < texture_units.size(); ++i) {
        if (texture_units[i].texture_2d != cur_state.texture_units[i].texture_2d) {
            glActiveTexture(TextureUnits::PicaTexture(i).Enum());
            glBindTexture(texture_units[i].target, texture_units[i].texture_2d);
        }
        if (texture_units[i].sampler != cur_state.texture_units[i].sampler) {
            glBindSampler(i, texture_units[i].sampler);
        }
    }

    // Texture buffer LUTs
    if (texture_buffer_lut_lf.texture_buffer != cur_state.texture_buffer_lut_lf.texture_buffer) {
        glActiveTexture(TextureUnits::TextureBufferLUT_LF.Enum());
        glBindTexture(GL_TEXTURE_BUFFER, texture_buffer_lut_lf.texture_buffer);
    }

    // Texture buffer LUTs
    if (texture_buffer_lut_rg.texture_buffer != cur_state.texture_buffer_lut_rg.texture_buffer) {
        glActiveTexture(TextureUnits::TextureBufferLUT_RG.Enum());
        glBindTexture(GL_TEXTURE_BUFFER, texture_buffer_lut_rg.texture_buffer);
    }

    // Texture buffer LUTs
    if (texture_buffer_lut_rgba.texture_buffer !=
        cur_state.texture_buffer_lut_rgba.texture_buffer) {
        glActiveTexture(TextureUnits::TextureBufferLUT_RGBA.Enum());
        glBindTexture(GL_TEXTURE_BUFFER, texture_buffer_lut_rgba.texture_buffer);
    }

    // Color buffer
    if (color_buffer.texture_2d != cur_state.color_buffer.texture_2d) {
        glActiveTexture(TextureUnits::TextureColorBuffer.Enum());
        glBindTexture(GL_TEXTURE_2D, color_buffer.texture_2d);
    }

    // Shadow Images
    if (image_shadow_buffer != cur_state.image_shadow_buffer) {
        glBindImageTexture(ImageUnits::ShadowBuffer, image_shadow_buffer, 0, GL_FALSE, 0,
                           GL_READ_WRITE, GL_R32UI);
    }

    if (image_shadow_texture_px != cur_state.image_shadow_texture_px) {
        glBindImageTexture(ImageUnits::ShadowTexturePX, image_shadow_texture_px, 0, GL_FALSE, 0,
                           GL_READ_ONLY, GL_R32UI);
    }

    if (image_shadow_texture_nx != cur_state.image_shadow_texture_nx) {
        glBindImageTexture(ImageUnits::ShadowTextureNX, image_shadow_texture_nx, 0, GL_FALSE, 0,
                           GL_READ_ONLY, GL_R32UI);
    }

    if (image_shadow_texture_py != cur_state.image_shadow_texture_py) {
        glBindImageTexture(ImageUnits::ShadowTexturePY, image_shadow_texture_py, 0, GL_FALSE, 0,
                           GL_READ_ONLY, GL_R32UI);
    }

    if (image_shadow_texture_ny != cur_state.image_shadow_texture_ny) {
        glBindImageTexture(ImageUnits::ShadowTextureNY, image_shadow_texture_ny, 0, GL_FALSE, 0,
                           GL_READ_ONLY, GL_R32UI);
    }

    if (image_shadow_texture_pz != cur_state.image_shadow_texture_pz) {
        glBindImageTexture(ImageUnits::ShadowTexturePZ, image_shadow_texture_pz, 0, GL_FALSE, 0,
                           GL_READ_ONLY, GL_R32UI);
    }

    if (image_shadow_texture_nz != cur_state.image_shadow_texture_nz) {
        glBindImageTexture(ImageUnits::ShadowTextureNZ, image_shadow_texture_nz, 0, GL_FALSE, 0,
                           GL_READ_ONLY, GL_R32UI);
    }

    // Framebuffer
    if (draw.read_framebuffer != cur_state.draw.read_framebuffer) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, draw.read_framebuffer);
    }
    if (draw.draw_framebuffer != cur_state.draw.draw_framebuffer) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw.draw_framebuffer);
    }

    // Vertex array
    if (draw.vertex_array != cur_state.draw.vertex_array) {
        glBindVertexArray(draw.vertex_array);
    }

    // Vertex buffer
    if (draw.vertex_buffer != cur_state.draw.vertex_buffer) {
        glBindBuffer(GL_ARRAY_BUFFER, draw.vertex_buffer);
    }

    // Uniform buffer
    if (draw.uniform_buffer != cur_state.draw.uniform_buffer) {
        glBindBuffer(GL_UNIFORM_BUFFER, draw.uniform_buffer);
    }

    // Shader program
    if (draw.shader_program != cur_state.draw.shader_program) {
        glUseProgram(draw.shader_program);
    }

    // Program pipeline
    if (draw.program_pipeline != cur_state.draw.program_pipeline) {
        glBindProgramPipeline(draw.program_pipeline);
    }

    // Scissor test
    if (scissor.enabled != cur_state.scissor.enabled) {
        if (scissor.enabled) {
            glEnable(GL_SCISSOR_TEST);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }
    }

    if (scissor.x != cur_state.scissor.x || scissor.y != cur_state.scissor.y ||
        scissor.width != cur_state.scissor.width || scissor.height != cur_state.scissor.height) {
        glScissor(scissor.x, scissor.y, scissor.width, scissor.height);
    }

    if (viewport.x != cur_state.viewport.x || viewport.y != cur_state.viewport.y ||
        viewport.width != cur_state.viewport.width ||
        viewport.height != cur_state.viewport.height) {
        glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    }

    // Clip distance
    if (!GLES || GLAD_GL_EXT_clip_cull_distance) {
        for (std::size_t i = 0; i < clip_distance.size(); ++i) {
            if (clip_distance[i] != cur_state.clip_distance[i]) {
                if (clip_distance[i]) {
                    glEnable(GL_CLIP_DISTANCE0 + static_cast<GLenum>(i));
                } else {
                    glDisable(GL_CLIP_DISTANCE0 + static_cast<GLenum>(i));
                }
            }
        }
    }

    if (renderbuffer != cur_state.renderbuffer) {
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    }

    cur_state = *this;
}

OpenGLState& OpenGLState::ResetTexture(GLuint handle) {
    for (auto& unit : texture_units) {
        if (unit.texture_2d == handle) {
            unit.texture_2d = 0;
        }
    }
    if (texture_buffer_lut_lf.texture_buffer == handle)
        texture_buffer_lut_lf.texture_buffer = 0;
    if (texture_buffer_lut_rg.texture_buffer == handle)
        texture_buffer_lut_rg.texture_buffer = 0;
    if (texture_buffer_lut_rgba.texture_buffer == handle)
        texture_buffer_lut_rgba.texture_buffer = 0;
    if (image_shadow_buffer == handle)
        image_shadow_buffer = 0;
    if (image_shadow_texture_px == handle)
        image_shadow_texture_px = 0;
    if (image_shadow_texture_nx == handle)
        image_shadow_texture_nx = 0;
    if (image_shadow_texture_py == handle)
        image_shadow_texture_py = 0;
    if (image_shadow_texture_ny == handle)
        image_shadow_texture_ny = 0;
    if (image_shadow_texture_pz == handle)
        image_shadow_texture_pz = 0;
    if (image_shadow_texture_nz == handle)
        image_shadow_texture_nz = 0;
    return *this;
}

OpenGLState& OpenGLState::ResetSampler(GLuint handle) {
    for (auto& unit : texture_units) {
        if (unit.sampler == handle) {
            unit.sampler = 0;
        }
    }
    return *this;
}

OpenGLState& OpenGLState::ResetProgram(GLuint handle) {
    if (draw.shader_program == handle) {
        draw.shader_program = 0;
    }
    return *this;
}

OpenGLState& OpenGLState::ResetPipeline(GLuint handle) {
    if (draw.program_pipeline == handle) {
        draw.program_pipeline = 0;
    }
    return *this;
}

OpenGLState& OpenGLState::ResetBuffer(GLuint handle) {
    if (draw.vertex_buffer == handle) {
        draw.vertex_buffer = 0;
    }
    if (draw.uniform_buffer == handle) {
        draw.uniform_buffer = 0;
    }
    return *this;
}

OpenGLState& OpenGLState::ResetVertexArray(GLuint handle) {
    if (draw.vertex_array == handle) {
        draw.vertex_array = 0;
    }
    return *this;
}

OpenGLState& OpenGLState::ResetFramebuffer(GLuint handle) {
    if (draw.read_framebuffer == handle) {
        draw.read_framebuffer = 0;
    }
    if (draw.draw_framebuffer == handle) {
        draw.draw_framebuffer = 0;
    }
    return *this;
}

OpenGLState& OpenGLState::ResetRenderbuffer(GLuint handle) {
    if (renderbuffer == handle) {
        renderbuffer = 0;
    }
    return *this;
}

} // namespace OpenGL
