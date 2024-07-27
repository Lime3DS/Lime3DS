// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>
#include <glad/glad.h>

namespace OpenGL {

/**
 * Utility function to create and compile an OpenGL GLSL shader
 * @param source String of the GLSL shader program
 * @param type Type of the shader (GL_VERTEX_SHADER, GL_GEOMETRY_SHADER or GL_FRAGMENT_SHADER)
 */
GLuint LoadShader(std::string_view source, GLenum type);

/**
 * Utility function to create and link an OpenGL GLSL shader program
 * @param separable_program whether to create a separable program
 * @param shaders ID of shaders to attach to the program
 * @returns Handle of the newly created OpenGL program object
 */
GLuint LoadProgram(bool separable_program, std::span<const GLuint> shaders);

} // namespace OpenGL
