// Copyright 2014-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

// Directory separators, do we need this?
#define DIR_SEP "/"
#define DIR_SEP_CHR '/'
#define DIR_SEP_CHR_WIN '\\'

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

// The user data dir
#define USERDATA_DIR "user"
#ifdef USER_DIR
#define EMU_DATA_DIR USER_DIR
#else
#ifdef _WIN32
#define EMU_DATA_DIR "Azahar"
#define LEGACY_CITRA_DATA_DIR "Citra"
#define LEGACY_LIME3DS_DATA_DIR "Lime3DS"
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define EMU_APPLE_DATA_DIR "Documents" DIR_SEP "Azahar"
#define LEGACY_CITRA_APPLE_DATA_DIR "Documents" DIR_SEP "Citra"
#define LEGACY_LIME3DS_APPLE_DATA_DIR "Documents" DIR_SEP "Lime3DS"
#else
#define EMU_APPLE_DATA_DIR "Library" DIR_SEP "Application Support" DIR_SEP "Azahar"
#define LEGACY_CITRA_APPLE_DATA_DIR "Library" DIR_SEP "Application Support" DIR_SEP "Citra"
#define LEGACY_LIME3DS_APPLE_DATA_DIR "Library" DIR_SEP "Application Support" DIR_SEP "Lime3DS"
#endif
// For compatibility with XDG paths.
#define EMU_DATA_DIR "azahar-emu"
#define LEGACY_CITRA_DATA_DIR "citra-emu"
#define LEGACY_LIME3DS_DATA_DIR "lime3ds-emu"
#else
#define EMU_DATA_DIR "azahar-emu"
#define LEGACY_CITRA_DATA_DIR "citra-emu"
#define LEGACY_LIME3DS_DATA_DIR "lime3ds-emu"
#endif
#endif

// Dirs in both User and Sys
#define EUR_DIR "EUR"
#define USA_DIR "USA"
#define JAP_DIR "JAP"

// Subdirs in the User dir returned by GetUserPath(UserPath::UserDir)
#define CONFIG_DIR "config"
#define CACHE_DIR "cache"
#define SDMC_DIR "sdmc"
#define NAND_DIR "nand"
#define SYSDATA_DIR "sysdata"
#define LOG_DIR "log"
#define CHEATS_DIR "cheats"
#define SHADER_DIR "shaders"
#define DUMP_DIR "dump"
#define LIBRARIES_DIR "libraries"
#define LOAD_DIR "load"
#define SHADER_DIR "shaders"
#define STATES_DIR "states"
#define ICONS_DIR "icons"

// Filenames
// Files in the directory returned by GetUserPath(UserPath::LogDir)
#define LOG_FILE "azahar_log.txt"

// Files in the directory returned by GetUserPath(UserPath::ConfigDir)
#define EMU_CONFIG "emu.ini"
#define DEBUGGER_CONFIG "debugger.ini"
#define LOGGER_CONFIG "logger.ini"

// Sys files
#define SHARED_FONT "shared_font.bin"
#define KEYS_FILE "keys.txt"
#define BOOTROM9 "boot9.bin"
#define SECRET_SECTOR "sector0x96.bin"
