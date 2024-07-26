// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/result.h"

namespace Kernel {
namespace ErrCodes {
enum {
    OutOfSharedMems = 11,
    OutOfThreads = 12,
    OutOfMutexes = 13,
    OutOfSemaphores = 14,
    OutOfEvents = 15,
    OutOfTimers = 16,
    OutOfHandles = 19,
    SessionClosedByRemote = 26,
    PortNameTooLong = 30,
    WrongLockingThread = 31,
    NoPendingSessions = 35,
    WrongPermission = 46,
    InvalidBufferDescriptor = 48,
    OutOfAddressArbiters = 51,
    MaxConnectionsReached = 52,
    CommandTooLarge = 54,
};
}

// WARNING: The kernel is quite inconsistent in it's usage of errors code. Make sure to always
// double check that the code matches before re-using the constant.

constexpr Result ResultOutOfHandles(ErrCodes::OutOfHandles, ErrorModule::Kernel,
                                    ErrorSummary::OutOfResource,
                                    ErrorLevel::Permanent); // 0xD8600413
constexpr Result ResultSessionClosed(ErrCodes::SessionClosedByRemote, ErrorModule::OS,
                                     ErrorSummary::Canceled,
                                     ErrorLevel::Status); // 0xC920181A
constexpr Result ResultPortNameTooLong(ErrCodes::PortNameTooLong, ErrorModule::OS,
                                       ErrorSummary::InvalidArgument,
                                       ErrorLevel::Usage); // 0xE0E0181E
constexpr Result ResultWrongPermission(ErrCodes::WrongPermission, ErrorModule::OS,
                                       ErrorSummary::WrongArgument, ErrorLevel::Permanent);
constexpr Result ResultInvalidBufferDescriptor(ErrCodes::InvalidBufferDescriptor, ErrorModule::OS,
                                               ErrorSummary::WrongArgument, ErrorLevel::Permanent);
constexpr Result ResultMaxConnectionsReached(ErrCodes::MaxConnectionsReached, ErrorModule::OS,
                                             ErrorSummary::WouldBlock,
                                             ErrorLevel::Temporary); // 0xD0401834

constexpr Result ResultNotAuthorized(ErrorDescription::NotAuthorized, ErrorModule::OS,
                                     ErrorSummary::WrongArgument,
                                     ErrorLevel::Permanent); // 0xD9001BEA
constexpr Result ResultInvalidEnumValue(ErrorDescription::InvalidEnumValue, ErrorModule::Kernel,
                                        ErrorSummary::InvalidArgument,
                                        ErrorLevel::Permanent); // 0xD8E007ED
constexpr Result ResultInvalidEnumValueFnd(ErrorDescription::InvalidEnumValue, ErrorModule::FND,
                                           ErrorSummary::InvalidArgument,
                                           ErrorLevel::Permanent); // 0xD8E093ED
constexpr Result ResultInvalidCombination(ErrorDescription::InvalidCombination, ErrorModule::OS,
                                          ErrorSummary::InvalidArgument,
                                          ErrorLevel::Usage); // 0xE0E01BEE
constexpr Result ResultInvalidCombinationKernel(ErrorDescription::InvalidCombination,
                                                ErrorModule::Kernel, ErrorSummary::WrongArgument,
                                                ErrorLevel::Permanent); // 0xD90007EE
constexpr Result ResultMisalignedAddress(ErrorDescription::MisalignedAddress, ErrorModule::OS,
                                         ErrorSummary::InvalidArgument,
                                         ErrorLevel::Usage); // 0xE0E01BF1
constexpr Result ResultMisalignedSize(ErrorDescription::MisalignedSize, ErrorModule::OS,
                                      ErrorSummary::InvalidArgument,
                                      ErrorLevel::Usage); // 0xE0E01BF2
constexpr Result ResultOutOfMemory(ErrorDescription::OutOfMemory, ErrorModule::Kernel,
                                   ErrorSummary::OutOfResource,
                                   ErrorLevel::Permanent); // 0xD86007F3
/// Returned when out of heap or linear heap memory when allocating
constexpr Result ResultOutOfHeapMemory(ErrorDescription::OutOfMemory, ErrorModule::OS,
                                       ErrorSummary::OutOfResource,
                                       ErrorLevel::Status); // 0xC860180A
constexpr Result ResultNotImplemented(ErrorDescription::NotImplemented, ErrorModule::OS,
                                      ErrorSummary::InvalidArgument,
                                      ErrorLevel::Usage); // 0xE0E01BF4
constexpr Result ResultInvalidAddress(ErrorDescription::InvalidAddress, ErrorModule::OS,
                                      ErrorSummary::InvalidArgument,
                                      ErrorLevel::Usage); // 0xE0E01BF5
constexpr Result ResultInvalidAddressState(ErrorDescription::InvalidAddress, ErrorModule::OS,
                                           ErrorSummary::InvalidState,
                                           ErrorLevel::Usage); // 0xE0A01BF5
constexpr Result ResultInvalidPointer(ErrorDescription::InvalidPointer, ErrorModule::Kernel,
                                      ErrorSummary::InvalidArgument,
                                      ErrorLevel::Permanent); // 0xD8E007F6
constexpr Result ResultInvalidHandle(ErrorDescription::InvalidHandle, ErrorModule::Kernel,
                                     ErrorSummary::InvalidArgument,
                                     ErrorLevel::Permanent); // 0xD8E007F7
/// Alternate code returned instead of ResultInvalidHandle in some code paths.
constexpr Result ResultInvalidHandleOs(ErrorDescription::InvalidHandle, ErrorModule::OS,
                                       ErrorSummary::WrongArgument,
                                       ErrorLevel::Permanent); // 0xD9001BF7
constexpr Result ResultNotFound(ErrorDescription::NotFound, ErrorModule::Kernel,
                                ErrorSummary::NotFound, ErrorLevel::Permanent); // 0xD88007FA
constexpr Result ResultOutOfRange(ErrorDescription::OutOfRange, ErrorModule::OS,
                                  ErrorSummary::InvalidArgument,
                                  ErrorLevel::Usage); // 0xE0E01BFD
constexpr Result ResultOutOfRangeKernel(ErrorDescription::OutOfRange, ErrorModule::Kernel,
                                        ErrorSummary::InvalidArgument,
                                        ErrorLevel::Permanent); // 0xD8E007FD
constexpr Result ResultTimeout(ErrorDescription::Timeout, ErrorModule::OS,
                               ErrorSummary::StatusChanged, ErrorLevel::Info);
/// Returned when Accept() is called on a port with no sessions to be accepted.
constexpr Result ResultNoPendingSessions(ErrCodes::NoPendingSessions, ErrorModule::OS,
                                         ErrorSummary::WouldBlock,
                                         ErrorLevel::Permanent); // 0xD8401823

} // namespace Kernel
