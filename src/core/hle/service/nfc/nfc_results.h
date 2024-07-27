// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/result.h"

namespace Service::NFC {

namespace ErrCodes {
enum {
    InvalidArgumentValue = 80,
    InvalidArgument = 81,

    InvalidChecksum = 200,

    Failed = 320,
    OperationFailed = 328,
    DataAccessFailed = 336,
    FatalError = 384,
    Unexpected = 392,

    InvalidOperation = 512,
    NotSupported = 522,
    NeedRestore = 528,
    NeedFormat = 536,
    NeedCreate = 544,
    NeedRegister = 552,
    AlreadyCreated = 560,
    AccessIdMisMatch = 568,
    NotBroken = 576,
    UidMisMatch = 584,
    InvalidFormatVersion = 592,

    NotImplemented = 600,
    Sleep = 608,
    WifiOff = 616,
    UpdateRequired = 624,
    IrFunctionError = 640,
    NfcTargetError = 648,
    ConnectCanceled = 656,
};
} // namespace ErrCodes

constexpr Result ResultInvalidArgumentValue(ErrCodes::InvalidArgumentValue, ErrorModule::NFC,
                                            ErrorSummary::InvalidArgument, ErrorLevel::Status);
constexpr Result ResultInvalidArgument(ErrCodes::InvalidArgument, ErrorModule::NFC,
                                       ErrorSummary::InvalidArgument, ErrorLevel::Status);
constexpr Result ResultInvalidOperation(ErrCodes::InvalidOperation, ErrorModule::NFC,
                                        ErrorSummary::InvalidState, ErrorLevel::Status);
constexpr Result ResultNotSupported(ErrCodes::NotSupported, ErrorModule::NFC,
                                    ErrorSummary::InvalidState, ErrorLevel::Status);
constexpr Result ResultNeedFormat(ErrCodes::NeedFormat, ErrorModule::NFC,
                                  ErrorSummary::InvalidState, ErrorLevel::Status);
constexpr Result ResultOperationFailed(ErrCodes::OperationFailed, ErrorModule::NFC,
                                       ErrorSummary::InvalidState, ErrorLevel::Status);
constexpr Result ResultNeedCreate(ErrCodes::NeedCreate, ErrorModule::NFC,
                                  ErrorSummary::InvalidState, ErrorLevel::Status);
constexpr Result ResultNeedRegister(ErrCodes::NeedRegister, ErrorModule::NFC,
                                    ErrorSummary::InvalidState, ErrorLevel::Status);
constexpr Result ResultAlreadyCreated(ErrCodes::AlreadyCreated, ErrorModule::NFC,
                                      ErrorSummary::InvalidState, ErrorLevel::Status);
constexpr Result ResultAccessIdMisMatch(ErrCodes::AccessIdMisMatch, ErrorModule::NFC,
                                        ErrorSummary::InvalidState, ErrorLevel::Status);
constexpr Result ResultSleep(ErrCodes::Sleep, ErrorModule::NFC, ErrorSummary::InvalidState,
                             ErrorLevel::Status);
constexpr Result ResultWifiOff(ErrCodes::WifiOff, ErrorModule::NFC, ErrorSummary::InvalidState,
                               ErrorLevel::Status);

} // namespace Service::NFC
