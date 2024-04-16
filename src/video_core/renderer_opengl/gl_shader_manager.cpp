// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <mutex>
#include <set>
#include <span>
#include <thread>
#include <unordered_map>
#include <variant>
#include "common/settings.h"
#include "core/frontend/emu_window.h"
#include "video_core/pica/shader_setup.h"
#include "video_core/renderer_opengl/gl_driver.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_disk_cache.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/shader/generator/glsl_fs_shader_gen.h"
#include "video_core/shader/generator/glsl_shader_gen.h"
#include "video_core/shader/generator/profile.h"

using namespace Pica::Shader::Generator;
using Pica::Shader::FSConfig;

namespace OpenGL {

static u64 GetUniqueIdentifier(const Pica::RegsInternal& regs, const ProgramCode& code) {
    std::size_t hash = 0;
    u64 regs_uid =
        Common::ComputeHash64(regs.reg_array.data(), Pica::RegsInternal::NUM_REGS * sizeof(u32));
    hash = Common::HashCombine(hash, regs_uid);

    if (code.size() > 0) {
        u64 code_uid = Common::ComputeHash64(code.data(), code.size() * sizeof(u32));
        hash = Common::HashCombine(hash, code_uid);
    }

    return hash;
}

static OGLProgram GeneratePrecompiledProgram(const ShaderDiskCacheDump& dump,
                                             const std::set<GLenum>& supported_formats,
                                             bool separable) {

    if (supported_formats.find(dump.binary_format) == supported_formats.end()) {
        LOG_INFO(Render_OpenGL, "Precompiled cache entry with unsupported format - removing");
        return {};
    }

    auto shader = OGLProgram();
    shader.handle = glCreateProgram();
    if (separable) {
        glProgramParameteri(shader.handle, GL_PROGRAM_SEPARABLE, GL_TRUE);
    }
    glProgramBinary(shader.handle, dump.binary_format, dump.binary.data(),
                    static_cast<GLsizei>(dump.binary.size()));

    GLint link_status{};
    glGetProgramiv(shader.handle, GL_LINK_STATUS, &link_status);
    if (link_status == GL_FALSE) {
        LOG_INFO(Render_OpenGL, "Precompiled cache rejected by the driver - removing");
        return {};
    }

    return shader;
}

static std::set<GLenum> GetSupportedFormats() {
    std::set<GLenum> supported_formats;

    GLint num_formats{};
    glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &num_formats);

    std::vector<GLint> formats(num_formats);
    glGetIntegerv(GL_PROGRAM_BINARY_FORMATS, formats.data());

    for (const GLint format : formats)
        supported_formats.insert(static_cast<GLenum>(format));
    return supported_formats;
}

static std::tuple<PicaVSConfig, Pica::ShaderSetup> BuildVSConfigFromRaw(
    const ShaderDiskCacheRaw& raw, const Driver& driver) {
    Pica::ProgramCode program_code{};
    Pica::SwizzleData swizzle_data{};
    std::copy_n(raw.GetProgramCode().begin(), Pica::MAX_PROGRAM_CODE_LENGTH, program_code.begin());
    std::copy_n(raw.GetProgramCode().begin() + Pica::MAX_PROGRAM_CODE_LENGTH,
                Pica::MAX_SWIZZLE_DATA_LENGTH, swizzle_data.begin());
    Pica::ShaderSetup setup;
    setup.program_code = program_code;
    setup.swizzle_data = swizzle_data;

    // Enable the geometry-shader only if we are actually doing per-fragment lighting
    // and care about proper quaternions. Otherwise just use standard vertex+fragment shaders
    const bool use_geometry_shader = !raw.GetRawShaderConfig().lighting.disable;
    return {PicaVSConfig{raw.GetRawShaderConfig(), setup, driver.HasClipCullDistance(),
                         use_geometry_shader},
            setup};
}

/**
 * An object representing a shader program staging. It can be either a shader object or a program
 * object, depending on whether separable program is used.
 */
class OGLShaderStage {
public:
    explicit OGLShaderStage(bool separable) {
        if (separable) {
            shader_or_program = OGLProgram();
        } else {
            shader_or_program = OGLShader();
        }
    }

    void Create(const char* source, GLenum type) {
        if (shader_or_program.index() == 0) {
            std::get<OGLShader>(shader_or_program).Create(source, type);
        } else {
            OGLShader shader;
            shader.Create(source, type);
            OGLProgram& program = std::get<OGLProgram>(shader_or_program);
            program.Create(true, std::array{shader.handle});
        }
    }

    GLuint GetHandle() const {
        if (shader_or_program.index() == 0) {
            return std::get<OGLShader>(shader_or_program).handle;
        } else {
            return std::get<OGLProgram>(shader_or_program).handle;
        }
    }

    void Inject(OGLProgram&& program) {
        shader_or_program = std::move(program);
    }

private:
    std::variant<OGLShader, OGLProgram> shader_or_program;
};

class TrivialVertexShader {
public:
    explicit TrivialVertexShader(const Driver& driver, bool separable) : program(separable) {
        const auto code =
            GLSL::GenerateTrivialVertexShader(driver.HasClipCullDistance(), separable);
        program.Create(code.c_str(), GL_VERTEX_SHADER);
    }
    GLuint Get() const {
        return program.GetHandle();
    }

private:
    OGLShaderStage program;
};

template <typename KeyConfigType, auto CodeGenerator, GLenum ShaderType>
class ShaderCache {
public:
    explicit ShaderCache(bool separable_) : separable{separable_} {}
    ~ShaderCache() = default;

    template <typename... Args>
    std::tuple<GLuint, std::optional<std::string>> Get(const KeyConfigType& config,
                                                       Args&&... args) {
        auto [iter, new_shader] = shaders.emplace(config, OGLShaderStage{separable});
        OGLShaderStage& cached_shader = iter->second;
        std::optional<std::string> result{};
        if (new_shader) {
            result = CodeGenerator(config, args...);
            cached_shader.Create(result->c_str(), ShaderType);
        }
        return {cached_shader.GetHandle(), std::move(result)};
    }

    void Inject(const KeyConfigType& key, OGLProgram&& program) {
        OGLShaderStage stage{separable};
        stage.Inject(std::move(program));
        shaders.emplace(key, std::move(stage));
    }

    void Inject(const KeyConfigType& key, OGLShaderStage&& stage) {
        shaders.emplace(key, std::move(stage));
    }

private:
    bool separable;
    std::unordered_map<KeyConfigType, OGLShaderStage> shaders;
};

// This is a cache designed for shaders translated from PICA shaders. The first cache matches the
// config structure like a normal cache does. On cache miss, the second cache matches the generated
// GLSL code. The configuration is like this because there might be leftover code in the PICA shader
// program buffer from the previous shader, which is hashed into the config, resulting several
// different config values from the same shader program.
template <typename KeyConfigType,
          std::string (*CodeGenerator)(const Pica::ShaderSetup&, const KeyConfigType&, bool),
          GLenum ShaderType>
class ShaderDoubleCache {
public:
    explicit ShaderDoubleCache(bool separable) : separable(separable) {}
    std::tuple<GLuint, std::optional<std::string>> Get(const KeyConfigType& key,
                                                       const Pica::ShaderSetup& setup) {
        std::optional<std::string> result{};
        auto map_it = shader_map.find(key);
        if (map_it == shader_map.end()) {
            auto program = CodeGenerator(setup, key, separable);
            if (program.empty()) {
                shader_map[key] = nullptr;
                return {0, std::nullopt};
            }

            auto [iter, new_shader] = shader_cache.emplace(program, OGLShaderStage{separable});
            OGLShaderStage& cached_shader = iter->second;
            if (new_shader) {
                result = program;
                cached_shader.Create(program.c_str(), ShaderType);
            }
            shader_map[key] = &cached_shader;
            return {cached_shader.GetHandle(), std::move(result)};
        }

        if (map_it->second == nullptr) {
            return {0, std::nullopt};
        }

        return {map_it->second->GetHandle(), std::nullopt};
    }

    void Inject(const KeyConfigType& key, std::string decomp, OGLProgram&& program) {
        OGLShaderStage stage{separable};
        stage.Inject(std::move(program));
        const auto iter = shader_cache.emplace(std::move(decomp), std::move(stage)).first;
        OGLShaderStage& cached_shader = iter->second;
        shader_map.insert_or_assign(key, &cached_shader);
    }

    void Inject(const KeyConfigType& key, std::string decomp, OGLShaderStage&& stage) {
        const auto iter = shader_cache.emplace(std::move(decomp), std::move(stage)).first;
        OGLShaderStage& cached_shader = iter->second;
        shader_map.insert_or_assign(key, &cached_shader);
    }

private:
    bool separable;
    std::unordered_map<KeyConfigType, OGLShaderStage*> shader_map;
    std::unordered_map<std::string, OGLShaderStage> shader_cache;
};

using ProgrammableVertexShaders =
    ShaderDoubleCache<PicaVSConfig, &GLSL::GenerateVertexShader, GL_VERTEX_SHADER>;

using FixedGeometryShaders =
    ShaderCache<PicaFixedGSConfig, &GLSL::GenerateFixedGeometryShader, GL_GEOMETRY_SHADER>;

using FragmentShaders = ShaderCache<FSConfig, &GLSL::GenerateFragmentShader, GL_FRAGMENT_SHADER>;

class ShaderProgramManager::Impl {
public:
    explicit Impl(const Driver& driver, bool separable)
        : separable(separable), programmable_vertex_shaders(separable),
          trivial_vertex_shader(driver, separable), fixed_geometry_shaders(separable),
          fragment_shaders(separable), disk_cache(separable) {
        if (separable) {
            pipeline.Create();
        }
        profile = Pica::Shader::Profile{
            .has_separable_shaders = separable,
            .has_clip_planes = driver.HasClipCullDistance(),
            .has_geometry_shader = true,
            .has_custom_border_color = true,
            .has_fragment_shader_interlock = driver.HasArbFragmentShaderInterlock(),
            // TODO: This extension requires GLSL 450 / OpenGL 4.5 context.
            .has_fragment_shader_barycentric = false,
            .has_blend_minmax_factor = driver.HasBlendMinMaxFactor(),
            .has_minus_one_to_one_range = true,
            .has_logic_op = !driver.IsOpenGLES(),
            .has_gl_ext_framebuffer_fetch = driver.HasExtFramebufferFetch(),
            .has_gl_arm_framebuffer_fetch = driver.HasArmShaderFramebufferFetch(),
            .has_gl_nv_fragment_shader_interlock = driver.HasNvFragmentShaderInterlock(),
            .has_gl_intel_fragment_shader_ordering = driver.HasIntelFragmentShaderOrdering(),
            // TODO: This extension requires GLSL 450 / OpenGL 4.5 context.
            .has_gl_nv_fragment_shader_barycentric = false,
            .is_vulkan = false,
        };
    }

    struct ShaderTuple {
        std::size_t vs_hash = 0;
        std::size_t gs_hash = 0;
        std::size_t fs_hash = 0;

        GLuint vs = 0;
        GLuint gs = 0;
        GLuint fs = 0;

        bool operator==(const ShaderTuple& rhs) const {
            return std::tie(vs, gs, fs) == std::tie(rhs.vs, rhs.gs, rhs.fs);
        }

        bool operator!=(const ShaderTuple& rhs) const {
            return std::tie(vs, gs, fs) != std::tie(rhs.vs, rhs.gs, rhs.fs);
        }

        std::size_t GetConfigHash() const {
            return Common::ComputeHash64(this, sizeof(std::size_t) * 3);
        }
    };

    static_assert(offsetof(ShaderTuple, vs_hash) == 0, "ShaderTuple layout changed!");
    static_assert(offsetof(ShaderTuple, fs_hash) == sizeof(std::size_t) * 2,
                  "ShaderTuple layout changed!");

    bool separable;
    Pica::Shader::Profile profile{};
    ShaderTuple current;

    ProgrammableVertexShaders programmable_vertex_shaders;
    TrivialVertexShader trivial_vertex_shader;

    FixedGeometryShaders fixed_geometry_shaders;

    FragmentShaders fragment_shaders;
    std::unordered_map<u64, OGLProgram> program_cache;
    OGLPipeline pipeline;
    ShaderDiskCache disk_cache;
};

ShaderProgramManager::ShaderProgramManager(Frontend::EmuWindow& emu_window_, const Driver& driver_,
                                           bool separable)
    : emu_window{emu_window_}, driver{driver_},
      strict_context_required{emu_window.StrictContextRequired()},
      impl{std::make_unique<Impl>(driver_, separable)} {}

ShaderProgramManager::~ShaderProgramManager() = default;

bool ShaderProgramManager::UseProgrammableVertexShader(const Pica::RegsInternal& regs,
                                                       Pica::ShaderSetup& setup) {
    // Enable the geometry-shader only if we are actually doing per-fragment lighting
    // and care about proper quaternions. Otherwise just use standard vertex+fragment shaders
    const bool use_geometry_shader = !regs.lighting.disable;

    PicaVSConfig config{regs, setup, driver.HasClipCullDistance(), use_geometry_shader};
    auto [handle, result] = impl->programmable_vertex_shaders.Get(config, setup);
    if (handle == 0)
        return false;
    impl->current.vs = handle;
    impl->current.vs_hash = config.Hash();

    // Save VS to the disk cache if its a new shader
    if (result) {
        auto& disk_cache = impl->disk_cache;
        ProgramCode program_code{setup.program_code.begin(), setup.program_code.end()};
        program_code.insert(program_code.end(), setup.swizzle_data.begin(),
                            setup.swizzle_data.end());
        const u64 unique_identifier = GetUniqueIdentifier(regs, program_code);
        const ShaderDiskCacheRaw raw{unique_identifier, ProgramType::VS, regs,
                                     std::move(program_code)};
        const bool sanitize_mul = Settings::values.shaders_accurate_mul.GetValue();
        disk_cache.SaveRaw(raw);
        disk_cache.SaveDecompiled(unique_identifier, *result, sanitize_mul);
    }
    return true;
}

void ShaderProgramManager::UseTrivialVertexShader() {
    impl->current.vs = impl->trivial_vertex_shader.Get();
    impl->current.vs_hash = 0;
}

void ShaderProgramManager::UseFixedGeometryShader(const Pica::RegsInternal& regs) {
    PicaFixedGSConfig gs_config(regs, driver.HasClipCullDistance());
    auto [handle, _] = impl->fixed_geometry_shaders.Get(gs_config, impl->separable);
    impl->current.gs = handle;
    impl->current.gs_hash = gs_config.Hash();
}

void ShaderProgramManager::UseTrivialGeometryShader() {
    impl->current.gs = 0;
    impl->current.gs_hash = 0;
}

void ShaderProgramManager::UseFragmentShader(const Pica::RegsInternal& regs,
                                             const Pica::Shader::UserConfig& user) {
    const FSConfig fs_config{regs, user, impl->profile};
    auto [handle, result] = impl->fragment_shaders.Get(fs_config, impl->profile);
    impl->current.fs = handle;
    impl->current.fs_hash = fs_config.Hash();
    // Save FS to the disk cache if its a new shader
    if (result) {
        auto& disk_cache = impl->disk_cache;
        u64 unique_identifier = GetUniqueIdentifier(regs, {});
        ShaderDiskCacheRaw raw{unique_identifier, ProgramType::FS, regs, {}};
        disk_cache.SaveRaw(raw);
        disk_cache.SaveDecompiled(unique_identifier, *result, false);
    }
}

void ShaderProgramManager::ApplyTo(OpenGLState& state) {
    if (impl->separable) {
        if (driver.HasBug(DriverBug::ShaderStageChangeFreeze)) {
            glUseProgramStages(
                impl->pipeline.handle,
                GL_VERTEX_SHADER_BIT | GL_GEOMETRY_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, 0);
        }

        glUseProgramStages(impl->pipeline.handle, GL_VERTEX_SHADER_BIT, impl->current.vs);
        glUseProgramStages(impl->pipeline.handle, GL_GEOMETRY_SHADER_BIT, impl->current.gs);
        glUseProgramStages(impl->pipeline.handle, GL_FRAGMENT_SHADER_BIT, impl->current.fs);
        state.draw.shader_program = 0;
        state.draw.program_pipeline = impl->pipeline.handle;
    } else {
        const u64 unique_identifier = impl->current.GetConfigHash();
        OGLProgram& cached_program = impl->program_cache[unique_identifier];
        if (cached_program.handle == 0) {
            cached_program.Create(false,
                                  std::array{impl->current.vs, impl->current.gs, impl->current.fs});
            auto& disk_cache = impl->disk_cache;
            const bool sanitize_mul = Settings::values.shaders_accurate_mul.GetValue();
            disk_cache.SaveDumpToFile(unique_identifier, cached_program.handle, sanitize_mul);
        }
        state.draw.shader_program = cached_program.handle;
    }
}

void ShaderProgramManager::LoadDiskCache(const std::atomic_bool& stop_loading,
                                         const VideoCore::DiskResourceLoadCallback& callback) {
    auto& disk_cache = impl->disk_cache;
    const auto transferable = disk_cache.LoadTransferable();
    if (!transferable) {
        return;
    }
    const auto& raws = *transferable;

    // Load uncompressed precompiled file for non-separable shaders.
    // Precompiled file for separable shaders is compressed.
    auto [decompiled, dumps] = disk_cache.LoadPrecompiled(impl->separable);

    if (stop_loading) {
        return;
    }

    std::set<GLenum> supported_formats = GetSupportedFormats();

    // Track if precompiled cache was altered during loading to know if we have to serialize the
    // virtual precompiled cache file back to the hard drive
    bool precompiled_cache_altered = false;

    std::mutex mutex;
    std::atomic_bool compilation_failed = false;
    if (callback) {
        callback(VideoCore::LoadCallbackStage::Decompile, 0, raws.size());
    }
    std::vector<std::size_t> load_raws_index;
    // Loads both decompiled and precompiled shaders from the cache. If either one is missing for
    const auto LoadPrecompiledShader = [&](std::size_t begin, std::size_t end,
                                           std::span<const ShaderDiskCacheRaw> raw_cache,
                                           const ShaderDecompiledMap& decompiled_map,
                                           const ShaderDumpsMap& dump_map) {
        for (std::size_t i = begin; i < end; ++i) {
            if (stop_loading || compilation_failed) {
                return;
            }
            const auto& raw{raw_cache[i]};
            const u64 unique_identifier{raw.GetUniqueIdentifier()};

            const u64 calculated_hash =
                GetUniqueIdentifier(raw.GetRawShaderConfig(), raw.GetProgramCode());
            if (unique_identifier != calculated_hash) {
                LOG_ERROR(Render_OpenGL,
                          "Invalid hash in entry={:016x} (obtained hash={:016x}) - removing "
                          "shader cache",
                          raw.GetUniqueIdentifier(), calculated_hash);
                disk_cache.InvalidateAll();
                return;
            }

            const auto dump{dump_map.find(unique_identifier)};
            const auto decomp{decompiled_map.find(unique_identifier)};

            OGLProgram shader;

            if (dump != dump_map.end() && decomp != decompiled_map.end()) {
                // Only load the vertex shader if its sanitize_mul setting matches
                const bool sanitize_mul = Settings::values.shaders_accurate_mul.GetValue();
                if (raw.GetProgramType() == ProgramType::VS &&
                    decomp->second.sanitize_mul != sanitize_mul) {
                    continue;
                }

                // If the shader is dumped, attempt to load it
                shader =
                    GeneratePrecompiledProgram(dump->second, supported_formats, impl->separable);
                if (shader.handle == 0) {
                    // If any shader failed, stop trying to compile, delete the cache, and start
                    // loading from raws
                    compilation_failed = true;
                    return;
                }
                // we have both the binary shader and the decompiled, so inject it into the
                // cache
                if (raw.GetProgramType() == ProgramType::VS) {
                    auto [conf, setup] = BuildVSConfigFromRaw(raw, driver);
                    std::scoped_lock lock(mutex);
                    impl->programmable_vertex_shaders.Inject(conf, decomp->second.code,
                                                             std::move(shader));
                } else if (raw.GetProgramType() == ProgramType::FS) {
                    // TODO: Support UserConfig in disk shader cache
                    const FSConfig conf(raw.GetRawShaderConfig(), {}, impl->profile);
                    std::scoped_lock lock(mutex);
                    impl->fragment_shaders.Inject(conf, std::move(shader));
                } else {
                    // Unsupported shader type got stored somehow so nuke the cache

                    LOG_CRITICAL(Frontend, "failed to load raw ProgramType {}",
                                 raw.GetProgramType());
                    compilation_failed = true;
                    return;
                }
            } else {
                // Since precompiled didn't have the dump, we'll load them in the next phase
                std::scoped_lock lock(mutex);
                load_raws_index.push_back(i);
            }
            if (callback) {
                callback(VideoCore::LoadCallbackStage::Decompile, i, raw_cache.size());
            }
        }
    };

    const auto LoadPrecompiledProgram = [&](const ShaderDecompiledMap& decompiled_map,
                                            const ShaderDumpsMap& dump_map) {
        std::size_t i{0};
        for (const auto& dump : dump_map) {
            if (stop_loading) {
                break;
            }
            const u64 unique_identifier{dump.first};
            const auto decomp{decompiled_map.find(unique_identifier)};

            // Only load the program if its sanitize_mul setting matches
            const bool sanitize_mul = Settings::values.shaders_accurate_mul.GetValue();
            if (decomp->second.sanitize_mul != sanitize_mul) {
                continue;
            }

            // If the shader program is dumped, attempt to load it
            OGLProgram shader =
                GeneratePrecompiledProgram(dump.second, supported_formats, impl->separable);
            if (shader.handle != 0) {
                impl->program_cache.emplace(unique_identifier, std::move(shader));
            } else {
                LOG_ERROR(Frontend, "Failed to link Precompiled program!");
                compilation_failed = true;
                break;
            }
            if (callback) {
                callback(VideoCore::LoadCallbackStage::Decompile, ++i, dump_map.size());
            }
        }
    };

    if (impl->separable) {
        LoadPrecompiledShader(0, raws.size(), raws, decompiled, dumps);
    } else {
        LoadPrecompiledProgram(decompiled, dumps);
    }

    bool load_all_raws = false;
    if (compilation_failed) {
        // Invalidate the precompiled cache if a shader dumped shader was rejected
        impl->program_cache.clear();
        disk_cache.InvalidatePrecompiled();
        dumps.clear();
        precompiled_cache_altered = true;
        load_all_raws = true;
    }
    // TODO(SachinV): Skip loading raws until we implement a proper way to link non-seperable
    // shaders.
    if (!impl->separable) {
        return;
    }

    const std::size_t load_raws_size = load_all_raws ? raws.size() : load_raws_index.size();

    if (callback) {
        callback(VideoCore::LoadCallbackStage::Build, 0, load_raws_size);
    }

    compilation_failed = false;

    std::size_t built_shaders = 0; // It doesn't have be atomic since it's used behind a mutex
    const auto LoadRawSepareble = [&](std::size_t begin, std::size_t end,
                                      Frontend::GraphicsContext* context = nullptr) {
        const auto scope = context->Acquire();
        for (std::size_t i = begin; i < end; ++i) {
            if (stop_loading || compilation_failed) {
                return;
            }

            const std::size_t raws_index = load_all_raws ? i : load_raws_index[i];
            const auto& raw{raws[raws_index]};
            const u64 unique_identifier{raw.GetUniqueIdentifier()};

            bool sanitize_mul = false;
            GLuint handle{0};
            std::string code;
            // Otherwise decompile and build the shader at boot and save the result to the
            // precompiled file
            if (raw.GetProgramType() == ProgramType::VS) {
                auto [conf, setup] = BuildVSConfigFromRaw(raw, driver);
                code = GLSL::GenerateVertexShader(setup, conf, impl->separable);
                OGLShaderStage stage{impl->separable};
                stage.Create(code.c_str(), GL_VERTEX_SHADER);
                handle = stage.GetHandle();
                sanitize_mul = conf.state.sanitize_mul;
                std::scoped_lock lock(mutex);
                impl->programmable_vertex_shaders.Inject(conf, code, std::move(stage));
            } else if (raw.GetProgramType() == ProgramType::FS) {
                // TODO: Support UserConfig in disk shader cache
                const FSConfig fs_config{raw.GetRawShaderConfig(), {}, impl->profile};
                code = GLSL::GenerateFragmentShader(fs_config, impl->profile);
                OGLShaderStage stage{impl->separable};
                stage.Create(code.c_str(), GL_FRAGMENT_SHADER);
                handle = stage.GetHandle();
                std::scoped_lock lock(mutex);
                impl->fragment_shaders.Inject(fs_config, std::move(stage));
            } else {
                // Unsupported shader type got stored somehow so nuke the cache
                LOG_ERROR(Frontend, "failed to load raw ProgramType {}", raw.GetProgramType());
                compilation_failed = true;
                return;
            }
            if (handle == 0) {
                LOG_ERROR(Frontend, "compilation from raw failed {:x} {:x}",
                          raw.GetProgramCode().at(0), raw.GetProgramCode().at(1));
                compilation_failed = true;
                return;
            }

            std::scoped_lock lock(mutex);
            // If this is a new separable shader, add it the precompiled cache
            if (!code.empty()) {
                disk_cache.SaveDecompiled(unique_identifier, code, sanitize_mul);
                disk_cache.SaveDump(unique_identifier, handle);
                precompiled_cache_altered = true;
            }

            if (callback) {
                callback(VideoCore::LoadCallbackStage::Build, ++built_shaders, load_raws_size);
            }
        }
    };

    if (!strict_context_required) {
        const std::size_t num_workers{std::max(1U, std::thread::hardware_concurrency())};
        const std::size_t bucket_size{load_raws_size / num_workers};
        std::vector<std::unique_ptr<Frontend::GraphicsContext>> contexts(num_workers);
        std::vector<std::thread> threads(num_workers);

        emu_window.SaveContext();
        for (std::size_t i = 0; i < num_workers; ++i) {
            const bool is_last_worker = i + 1 == num_workers;
            const std::size_t start{bucket_size * i};
            const std::size_t end{is_last_worker ? load_raws_size : start + bucket_size};

            // On some platforms the shared context has to be created from the GUI thread
            contexts[i] = emu_window.CreateSharedContext();
            // Release the context, so it can be immediately used by the spawned thread
            contexts[i]->DoneCurrent();
            threads[i] = std::thread(LoadRawSepareble, start, end, contexts[i].get());
        }
        for (auto& thread : threads) {
            thread.join();
        }
        emu_window.RestoreContext();
    } else {
        const auto dummy_context{std::make_unique<Frontend::GraphicsContext>()};
        LoadRawSepareble(0, load_raws_size, dummy_context.get());
    }

    if (compilation_failed) {
        disk_cache.InvalidateAll();
    }

    if (precompiled_cache_altered) {
        disk_cache.SaveVirtualPrecompiledFile();
    }
}

} // namespace OpenGL
