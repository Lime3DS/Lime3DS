// Copyright 2022 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>
#include <utility>
#include <vector>
#include <glad/glad.h>
#include "common/common_types.h"

namespace OpenGL {

class OGLRenderbuffer : private NonCopyable {
public:
    OGLRenderbuffer() = default;

    OGLRenderbuffer(OGLRenderbuffer&& o) noexcept : handle(std::exchange(o.handle, 0)) {}

    ~OGLRenderbuffer() {
        Release();
    }

    OGLRenderbuffer& operator=(OGLRenderbuffer&& o) noexcept {
        Release();
        handle = std::exchange(o.handle, 0);
        return *this;
    }

    /// Creates a new internal OpenGL resource and stores the handle
    void Create();

    /// Deletes the internal OpenGL resource
    void Release();

    GLuint handle = 0;
};

class OGLTexture : private NonCopyable {
public:
    OGLTexture() = default;

    OGLTexture(OGLTexture&& o) noexcept : handle(std::exchange(o.handle, 0)) {}

    ~OGLTexture() {
        Release();
    }

    OGLTexture& operator=(OGLTexture&& o) noexcept {
        Release();
        handle = std::exchange(o.handle, 0);
        return *this;
    }

    /// Creates a new internal OpenGL resource and stores the handle
    void Create();

    /// Deletes the internal OpenGL resource
    void Release();

    void Allocate(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                  GLsizei height = 1, GLsizei depth = 1);

    GLuint handle = 0;
};

class OGLSampler : private NonCopyable {
public:
    OGLSampler() = default;

    OGLSampler(OGLSampler&& o) noexcept : handle(std::exchange(o.handle, 0)) {}

    ~OGLSampler() {
        Release();
    }

    OGLSampler& operator=(OGLSampler&& o) noexcept {
        Release();
        handle = std::exchange(o.handle, 0);
        return *this;
    }

    /// Creates a new internal OpenGL resource and stores the handle
    void Create();

    /// Deletes the internal OpenGL resource
    void Release();

    GLuint handle = 0;
};

class OGLShader : private NonCopyable {
public:
    OGLShader() = default;

    OGLShader(OGLShader&& o) noexcept : handle(std::exchange(o.handle, 0)) {}

    ~OGLShader() {
        Release();
    }

    OGLShader& operator=(OGLShader&& o) noexcept {
        Release();
        handle = std::exchange(o.handle, 0);
        return *this;
    }

    void Create(std::string_view source, GLenum type);

    void Release();

    GLuint handle = 0;
};

class OGLProgram : private NonCopyable {
public:
    OGLProgram() = default;

    OGLProgram(OGLProgram&& o) noexcept : handle(std::exchange(o.handle, 0)) {}

    ~OGLProgram() {
        Release();
    }

    OGLProgram& operator=(OGLProgram&& o) noexcept {
        Release();
        handle = std::exchange(o.handle, 0);
        return *this;
    }

    /// Creates a new program from given shader objects
    void Create(bool separable_program, std::span<const GLuint> shaders);

    /// Creates a new program from given shader soruce code
    void Create(std::string_view vert_shader, std::string_view frag_shader);

    /// Deletes the internal OpenGL resource
    void Release();

    GLuint handle = 0;
};

class OGLPipeline : private NonCopyable {
public:
    OGLPipeline() = default;
    OGLPipeline(OGLPipeline&& o) noexcept {
        handle = std::exchange<GLuint>(o.handle, 0);
    }
    ~OGLPipeline() {
        Release();
    }
    OGLPipeline& operator=(OGLPipeline&& o) noexcept {
        Release();
        handle = std::exchange<GLuint>(o.handle, 0);
        return *this;
    }

    /// Creates a new internal OpenGL resource and stores the handle
    void Create();

    /// Deletes the internal OpenGL resource
    void Release();

    GLuint handle = 0;
};

class OGLBuffer : private NonCopyable {
public:
    OGLBuffer() = default;

    OGLBuffer(OGLBuffer&& o) noexcept : handle(std::exchange(o.handle, 0)) {}

    ~OGLBuffer() {
        Release();
    }

    OGLBuffer& operator=(OGLBuffer&& o) noexcept {
        Release();
        handle = std::exchange(o.handle, 0);
        return *this;
    }

    /// Creates a new internal OpenGL resource and stores the handle
    void Create();

    /// Deletes the internal OpenGL resource
    void Release();

    GLuint handle = 0;
};

class OGLVertexArray : private NonCopyable {
public:
    OGLVertexArray() = default;

    OGLVertexArray(OGLVertexArray&& o) noexcept : handle(std::exchange(o.handle, 0)) {}

    ~OGLVertexArray() {
        Release();
    }

    OGLVertexArray& operator=(OGLVertexArray&& o) noexcept {
        Release();
        handle = std::exchange(o.handle, 0);
        return *this;
    }

    /// Creates a new internal OpenGL resource and stores the handle
    void Create();

    /// Deletes the internal OpenGL resource
    void Release();

    GLuint handle = 0;
};

class OGLFramebuffer : private NonCopyable {
public:
    OGLFramebuffer() = default;

    OGLFramebuffer(OGLFramebuffer&& o) noexcept : handle(std::exchange(o.handle, 0)) {}

    ~OGLFramebuffer() {
        Release();
    }

    OGLFramebuffer& operator=(OGLFramebuffer&& o) noexcept {
        Release();
        handle = std::exchange(o.handle, 0);
        return *this;
    }

    /// Creates a new internal OpenGL resource and stores the handle
    void Create();

    /// Deletes the internal OpenGL resource
    void Release();

    GLuint handle = 0;
};

} // namespace OpenGL
