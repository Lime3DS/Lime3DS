// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <json.hpp>
#include "common/detached_tasks.h"
#include "common/web_result.h"
#include "web_service/telemetry_json.h"
#include "web_service/web_backend.h"

namespace WebService {

namespace Telemetry = Common::Telemetry;

struct TelemetryJson::Impl {
    Impl(std::string host, std::string username, std::string token)
        : host{std::move(host)}, username{std::move(username)}, token{std::move(token)} {}

    nlohmann::json& TopSection() {
        return sections[static_cast<u8>(Telemetry::FieldType::None)];
    }

    const nlohmann::json& TopSection() const {
        return sections[static_cast<u8>(Telemetry::FieldType::None)];
    }

    template <class T>
    void Serialize(Telemetry::FieldType type, const std::string& name, T value) {
        sections[static_cast<u8>(type)][name] = value;
    }

    void SerializeSection(Telemetry::FieldType type, const std::string& name) {
        TopSection()[name] = sections[static_cast<unsigned>(type)];
    }

    nlohmann::json output;
    std::array<nlohmann::json, 7> sections;
    std::string host;
    std::string username;
    std::string token;
};

TelemetryJson::TelemetryJson(std::string host, std::string username, std::string token)
    : impl{std::make_unique<Impl>(std::move(host), std::move(username), std::move(token))} {}
TelemetryJson::~TelemetryJson() = default;

void TelemetryJson::Visit(const Telemetry::Field<bool>& field) {
    impl->Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<double>& field) {
    impl->Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<float>& field) {
    impl->Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<u8>& field) {
    impl->Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<u16>& field) {
    impl->Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<u32>& field) {
    impl->Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<u64>& field) {
    impl->Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<s8>& field) {
    impl->Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<s16>& field) {
    impl->Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<s32>& field) {
    impl->Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<s64>& field) {
    impl->Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<std::string>& field) {
    impl->Serialize(field.GetType(), field.GetName(), field.GetValue());
}

void TelemetryJson::Visit(const Telemetry::Field<const char*>& field) {
    impl->Serialize(field.GetType(), field.GetName(), std::string(field.GetValue()));
}

void TelemetryJson::Visit(const Telemetry::Field<std::chrono::microseconds>& field) {
    impl->Serialize(field.GetType(), field.GetName(), field.GetValue().count());
}

void TelemetryJson::Complete() {
    impl->SerializeSection(Telemetry::FieldType::App, "App");
    impl->SerializeSection(Telemetry::FieldType::Session, "Session");
    impl->SerializeSection(Telemetry::FieldType::Performance, "Performance");
    impl->SerializeSection(Telemetry::FieldType::UserConfig, "UserConfig");
    impl->SerializeSection(Telemetry::FieldType::UserSystem, "UserSystem");

    auto content = impl->TopSection().dump();
    // Send the telemetry async but don't handle the errors since they were written to the log
    Common::DetachedTasks::AddTask([host{impl->host}, content]() {
        Client{host, "", ""}.PostJson("/telemetry", content, true);
    });
}

bool TelemetryJson::SubmitTestcase() {
    impl->SerializeSection(Telemetry::FieldType::App, "App");
    impl->SerializeSection(Telemetry::FieldType::Session, "Session");
    impl->SerializeSection(Telemetry::FieldType::UserFeedback, "UserFeedback");
    impl->SerializeSection(Telemetry::FieldType::UserSystem, "UserSystem");

    auto content = impl->TopSection().dump();
    Client client(impl->host, impl->username, impl->token);
    auto value = client.PostJson("/gamedb/testcase", content, false);

    return value.result_code == Common::WebResult::Code::Success;
}

} // namespace WebService
