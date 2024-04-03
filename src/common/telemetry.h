// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include "common/common_types.h"

namespace Common::Telemetry {

/// Field type, used for grouping fields together in the final submitted telemetry log
enum class FieldType : u8 {
    None = 0,     ///< No specified field group
    App,          ///< Citra application fields (e.g. version, branch, etc.)
    Session,      ///< Emulated session fields (e.g. title ID, log, etc.)
    Performance,  ///< Emulated performance (e.g. fps, emulated CPU speed, etc.)
    UserFeedback, ///< User submitted feedback (e.g. star rating, user notes, etc.)
    UserConfig,   ///< User configuration fields (e.g. emulated CPU core, renderer, etc.)
    UserSystem,   ///< User system information (e.g. host CPU type, RAM, etc.)
};

struct VisitorInterface;

/**
 * Interface class for telemetry data fields.
 */
class FieldInterface : NonCopyable {
public:
    virtual ~FieldInterface() = default;

    /**
     * Accept method for the visitor pattern.
     * @param visitor Reference to the visitor that will visit this field.
     */
    virtual void Accept(VisitorInterface& visitor) const = 0;

    /**
     * Gets the name of this field.
     * @returns Name of this field as a string.
     */
    virtual const std::string& GetName() const = 0;
};

/**
 * Represents a telemetry data field, i.e. a unit of data that gets logged and submitted to our
 * telemetry web service.
 */
template <typename T>
class Field : public FieldInterface {
public:
    Field(FieldType type, std::string name, T value)
        : name(std::move(name)), type(type), value(std::move(value)) {}

    Field(const Field&) = default;
    Field& operator=(const Field&) = default;

    Field(Field&&) = default;
    Field& operator=(Field&& other) = default;

    void Accept(VisitorInterface& visitor) const override;

    [[nodiscard]] const std::string& GetName() const override {
        return name;
    }

    /**
     * Returns the type of the field.
     */
    [[nodiscard]] FieldType GetType() const {
        return type;
    }

    /**
     * Returns the value of the field.
     */
    [[nodiscard]] const T& GetValue() const {
        return value;
    }

    [[nodiscard]] bool operator==(const Field& other) const {
        return (type == other.type) && (name == other.name) && (value == other.value);
    }

    [[nodiscard]] bool operator!=(const Field& other) const {
        return !operator==(other);
    }

private:
    std::string name; ///< Field name, must be unique
    FieldType type{}; ///< Field type, used for grouping fields together
    T value;          ///< Field value
};

/**
 * Collection of data fields that have been logged.
 */
class FieldCollection final : NonCopyable {
public:
    FieldCollection() = default;

    /**
     * Accept method for the visitor pattern, visits each field in the collection.
     * @param visitor Reference to the visitor that will visit each field.
     */
    void Accept(VisitorInterface& visitor) const;

    /**
     * Creates a new field and adds it to the field collection.
     * @param type Type of the field to add.
     * @param name Name of the field to add.
     * @param value Value for the field to add.
     */
    template <typename T>
    void AddField(FieldType type, const char* name, T value) {
        return AddField(std::make_unique<Field<T>>(type, name, std::move(value)));
    }

    /**
     * Adds a new field to the field collection.
     * @param field Field to add to the field collection.
     */
    void AddField(std::unique_ptr<FieldInterface> field);

private:
    std::map<std::string, std::unique_ptr<FieldInterface>> fields;
};

/**
 * Telemetry fields visitor interface class. A backend to log to a web service should implement
 * this interface.
 */
struct VisitorInterface : NonCopyable {
    virtual ~VisitorInterface() = default;

    virtual void Visit(const Field<bool>& field) = 0;
    virtual void Visit(const Field<double>& field) = 0;
    virtual void Visit(const Field<float>& field) = 0;
    virtual void Visit(const Field<u8>& field) = 0;
    virtual void Visit(const Field<u16>& field) = 0;
    virtual void Visit(const Field<u32>& field) = 0;
    virtual void Visit(const Field<u64>& field) = 0;
    virtual void Visit(const Field<s8>& field) = 0;
    virtual void Visit(const Field<s16>& field) = 0;
    virtual void Visit(const Field<s32>& field) = 0;
    virtual void Visit(const Field<s64>& field) = 0;
    virtual void Visit(const Field<std::string>& field) = 0;
    virtual void Visit(const Field<const char*>& field) = 0;
    virtual void Visit(const Field<std::chrono::microseconds>& field) = 0;

    /// Completion method, called once all fields have been visited
    virtual void Complete() = 0;
    virtual bool SubmitTestcase() = 0;
};

/**
 * Empty implementation of VisitorInterface that drops all fields. Used when a functional
 * backend implementation is not available.
 */
struct NullVisitor : public VisitorInterface {
    ~NullVisitor() = default;

    void Visit(const Field<bool>& /*field*/) override {}
    void Visit(const Field<double>& /*field*/) override {}
    void Visit(const Field<float>& /*field*/) override {}
    void Visit(const Field<u8>& /*field*/) override {}
    void Visit(const Field<u16>& /*field*/) override {}
    void Visit(const Field<u32>& /*field*/) override {}
    void Visit(const Field<u64>& /*field*/) override {}
    void Visit(const Field<s8>& /*field*/) override {}
    void Visit(const Field<s16>& /*field*/) override {}
    void Visit(const Field<s32>& /*field*/) override {}
    void Visit(const Field<s64>& /*field*/) override {}
    void Visit(const Field<std::string>& /*field*/) override {}
    void Visit(const Field<const char*>& /*field*/) override {}
    void Visit(const Field<std::chrono::microseconds>& /*field*/) override {}

    void Complete() override {}
    bool SubmitTestcase() override {
        return false;
    }
};

/// Appends build-specific information to the given FieldCollection,
/// such as branch name, revision hash, etc.
void AppendBuildInfo(FieldCollection& fc);

/// Appends CPU-specific information to the given FieldCollection,
/// such as instruction set extensions, etc.
void AppendCPUInfo(FieldCollection& fc);

/// Appends OS-specific information to the given FieldCollection,
/// such as platform name, etc.
void AppendOSInfo(FieldCollection& fc);

} // namespace Common::Telemetry
