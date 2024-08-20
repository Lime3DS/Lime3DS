// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace OpenGL {

// Returns a vector of the names of the shaders available in the
// "shaders" directory in lime3ds's data directory
std::vector<std::string> GetPostProcessingShaderList(bool anaglyph);

// Returns the shader code for the shader named "shader_name"
// with the appropriate header prepended to it
// If anaglyph is true, it searches the shaders/anaglyph directory rather than
// the shaders directory
// If the shader cannot be loaded, an empty string is returned
std::string GetPostProcessingShaderCode(bool anaglyph, std::string_view shader_name);

} // namespace OpenGL
