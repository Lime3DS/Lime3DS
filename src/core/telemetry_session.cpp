// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cryptopp/osrng.h>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/loader/loader.h"
#include "core/telemetry_session.h"
#include "network/network_settings.h"

#ifdef ENABLE_WEB_SERVICE
#include "web_service/telemetry_json.h"
#include "web_service/verify_login.h"
#endif

namespace Core {

namespace Telemetry = Common::Telemetry;

static u64 GenerateTelemetryId() {
    u64 telemetry_id{};
    CryptoPP::AutoSeededRandomPool rng;
    rng.GenerateBlock(reinterpret_cast<CryptoPP::byte*>(&telemetry_id), sizeof(u64));
    return telemetry_id;
}

u64 GetTelemetryId() {
    u64 telemetry_id{};
    const std::string filename{FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) +
                               "telemetry_id"};

    if (FileUtil::Exists(filename)) {
        FileUtil::IOFile file(filename, "rb");
        if (!file.IsOpen()) {
            LOG_ERROR(Core, "failed to open telemetry_id: {}", filename);
            return {};
        }
        file.ReadBytes(&telemetry_id, sizeof(u64));
    } else {
        FileUtil::IOFile file(filename, "wb");
        if (!file.IsOpen()) {
            LOG_ERROR(Core, "failed to open telemetry_id: {}", filename);
            return {};
        }
        telemetry_id = GenerateTelemetryId();
        file.WriteBytes(&telemetry_id, sizeof(u64));
    }

    return telemetry_id;
}

u64 RegenerateTelemetryId() {
    const u64 new_telemetry_id{GenerateTelemetryId()};
    const std::string filename{FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) +
                               "telemetry_id"};

    FileUtil::IOFile file(filename, "wb");
    if (!file.IsOpen()) {
        LOG_ERROR(Core, "failed to open telemetry_id: {}", filename);
        return {};
    }
    file.WriteBytes(&new_telemetry_id, sizeof(u64));
    return new_telemetry_id;
}

bool VerifyLogin(const std::string& username, const std::string& token) {
#ifdef ENABLE_WEB_SERVICE
    return WebService::VerifyLogin(NetSettings::values.web_api_url, username, token);
#else
    return false;
#endif
}

TelemetrySession::TelemetrySession() = default;

TelemetrySession::~TelemetrySession() {
    // Log one-time session end information
    const s64 shutdown_time{std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count()};
    AddField(Telemetry::FieldType::Session, "Shutdown_Time", shutdown_time);

#ifdef ENABLE_WEB_SERVICE
    auto backend = std::make_unique<WebService::TelemetryJson>(NetSettings::values.web_api_url,
                                                               NetSettings::values.citra_username,
                                                               NetSettings::values.citra_token);
#else
    auto backend = std::make_unique<Telemetry::NullVisitor>();
#endif

    // Complete the session, submitting to the web service backend if necessary
    field_collection.Accept(*backend);
    if (NetSettings::values.enable_telemetry) {
        backend->Complete();
    }
}

void TelemetrySession::AddInitialInfo(Loader::AppLoader& app_loader) {
    // Log one-time top-level information
    AddField(Telemetry::FieldType::None, "TelemetryId", GetTelemetryId());

    // Log one-time session start information
    const s64 init_time{std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count()};
    AddField(Telemetry::FieldType::Session, "Init_Time", init_time);
    std::string program_name;
    const Loader::ResultStatus res{app_loader.ReadTitle(program_name)};
    if (res == Loader::ResultStatus::Success) {
        AddField(Telemetry::FieldType::Session, "ProgramName", program_name);
    }

    // Log application information
    Telemetry::AppendBuildInfo(field_collection);

    // Log user system information
    Telemetry::AppendCPUInfo(field_collection);
    Telemetry::AppendOSInfo(field_collection);

    // Log user configuration information
    AddField(Telemetry::FieldType::UserConfig, "Audio_SinkId",
             static_cast<int>(Settings::values.output_type.GetValue()));
    AddField(Telemetry::FieldType::UserConfig, "Audio_EnableAudioStretching",
             Settings::values.enable_audio_stretching.GetValue());
    AddField(Telemetry::FieldType::UserConfig, "Core_UseCpuJit",
             Settings::values.use_cpu_jit.GetValue());
    AddField(Telemetry::FieldType::UserConfig, "Renderer_ResolutionFactor",
             Settings::values.resolution_factor.GetValue());
    AddField(Telemetry::FieldType::UserConfig, "Renderer_FrameLimit",
             Settings::values.frame_limit.GetValue());
    AddField(Telemetry::FieldType::UserConfig, "Renderer_Backend",
             static_cast<int>(Settings::values.graphics_api.GetValue()));
    AddField(Telemetry::FieldType::UserConfig, "Renderer_UseHwShader",
             Settings::values.use_hw_shader.GetValue());
    AddField(Telemetry::FieldType::UserConfig, "Renderer_ShadersAccurateMul",
             Settings::values.shaders_accurate_mul.GetValue());
    AddField(Telemetry::FieldType::UserConfig, "Renderer_UseShaderJit",
             Settings::values.use_shader_jit.GetValue());
    AddField(Telemetry::FieldType::UserConfig, "Renderer_UseVsync",
             Settings::values.use_vsync_new.GetValue());
    AddField(Telemetry::FieldType::UserConfig, "Renderer_FilterMode",
             Settings::values.filter_mode.GetValue());
    AddField(Telemetry::FieldType::UserConfig, "Renderer_Render3d",
             static_cast<int>(Settings::values.render_3d.GetValue()));
    AddField(Telemetry::FieldType::UserConfig, "Renderer_Factor3d",
             Settings::values.factor_3d.GetValue());
    AddField(Telemetry::FieldType::UserConfig, "Renderer_MonoRenderOption",
             static_cast<int>(Settings::values.mono_render_option.GetValue()));
    AddField(Telemetry::FieldType::UserConfig, "System_IsNew3ds",
             Settings::values.is_new_3ds.GetValue());
    AddField(Telemetry::FieldType::UserConfig, "System_LLEApplets",
             Settings::values.lle_applets.GetValue());
    AddField(Telemetry::FieldType::UserConfig, "System_RegionValue",
             Settings::values.region_value.GetValue());
}

bool TelemetrySession::SubmitTestcase() {
#ifdef ENABLE_WEB_SERVICE
    auto backend = std::make_unique<WebService::TelemetryJson>(NetSettings::values.web_api_url,
                                                               NetSettings::values.citra_username,
                                                               NetSettings::values.citra_token);
    field_collection.Accept(*backend);
    return backend->SubmitTestcase();
#else
    return false;
#endif
}

} // namespace Core
