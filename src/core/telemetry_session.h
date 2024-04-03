// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "common/telemetry.h"

namespace Loader {
class AppLoader;
}

namespace Core {

/**
 * Instruments telemetry for this emulation session. Creates a new set of telemetry fields on each
 * session, logging any one-time fields. Interfaces with the telemetry backend used for submitting
 * data to the web service. Submits session data on close.
 */
class TelemetrySession {
public:
    explicit TelemetrySession();
    ~TelemetrySession();

    TelemetrySession(const TelemetrySession&) = delete;
    TelemetrySession& operator=(const TelemetrySession&) = delete;

    TelemetrySession(TelemetrySession&&) = delete;
    TelemetrySession& operator=(TelemetrySession&&) = delete;

    /**
     * Adds the initial telemetry info necessary when starting up a title.
     *
     * This includes information such as:
     *   - Telemetry ID
     *   - Initialization time
     *   - Title ID
     *   - Title name
     *   - Title file format
     *   - Miscellaneous settings values.
     *
     * @param app_loader The application loader to use to retrieve
     *                   title-specific information.
     */
    void AddInitialInfo(Loader::AppLoader& app_loader);

    /**
     * Wrapper around the Telemetry::FieldCollection::AddField method.
     * @param type Type of the field to add.
     * @param name Name of the field to add.
     * @param value Value for the field to add.
     */
    template <typename T>
    void AddField(Common::Telemetry::FieldType type, const char* name, T value) {
        field_collection.AddField(type, name, std::move(value));
    }

    /**
     * Submits a Testcase.
     * @returns A bool indicating whether the submission succeeded
     */
    bool SubmitTestcase();

private:
    /// Tracks all added fields for the session
    Common::Telemetry::FieldCollection field_collection;
};

/**
 * Gets TelemetryId, a unique identifier used for the user's telemetry sessions.
 * @returns The current TelemetryId for the session.
 */
u64 GetTelemetryId();

/**
 * Regenerates TelemetryId, a unique identifier used for the user's telemetry sessions.
 * @returns The new TelemetryId that was generated.
 */
u64 RegenerateTelemetryId();

/**
 * Verifies the username and token.
 * @param username Citra username to use for authentication.
 * @param token Citra token to use for authentication.
 * @returns Future with bool indicating whether the verification succeeded
 */
bool VerifyLogin(const std::string& username, const std::string& token);

} // namespace Core
