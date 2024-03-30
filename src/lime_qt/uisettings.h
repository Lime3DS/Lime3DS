// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>
#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>
#include "common/settings.h"

namespace UISettings {

struct ContextualShortcut {
    QString keyseq;
    int context;
};

struct Shortcut {
    QString name;
    QString group;
    ContextualShortcut shortcut;
};

using Themes = std::array<std::pair<const char*, const char*>, 6>;
extern const Themes themes;

struct GameDir {
    QString path;
    bool deep_scan = false;
    bool expanded = false;
    bool operator==(const GameDir& rhs) const {
        return path == rhs.path;
    }
    bool operator!=(const GameDir& rhs) const {
        return !operator==(rhs);
    }
};

enum class GameListIconSize : u32 {
    NoIcon,    ///< Do not display icons
    SmallIcon, ///< Display a small (24x24) icon
    LargeIcon, ///< Display a large (48x48) icon
};

enum class GameListText : s32 {
    NoText = -1,   ///< No text
    FileName,      ///< Display the file name of the entry
    FullPath,      ///< Display the full path of the entry
    TitleName,     ///< Display the name of the title
    TitleID,       ///< Display the title ID
    LongTitleName, ///< Display the long name of the title
    ListEnd,       ///< Keep this at the end of the enum.
};

struct Values {
    QByteArray geometry;
    QByteArray state;

    QByteArray renderwindow_geometry;

    QByteArray gamelist_header_state;

    QByteArray microprofile_geometry;
    Settings::Setting<bool> microprofile_visible{false, "microProfileDialogVisible"};

    Settings::Setting<bool> single_window_mode{true, "singleWindowMode"};
    Settings::Setting<bool> fullscreen{false, "fullscreen"};
    Settings::Setting<bool> display_titlebar{true, "displayTitleBars"};
    Settings::Setting<bool> show_filter_bar{true, "showFilterBar"};
    Settings::Setting<bool> show_status_bar{true, "showStatusBar"};

    Settings::Setting<bool> confirm_before_closing{true, "confirmClose"};
    Settings::Setting<bool> save_state_warning{true, "saveStateWarning"};
    Settings::Setting<bool> first_start{true, "firstStart"};
    Settings::Setting<bool> pause_when_in_background{false, "pauseWhenInBackground"};
    Settings::Setting<bool> mute_when_in_background{false, "muteWhenInBackground"};
    Settings::Setting<bool> hide_mouse{false, "hideInactiveMouse"};

    bool updater_found;
    Settings::Setting<bool> update_on_close{false, "update_on_close"};
    Settings::Setting<bool> check_for_update_on_start{true, "check_for_update_on_start"};

    // Discord RPC
    Settings::Setting<bool> enable_discord_presence{true, "enable_discord_presence"};

    // Game List
    Settings::Setting<GameListIconSize> game_list_icon_size{GameListIconSize::LargeIcon,
                                                            "iconSize"};
    Settings::Setting<GameListText> game_list_row_1{GameListText::TitleName, "row1"};
    Settings::Setting<GameListText> game_list_row_2{GameListText::FileName, "row2"};
    Settings::Setting<bool> game_list_hide_no_icon{false, "hideNoIcon"};
    Settings::Setting<bool> game_list_single_line_mode{false, "singleLineMode"};

    // Compatibility List
    Settings::Setting<bool> show_compat_column{true, "show_compat_column"};
    Settings::Setting<bool> show_region_column{true, "show_region_column"};
    Settings::Setting<bool> show_type_column{true, "show_type_column"};
    Settings::Setting<bool> show_size_column{true, "show_size_column"};

    Settings::Setting<u16> screenshot_resolution_factor{0, "screenshot_resolution_factor"};
    Settings::SwitchableSetting<std::string> screenshot_path{"", "screenshotPath"};

    QString roms_path;
    QString symbols_path;
    QString movie_record_path;
    QString movie_playback_path;
    QString video_dumping_path;
    QString game_dir_deprecated;
    bool game_dir_deprecated_deepscan;
    QVector<UISettings::GameDir> game_dirs;
    QStringList recent_files;
    QString language;

    QString theme;

    // Shortcut name <Shortcut, context>
    std::vector<Shortcut> shortcuts;

    Settings::Setting<u32> callout_flags{0, "calloutFlags"};

    // multiplayer settings
    QString nickname;
    QString ip;
    QString port;
    QString room_nickname;
    QString room_name;
    quint32 max_player;
    QString room_port;
    uint host_type;
    qulonglong game_id;
    QString room_description;
    std::pair<std::vector<std::string>, std::vector<std::string>> ban_list;

    QString multiplayer_filter_text;
    bool multiplayer_filter_games_owned;
    bool multiplayer_filter_hide_empty;
    bool multiplayer_filter_hide_full;

    // logging
    Settings::Setting<bool> show_console{false, "showConsole"};
};

extern Values values;
} // namespace UISettings

Q_DECLARE_METATYPE(UISettings::GameDir*);
