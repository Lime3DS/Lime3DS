// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/serialization/access.hpp>
#include "common/assert.h"
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/expected.h"

// All the constants in this file come from http://3dbrew.org/wiki/Error_codes

/**
 * Detailed description of the error. Code 0 always means success. Codes 1000 and above are
 * considered "well-known" and have common values between all modules. The meaning of other codes
 * vary by module.
 */
enum class ErrorDescription : u32 {
    Success = 0,

    // Codes 1000 and above are considered "well-known" and have common values between all modules.
    InvalidSection = 1000,
    TooLarge = 1001,
    NotAuthorized = 1002,
    AlreadyDone = 1003,
    InvalidSize = 1004,
    InvalidEnumValue = 1005,
    InvalidCombination = 1006,
    NoData = 1007,
    Busy = 1008,
    MisalignedAddress = 1009,
    MisalignedSize = 1010,
    OutOfMemory = 1011,
    NotImplemented = 1012,
    InvalidAddress = 1013,
    InvalidPointer = 1014,
    InvalidHandle = 1015,
    NotInitialized = 1016,
    AlreadyInitialized = 1017,
    NotFound = 1018,
    CancelRequested = 1019,
    AlreadyExists = 1020,
    OutOfRange = 1021,
    Timeout = 1022,
    InvalidResultValue = 1023,
};

/**
 * Identifies the module which caused the error. Error codes can be propagated through a call
 * chain, meaning that this doesn't always correspond to the module where the API call made is
 * contained.
 */
enum class ErrorModule : u32 {
    Common = 0,
    Kernel = 1,
    Util = 2,
    FileServer = 3,
    LoaderServer = 4,
    TCB = 5,
    OS = 6,
    DBG = 7,
    DMNT = 8,
    PDN = 9,
    GX = 10,
    I2C = 11,
    GPIO = 12,
    DD = 13,
    CODEC = 14,
    SPI = 15,
    PXI = 16,
    FS = 17,
    DI = 18,
    HID = 19,
    CAM = 20,
    PI = 21,
    PM = 22,
    PM_LOW = 23,
    FSI = 24,
    SRV = 25,
    NDM = 26,
    NWM = 27,
    SOC = 28,
    LDR = 29,
    ACC = 30,
    RomFS = 31,
    AM = 32,
    HIO = 33,
    Updater = 34,
    MIC = 35,
    FND = 36,
    MP = 37,
    MPWL = 38,
    AC = 39,
    HTTP = 40,
    DSP = 41,
    SND = 42,
    DLP = 43,
    HIO_LOW = 44,
    CSND = 45,
    SSL = 46,
    AM_LOW = 47,
    NEX = 48,
    Friends = 49,
    RDT = 50,
    Applet = 51,
    NIM = 52,
    PTM = 53,
    MIDI = 54,
    MC = 55,
    SWC = 56,
    FatFS = 57,
    NGC = 58,
    CARD = 59,
    CARDNOR = 60,
    SDMC = 61,
    BOSS = 62,
    DBM = 63,
    Config = 64,
    PS = 65,
    CEC = 66,
    IR = 67,
    UDS = 68,
    PL = 69,
    CUP = 70,
    Gyroscope = 71,
    MCU = 72,
    NS = 73,
    News = 74,
    RO = 75,
    GD = 76,
    CardSPI = 77,
    EC = 78,
    WebBrowser = 79,
    Test = 80,
    ENC = 81,
    PIA = 82,
    ACT = 83,
    VCTL = 84,
    OLV = 85,
    NEIA = 86,
    NPNS = 87,

    AVD = 90,
    L2B = 91,
    MVD = 92,
    NFC = 93,
    UART = 94,
    SPM = 95,
    QTM = 96,
    NFP = 97,

    Application = 254,
    InvalidResult = 255
};

/// A less specific error cause.
enum class ErrorSummary : u32 {
    Success = 0,
    NothingHappened = 1,
    WouldBlock = 2,
    OutOfResource = 3, ///< There are no more kernel resources (memory, table slots) to
                       ///< execute the operation.
    NotFound = 4,      ///< A file or resource was not found.
    InvalidState = 5,
    NotSupported = 6,    ///< The operation is not supported or not implemented.
    InvalidArgument = 7, ///< Returned when a passed argument is invalid in the current runtime
                         ///< context. (Invalid handle, out-of-bounds pointer or size, etc.)
    WrongArgument = 8,   ///< Returned when a passed argument is in an incorrect format for use
                         ///< with the function. (E.g. Invalid enum value)
    Canceled = 9,
    StatusChanged = 10,
    Internal = 11,

    InvalidResult = 63
};

/// The severity of the error.
enum class ErrorLevel : u32 {
    Success = 0,
    Info = 1,

    Status = 25,
    Temporary = 26,
    Permanent = 27,
    Usage = 28,
    Reinitialize = 29,
    Reset = 30,
    Fatal = 31
};

/// Encapsulates a CTR-OS error code, allowing it to be separated into its constituent fields.
union Result {
    u32 raw;

    BitField<0, 10, u32> description;
    BitField<10, 8, ErrorModule> module;

    BitField<21, 6, ErrorSummary> summary;
    BitField<27, 5, ErrorLevel> level;

    // The last bit of `level` is checked by apps and the kernel to determine if a result code is an
    // error
    BitField<31, 1, u32> is_error;

    constexpr explicit Result(u32 raw) : raw(raw) {}

    constexpr Result(ErrorDescription description, ErrorModule module, ErrorSummary summary,
                     ErrorLevel level)
        : Result(static_cast<u32>(description), module, summary, level) {}

    constexpr Result(u32 description_, ErrorModule module_, ErrorSummary summary_,
                     ErrorLevel level_)
        : raw(description.FormatValue(description_) | module.FormatValue(module_) |
              summary.FormatValue(summary_) | level.FormatValue(level_)) {}

    constexpr Result& operator=(const Result& o) = default;

    constexpr bool IsSuccess() const {
        return is_error.ExtractValue(raw) == 0;
    }

    constexpr bool IsError() const {
        return is_error.ExtractValue(raw) == 1;
    }

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & raw;
    }
    friend class boost::serialization::access;
};

constexpr bool operator==(const Result& a, const Result& b) {
    return a.raw == b.raw;
}

constexpr bool operator!=(const Result& a, const Result& b) {
    return a.raw != b.raw;
}

// Convenience functions for creating some common kinds of errors:

/// The default success `Result`.
constexpr Result ResultSuccess(0);

/// Might be returned instead of a dummy success for unimplemented APIs.
constexpr Result UnimplementedFunction(ErrorModule module) {
    return Result(ErrorDescription::NotImplemented, module, ErrorSummary::NotSupported,
                  ErrorLevel::Permanent);
}

/**
 * Placeholder result code used for unknown error codes.
 *
 * @note This should only be used when a particular error code
 *       is not known yet.
 */
constexpr Result ResultUnknown(std::numeric_limits<u32>::max());

/**
 * This is an optional value type. It holds a `Result` and, if that code is ResultSuccess, it
 * also holds a result of type `T`. If the code is an error code (not ResultSuccess), then trying
 * to access the inner value with operator* is undefined behavior and will assert with Unwrap().
 * Users of this class must be cognizant to check the status of the ResultVal with operator bool(),
 * Code(), Succeeded() or Failed() prior to accessing the inner value.
 *
 * An example of how it could be used:
 * \code
 * ResultVal<int> Frobnicate(float strength) {
 *     if (strength < 0.f || strength > 1.0f) {
 *         // Can't frobnicate too weakly or too strongly
 *         return Result(ErrorDescription::OutOfRange, ErrorModule::Common,
 *             ErrorSummary::InvalidArgument, ErrorLevel::Permanent);
 *     } else {
 *         // Frobnicated! Give caller a cookie
 *         return 42;
 *     }
 * }
 * \endcode
 *
 * \code
 * auto frob_result = Frobnicate(0.75f);
 * if (frob_result) {
 *     // Frobbed ok
 *     printf("My cookie is %d\n", *frob_result);
 * } else {
 *     printf("Guess I overdid it. :( Error code: %ux\n", frob_result.Code().raw);
 * }
 * \endcode
 */
template <typename T>
class ResultVal {
public:
    constexpr ResultVal() : expected{} {}

    constexpr ResultVal(Result code) : expected{Common::Unexpected(code)} {}

    template <typename U>
    constexpr ResultVal(U&& val) : expected{std::forward<U>(val)} {}

    template <typename... Args>
    constexpr ResultVal(Args&&... args) : expected{std::in_place, std::forward<Args>(args)...} {}

    ~ResultVal() = default;

    constexpr ResultVal(const ResultVal&) = default;
    constexpr ResultVal(ResultVal&&) = default;

    ResultVal& operator=(const ResultVal&) = default;
    ResultVal& operator=(ResultVal&&) = default;

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return expected.has_value();
    }

    [[nodiscard]] constexpr Result Code() const {
        return expected.has_value() ? ResultSuccess : expected.error();
    }

    [[nodiscard]] constexpr bool Succeeded() const {
        return expected.has_value();
    }

    [[nodiscard]] constexpr bool Failed() const {
        return !expected.has_value();
    }

    [[nodiscard]] constexpr T* operator->() {
        return std::addressof(expected.value());
    }

    [[nodiscard]] constexpr const T* operator->() const {
        return std::addressof(expected.value());
    }

    [[nodiscard]] constexpr T& operator*() & {
        return *expected;
    }

    [[nodiscard]] constexpr const T& operator*() const& {
        return *expected;
    }

    [[nodiscard]] constexpr T&& operator*() && {
        return *expected;
    }

    [[nodiscard]] constexpr const T&& operator*() const&& {
        return *expected;
    }

    [[nodiscard]] constexpr T& Unwrap() & {
        ASSERT_MSG(Succeeded(), "Tried to Unwrap empty ResultVal");
        return expected.value();
    }

    [[nodiscard]] constexpr const T& Unwrap() const& {
        ASSERT_MSG(Succeeded(), "Tried to Unwrap empty ResultVal");
        return expected.value();
    }

    [[nodiscard]] constexpr T&& Unwrap() && {
        ASSERT_MSG(Succeeded(), "Tried to Unwrap empty ResultVal");
        return std::move(expected.value());
    }

    [[nodiscard]] constexpr const T&& Unwrap() const&& {
        ASSERT_MSG(Succeeded(), "Tried to Unwrap empty ResultVal");
        return std::move(expected.value());
    }

    template <typename U>
    [[nodiscard]] constexpr T ValueOr(U&& v) const& {
        return expected.value_or(v);
    }

    template <typename U>
    [[nodiscard]] constexpr T ValueOr(U&& v) && {
        return expected.value_or(v);
    }

private:
    // TODO: Replace this with std::expected once it is standardized in the STL.
    Common::Expected<T, Result> expected;
};

/**
 * Check for the success of `source` (which must evaluate to a ResultVal). If it succeeds, unwraps
 * the contained value and assigns it to `target`, which can be either an l-value expression or a
 * variable declaration. If it fails the return code is returned from the current function. Thus it
 * can be used to cascade errors out, achieving something akin to exception handling.
 */
#define CASCADE_RESULT(target, source)                                                             \
    auto CONCAT2(check_result_L, __LINE__) = source;                                               \
    if (CONCAT2(check_result_L, __LINE__).Failed())                                                \
        return CONCAT2(check_result_L, __LINE__).Code();                                           \
    target = std::move(*CONCAT2(check_result_L, __LINE__))

#define R_SUCCEEDED(res) (static_cast<Result>(res).IsSuccess())
#define R_FAILED(res) (static_cast<Result>(res).IsError())

/// Evaluates a boolean expression, and returns a result unless that expression is true.
#define R_UNLESS(expr, res)                                                                        \
    {                                                                                              \
        if (!(expr)) {                                                                             \
            return (res);                                                                          \
        }                                                                                          \
    }

/// Evaluates an expression that returns a result, and returns the result if it would fail.
#define R_TRY(res_expr)                                                                            \
    {                                                                                              \
        const auto _tmp_r_try_rc = (res_expr);                                                     \
        if (R_FAILED(_tmp_r_try_rc)) {                                                             \
            return (_tmp_r_try_rc);                                                                \
        }                                                                                          \
    }

/// Evaluates a boolean expression, and succeeds if that expression is true.
#define R_SUCCEED_IF(expr) R_UNLESS(!(expr), ResultSuccess)

/// Evaluates a boolean expression, and asserts if that expression is false.
#define R_ASSERT(expr) ASSERT(R_SUCCEEDED(expr))
