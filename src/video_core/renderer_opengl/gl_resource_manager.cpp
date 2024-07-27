// Copyright 2022 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/microprofile.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/gl_state.h"

MICROPROFILE_DEFINE(OpenGL_ResourceCreation, "OpenGL", "Resource Creation", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_ResourceDeletion, "OpenGL", "Resource Deletion", MP_RGB(128, 128, 192));

namespace OpenGL {

void OGLRenderbuffer::Create() {
    if (handle != 0) {
        return;
    }

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glGenRenderbuffers(1, &handle);
}

void OGLRenderbuffer::Release() {
    if (handle == 0) {
        return;
    }

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteRenderbuffers(1, &handle);
    OpenGLState::GetCurState().ResetRenderbuffer(handle).Apply();
    handle = 0;
}

void OGLTexture::Create() {
    if (handle != 0) {
        return;
    }

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glGenTextures(1, &handle);
}

void OGLTexture::Release() {
    if (handle == 0) {
        return;
    }

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteTextures(1, &handle);
    OpenGLState::GetCurState().ResetTexture(handle).Apply();
    handle = 0;
}

void OGLTexture::Allocate(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                          GLsizei height, GLsizei depth) {
    GLuint old_tex = OpenGLState::GetCurState().texture_units[0].texture_2d;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(target, handle);

    switch (target) {
    case GL_TEXTURE_1D:
    case GL_TEXTURE:
        glTexStorage1D(target, levels, internalformat, width);
        break;
    case GL_TEXTURE_2D:
    case GL_TEXTURE_1D_ARRAY:
    case GL_TEXTURE_RECTANGLE:
    case GL_TEXTURE_CUBE_MAP:
        glTexStorage2D(target, levels, internalformat, width, height);
        break;
    case GL_TEXTURE_3D:
    case GL_TEXTURE_2D_ARRAY:
    case GL_TEXTURE_CUBE_MAP_ARRAY:
        glTexStorage3D(target, levels, internalformat, width, height, depth);
        break;
    }

    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, old_tex);
}

void OGLSampler::Create() {
    if (handle != 0) {
        return;
    }

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glGenSamplers(1, &handle);
}

void OGLSampler::Release() {
    if (handle == 0) {
        return;
    }

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteSamplers(1, &handle);
    OpenGLState::GetCurState().ResetSampler(handle).Apply();
    handle = 0;
}

void OGLShader::Create(std::string_view source, GLenum type) {
    if (handle != 0)
        return;
    if (source.empty())
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    handle = LoadShader(source, type);
}

void OGLShader::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteShader(handle);
    handle = 0;
}

void OGLProgram::Create(bool separable_program, std::span<const GLuint> shaders) {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    handle = LoadProgram(separable_program, shaders);
}

void OGLProgram::Create(std::string_view vert_shader, std::string_view frag_shader) {
    OGLShader vert, frag;
    vert.Create(vert_shader, GL_VERTEX_SHADER);
    frag.Create(frag_shader, GL_FRAGMENT_SHADER);

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    const std::array shaders{vert.handle, frag.handle};
    Create(false, shaders);
}

void OGLProgram::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteProgram(handle);
    OpenGLState::GetCurState().ResetProgram(handle).Apply();
    handle = 0;
}

void OGLPipeline::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glGenProgramPipelines(1, &handle);
}

void OGLPipeline::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteProgramPipelines(1, &handle);
    OpenGLState::GetCurState().ResetPipeline(handle).Apply();
    handle = 0;
}

void OGLBuffer::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glGenBuffers(1, &handle);
}

void OGLBuffer::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteBuffers(1, &handle);
    OpenGLState::GetCurState().ResetBuffer(handle).Apply();
    handle = 0;
}

void OGLVertexArray::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glGenVertexArrays(1, &handle);
}

void OGLVertexArray::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteVertexArrays(1, &handle);
    OpenGLState::GetCurState().ResetVertexArray(handle).Apply();
    handle = 0;
}

void OGLFramebuffer::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glGenFramebuffers(1, &handle);
}

void OGLFramebuffer::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteFramebuffers(1, &handle);
    OpenGLState::GetCurState().ResetFramebuffer(handle).Apply();
    handle = 0;
}

} // namespace OpenGL
