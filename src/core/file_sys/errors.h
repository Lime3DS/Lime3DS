// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/result.h"

namespace FileSys {

namespace ErrCodes {
enum {
    RomFSNotFound = 100,
    ArchiveNotMounted = 101,
    FileNotFound = 112,
    PathNotFound = 113,
    GameCardNotInserted = 141,
    NotFound = 120,
    FileAlreadyExists = 180,
    DirectoryAlreadyExists = 185,
    AlreadyExists = 190,
    InsufficientSpace = 210,
    InvalidOpenFlags = 230,
    DirectoryNotEmpty = 240,
    NotAFile = 250,
    NotFormatted = 340, ///< This is used by the FS service when creating a SaveData archive
    ExeFSSectionNotFound = 567,
    CommandNotAllowed = 630,
    InvalidReadFlag = 700,
    InvalidPath = 702,
    WriteBeyondEnd = 705,
    UnsupportedOpenFlags = 760,
    IncorrectExeFSReadSize = 761,
    UnexpectedFileOrDirectory = 770,
};
}

constexpr Result ResultInvalidPath(ErrCodes::InvalidPath, ErrorModule::FS,
                                   ErrorSummary::InvalidArgument, ErrorLevel::Usage);
constexpr Result ResultUnsupportedOpenFlags(ErrCodes::UnsupportedOpenFlags, ErrorModule::FS,
                                            ErrorSummary::NotSupported, ErrorLevel::Usage);
constexpr Result ResultInvalidOpenFlags(ErrCodes::InvalidOpenFlags, ErrorModule::FS,
                                        ErrorSummary::Canceled, ErrorLevel::Status);
constexpr Result ResultInvalidReadFlag(ErrCodes::InvalidReadFlag, ErrorModule::FS,
                                       ErrorSummary::InvalidArgument, ErrorLevel::Usage);
constexpr Result ResultFileNotFound(ErrCodes::FileNotFound, ErrorModule::FS, ErrorSummary::NotFound,
                                    ErrorLevel::Status);
constexpr Result ResultPathNotFound(ErrCodes::PathNotFound, ErrorModule::FS, ErrorSummary::NotFound,
                                    ErrorLevel::Status);
constexpr Result ResultNotFound(ErrCodes::NotFound, ErrorModule::FS, ErrorSummary::NotFound,
                                ErrorLevel::Status);
constexpr Result ResultUnexpectedFileOrDirectory(ErrCodes::UnexpectedFileOrDirectory,
                                                 ErrorModule::FS, ErrorSummary::NotSupported,
                                                 ErrorLevel::Usage);
constexpr Result ResultUnexpectedFileOrDirectorySdmc(ErrCodes::NotAFile, ErrorModule::FS,
                                                     ErrorSummary::Canceled, ErrorLevel::Status);
constexpr Result ResultDirectoryAlreadyExists(ErrCodes::DirectoryAlreadyExists, ErrorModule::FS,
                                              ErrorSummary::NothingHappened, ErrorLevel::Status);
constexpr Result ResultFileAlreadyExists(ErrCodes::FileAlreadyExists, ErrorModule::FS,
                                         ErrorSummary::NothingHappened, ErrorLevel::Status);
constexpr Result ResultAlreadyExists(ErrCodes::AlreadyExists, ErrorModule::FS,
                                     ErrorSummary::NothingHappened, ErrorLevel::Status);
constexpr Result ResultDirectoryNotEmpty(ErrCodes::DirectoryNotEmpty, ErrorModule::FS,
                                         ErrorSummary::Canceled, ErrorLevel::Status);
constexpr Result ResultGamecardNotInserted(ErrCodes::GameCardNotInserted, ErrorModule::FS,
                                           ErrorSummary::NotFound, ErrorLevel::Status);
constexpr Result ResultIncorrectExefsReadSize(ErrCodes::IncorrectExeFSReadSize, ErrorModule::FS,
                                              ErrorSummary::NotSupported, ErrorLevel::Usage);
constexpr Result ResultRomfsNotFound(ErrCodes::RomFSNotFound, ErrorModule::FS,
                                     ErrorSummary::NotFound, ErrorLevel::Status);
constexpr Result ResultCommandNotAllowed(ErrCodes::CommandNotAllowed, ErrorModule::FS,
                                         ErrorSummary::WrongArgument, ErrorLevel::Permanent);
constexpr Result ResultExefsSectionNotFound(ErrCodes::ExeFSSectionNotFound, ErrorModule::FS,
                                            ErrorSummary::NotFound, ErrorLevel::Status);
constexpr Result ResultInsufficientSpace(ErrCodes::InsufficientSpace, ErrorModule::FS,
                                         ErrorSummary::OutOfResource, ErrorLevel::Status);

/// Returned when a function is passed an invalid archive handle.
constexpr Result ResultInvalidArchiveHandle(ErrCodes::ArchiveNotMounted, ErrorModule::FS,
                                            ErrorSummary::NotFound,
                                            ErrorLevel::Status); // 0xC8804465
constexpr Result ResultWriteBeyondEnd(ErrCodes::WriteBeyondEnd, ErrorModule::FS,
                                      ErrorSummary::InvalidArgument, ErrorLevel::Usage);

/**
 * Variant of ResultNotFound returned in some places in the code. Unknown if these usages are
 * correct or a bug.
 */
constexpr Result ResultNotFoundInvalidState(ErrCodes::NotFound, ErrorModule::FS,
                                            ErrorSummary::InvalidState, ErrorLevel::Status);
constexpr Result ResultNotFormatted(ErrCodes::NotFormatted, ErrorModule::FS,
                                    ErrorSummary::InvalidState, ErrorLevel::Status);

} // namespace FileSys
