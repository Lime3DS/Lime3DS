// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <glad/glad.h>
#include "common/assert.h"
#include "common/settings.h"
#include "core/telemetry_session.h"
#include "video_core/custom_textures/custom_format.h"
#include "video_core/renderer_opengl/gl_driver.h"
#include "video_core/renderer_opengl/gl_vars.h"

namespace OpenGL {

DECLARE_ENUM_FLAG_OPERATORS(DriverBug);

inline std::string_view GetSource(GLenum source) {
#define RET(s)                                                                                     \
    case GL_DEBUG_SOURCE_##s:                                                                      \
        return #s
    switch (source) {
        RET(API);
        RET(WINDOW_SYSTEM);
        RET(SHADER_COMPILER);
        RET(THIRD_PARTY);
        RET(APPLICATION);
        RET(OTHER);
    default:
        UNREACHABLE();
    }
#undef RET

    return std::string_view{};
}

inline std::string_view GetType(GLenum type) {
#define RET(t)                                                                                     \
    case GL_DEBUG_TYPE_##t:                                                                        \
        return #t
    switch (type) {
        RET(ERROR);
        RET(DEPRECATED_BEHAVIOR);
        RET(UNDEFINED_BEHAVIOR);
        RET(PORTABILITY);
        RET(PERFORMANCE);
        RET(OTHER);
        RET(MARKER);
        RET(POP_GROUP);
        RET(PUSH_GROUP);
    default:
        UNREACHABLE();
    }
#undef RET

    return std::string_view{};
}

static void APIENTRY DebugHandler(GLenum source, GLenum type, GLuint id, GLenum severity,
                                  GLsizei length, const GLchar* message, const void* user_param) {
    auto level = Common::Log::Level::Info;
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        level = Common::Log::Level::Critical;
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        level = Common::Log::Level::Warning;
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
    case GL_DEBUG_SEVERITY_LOW:
        level = Common::Log::Level::Debug;
        break;
    }

    LOG_GENERIC(Common::Log::Class::Render_OpenGL, level, "{} {} {}: {}", GetSource(source),
                GetType(type), id, message);
}

Driver::Driver(Core::TelemetrySession& telemetry_session_) : telemetry_session{telemetry_session_} {
    const bool enable_debug = Settings::values.renderer_debug.GetValue();
    if (enable_debug) {
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(DebugHandler, nullptr);
    }

    ReportDriverInfo();
    DeduceGLES();
    DeduceVendor();
    CheckExtensionSupport();
    FindBugs();
}

Driver::~Driver() = default;

bool Driver::HasBug(DriverBug bug) const {
    return True(bugs & bug);
}

bool Driver::HasDebugTool() {
    GLint num_extensions;
    glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
    for (GLuint index = 0; index < static_cast<GLuint>(num_extensions); ++index) {
        const auto name = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, index));
        if (!std::strcmp(name, "GL_EXT_debug_tool")) {
            return true;
        }
    }
    return false;
}

bool Driver::IsCustomFormatSupported(VideoCore::CustomPixelFormat format) const {
    switch (format) {
    case VideoCore::CustomPixelFormat::RGBA8:
        return true;
    case VideoCore::CustomPixelFormat::BC1:
    case VideoCore::CustomPixelFormat::BC3:
    case VideoCore::CustomPixelFormat::BC5:
        return ext_texture_compression_s3tc;
    case VideoCore::CustomPixelFormat::BC7:
        return arb_texture_compression_bptc;
    case VideoCore::CustomPixelFormat::ASTC4:
    case VideoCore::CustomPixelFormat::ASTC6:
    case VideoCore::CustomPixelFormat::ASTC8:
        return is_gles;
    default:
        return false;
    }
}

void Driver::ReportDriverInfo() {
    // Report the context version and the vendor string
    gl_version = std::string_view{reinterpret_cast<const char*>(glGetString(GL_VERSION))};
    gpu_vendor = std::string_view{reinterpret_cast<const char*>(glGetString(GL_VENDOR))};
    gpu_model = std::string_view{reinterpret_cast<const char*>(glGetString(GL_RENDERER))};

    LOG_INFO(Render_OpenGL, "GL_VERSION: {}", gl_version);
    LOG_INFO(Render_OpenGL, "GL_VENDOR: {}", gpu_vendor);
    LOG_INFO(Render_OpenGL, "GL_RENDERER: {}", gpu_model);

    // Add the information to the telemetry system
    constexpr auto user_system = Common::Telemetry::FieldType::UserSystem;
    telemetry_session.AddField(user_system, "GPU_Vendor", std::string{gpu_vendor});
    telemetry_session.AddField(user_system, "GPU_Model", std::string{gpu_model});
    telemetry_session.AddField(user_system, "GPU_OpenGL_Version", std::string{gl_version});
}

void Driver::DeduceGLES() {
    // According to the spec, all GLES version strings must start with "OpenGL ES".
    is_gles = gl_version.starts_with("OpenGL ES");

    // TODO: Eliminate this global state and replace with driver references.
    OpenGL::GLES = is_gles;
}

void Driver::DeduceVendor() {
    if (gpu_vendor.find("NVIDIA") != gpu_vendor.npos) {
        vendor = Vendor::Nvidia;
    } else if ((gpu_vendor.find("ATI") != gpu_vendor.npos) ||
               (gpu_vendor.find("AMD") != gpu_vendor.npos) ||
               (gpu_vendor.find("Advanced Micro Devices") != gpu_vendor.npos)) {
        vendor = Vendor::AMD;
    } else if (gpu_vendor.find("Intel") != gpu_vendor.npos) {
        vendor = Vendor::Intel;
    } else if (gpu_vendor.find("ARM") != gpu_vendor.npos) {
        vendor = Vendor::ARM;
    } else if (gpu_vendor.find("Qualcomm") != gpu_vendor.npos) {
        vendor = Vendor::Qualcomm;
    } else if (gpu_vendor.find("Samsung") != gpu_vendor.npos) {
        vendor = Vendor::Samsung;
    } else if (gpu_vendor.find("GDI Generic") != gpu_vendor.npos) {
        vendor = Vendor::Generic;
    }
}

void Driver::CheckExtensionSupport() {
    ext_buffer_storage = GLAD_GL_EXT_buffer_storage;
    arb_buffer_storage = GLAD_GL_ARB_buffer_storage;
    arb_clear_texture = GLAD_GL_ARB_clear_texture;
    arb_get_texture_sub_image = GLAD_GL_ARB_get_texture_sub_image;
    arb_texture_compression_bptc = GLAD_GL_ARB_texture_compression_bptc;
    clip_cull_distance = !is_gles || GLAD_GL_EXT_clip_cull_distance;
    ext_texture_compression_s3tc = GLAD_GL_EXT_texture_compression_s3tc;
    ext_shader_framebuffer_fetch = GLAD_GL_EXT_shader_framebuffer_fetch;
    arm_shader_framebuffer_fetch = GLAD_GL_ARM_shader_framebuffer_fetch;
    arb_fragment_shader_interlock = GLAD_GL_ARB_fragment_shader_interlock;
    nv_fragment_shader_interlock = GLAD_GL_NV_fragment_shader_interlock;
    intel_fragment_shader_ordering = GLAD_GL_INTEL_fragment_shader_ordering;
    blend_minmax_factor = GLAD_GL_AMD_blend_minmax_factor || GLAD_GL_NV_blend_minmax_factor;
    is_suitable = GLAD_GL_VERSION_4_3 || GLAD_GL_ES_VERSION_3_1;
}

void Driver::FindBugs() {
#ifdef __unix__
    const bool is_linux = true;
#else
    const bool is_linux = false;
#endif

    // TODO: Check if these have been fixed in the newer driver
    if (vendor == Vendor::AMD) {
        bugs |= DriverBug::ShaderStageChangeFreeze | DriverBug::VertexArrayOutOfBound;
    }

    if (vendor == Vendor::AMD || (vendor == Vendor::Intel && !is_linux)) {
        bugs |= DriverBug::BrokenTextureView;
    }

    if (vendor == Vendor::Intel && !is_linux) {
        bugs |= DriverBug::BrokenClearTexture;
    }
}

} // namespace OpenGL
