// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <vector>
#include <QMainWindow>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QTranslator>
#include "core/core.h"
#include "core/savestate.h"
#include "lime_qt/compatibility_list.h"
#include "lime_qt/hotkeys.h"

#ifdef __unix__
#include <QDBusObjectPath>
#endif

// Needs to be included at the end due to https://bugreports.qt.io/browse/QTBUG-73263
#include <filesystem>

class AboutDialog;
class Config;
class ClickableLabel;
class EmuThread;
class GameList;
enum class GameListOpenTarget;
class GameListPlaceholder;
enum class GameListShortcutTarget;
class GImageInfo;
class GPUCommandListWidget;
class GPUCommandStreamWidget;
class GraphicsBreakPointsWidget;
class GraphicsTracingWidget;
class GraphicsVertexShaderWidget;
class GRenderWindow;
class IPCRecorderWidget;
class LLEServiceModulesWidget;
class LoadingScreen;
class MicroProfileDialog;
class MultiplayerState;
class ProfilerWidget;
class QFileOpenEvent;
template <typename>
class QFutureWatcher;
class QLabel;
class QProgressBar;
class QPushButton;
class QSlider;
class RegistersWidget;
#if ENABLE_QT_UPDATER
class Updater;
#endif
class WaitTreeWidget;

namespace Camera {
class QtMultimediaCameraHandlerFactory;
}

namespace DiscordRPC {
class DiscordInterface;
}

namespace PlayTime {
class PlayTimeManager;
}

namespace Core {
class Movie;
}

namespace Ui {
class MainWindow;
}

namespace Service::AM {
enum class InstallStatus : u32;
}

namespace Service::FS {
enum class MediaType : u32;
}

class GMainWindow : public QMainWindow {
    Q_OBJECT

    /// Max number of recently loaded items to keep track of
    static const int max_recent_files_item = 10;

public:
    void filterBarSetChecked(bool state);
    void UpdateUITheme();

    explicit GMainWindow(Core::System& system);
    ~GMainWindow();

    GameList* game_list;
    std::unique_ptr<PlayTime::PlayTimeManager> play_time_manager;
    std::unique_ptr<DiscordRPC::DiscordInterface> discord_rpc;

    bool DropAction(QDropEvent* event);
    void AcceptDropEvent(QDropEvent* event);

    void OnFileOpen(const QFileOpenEvent* event);

    void UninstallTitles(
        const std::vector<std::tuple<Service::FS::MediaType, u64, QString>>& titles);

public slots:
    void OnAppFocusStateChanged(Qt::ApplicationState state);
    void OnLoadComplete();

signals:

    /**
     * Signal that is emitted when a new EmuThread has been created and an emulation session is
     * about to start. At this time, the core system emulation has been initialized, and all
     * emulation handles and memory should be valid.
     *
     * @param emu_thread Pointer to the newly created EmuThread (to be used by widgets that need
     * to access/change emulation state).
     */
    void EmulationStarting(EmuThread* emu_thread);

    /**
     * Signal that is emitted when emulation is about to stop. At this time, the EmuThread and core
     * system emulation handles and memory are still valid, but are about become invalid.
     */
    void EmulationStopping();

    void UpdateProgress(std::size_t written, std::size_t total);
    void CIAInstallReport(Service::AM::InstallStatus status, QString filepath);
    void CIAInstallFinished();
    // Signal that tells widgets to update icons to use the current theme
    void UpdateThemedIcons();

private:
    void InitializeWidgets();
    void InitializeDebugWidgets();
    void InitializeRecentFileMenuActions();
    void InitializeSaveStateMenuActions();

    void SetDefaultUIGeometry();
    void SyncMenuUISettings();
    void RestoreUIState();

    void ConnectAppEvents();
    void ConnectWidgetEvents();
    void ConnectMenuEvents();
    void UpdateMenuState();

    void PreventOSSleep();
    void AllowOSSleep();

    bool LoadROM(const QString& filename);
    void BootGame(const QString& filename);
    void ShutdownGame();

    void SetDiscordEnabled(bool state);
    void LoadAmiibo(const QString& filename);

#if ENABLE_QT_UPDATER
    void ShowUpdaterWidgets();
    void ShowUpdatePrompt();
    void ShowNoUpdatePrompt();
    void CheckForUpdates();
#endif

    /**
     * Stores the filename in the recently loaded files list.
     * The new filename is stored at the beginning of the recently loaded files list.
     * After inserting the new entry, duplicates are removed meaning that if
     * this was inserted from \a OnMenuRecentFile(), the entry will be put on top
     * and remove from its previous position.
     *
     * Finally, this function calls \a UpdateRecentFiles() to update the UI.
     *
     * @param filename the filename to store
     */
    void StoreRecentFile(const QString& filename);

    /**
     * Updates the recent files menu.
     * Menu entries are rebuilt from the configuration file.
     * If there is no entry in the menu, the menu is greyed out.
     */
    void UpdateRecentFiles();

    void UpdateSaveStates();

    /**
     * If the emulation is running,
     * asks the user if he really want to close the emulator
     *
     * @return true if the user confirmed
     */
    bool ConfirmClose();
    bool ConfirmChangeGame();
    void closeEvent(QCloseEvent* event) override;

    enum {
        CREATE_SHORTCUT_MSGBOX_FULLSCREEN_PROMPT,
        CREATE_SHORTCUT_MSGBOX_SUCCESS,
        CREATE_SHORTCUT_MSGBOX_ERROR,
        CREATE_SHORTCUT_MSGBOX_APPIMAGE_VOLATILE_WARNING,
    };

    bool CreateShortcutMessagesGUI(QWidget* parent, int message, const QString& game_title);
    bool MakeShortcutIcoPath(const u64 program_id, const std::string_view game_file_name,
                             std::filesystem::path& out_icon_path);
    bool CreateShortcutLink(const std::filesystem::path& shortcut_path, const std::string& comment,
                            const std::filesystem::path& icon_path,
                            const std::filesystem::path& command, const std::string& arguments,
                            const std::string& categories, const std::string& keywords,
                            const std::string& name);

private slots:
    void OnStartGame();
    void OnRestartGame();
    void OnPauseGame();
    void OnPauseContinueGame();
    void OnStopGame();
    void OnSaveState();
    void OnLoadState();
    void OnMenuReportCompatibility();
    /// Called whenever a user selects a game in the game list widget.
    void OnGameListLoadFile(QString game_path);
    void OnGameListOpenFolder(u64 program_id, GameListOpenTarget target);
    void OnGameListRemovePlayTimeData(u64 program_id);
    void OnGameListCreateShortcut(u64 program_id, const std::string& game_path,
                                  GameListShortcutTarget target);
    void OnGameListDumpRomFS(QString game_path, u64 program_id);
    void OnGameListOpenDirectory(const QString& directory);
    void OnGameListAddDirectory();
    void OnGameListShowList(bool show);
    void OnGameListOpenPerGameProperties(const QString& file);
    void OnConfigurePerGame();
    void OnMenuLoadFile();
    void OnMenuInstallCIA();
    void OnMenuConnectArticBase();
    void OnMenuBootHomeMenu(u32 region);
    void OnUpdateProgress(std::size_t written, std::size_t total);
    void OnCIAInstallReport(Service::AM::InstallStatus status, QString filepath);
    void OnCIAInstallFinished();
    void OnMenuRecentFile();
    void OnConfigure();
    void OnLoadAmiibo();
    void OnRemoveAmiibo();
    void OnOpenCitraFolder();
    void OnToggleFilterBar();
    void OnDisplayTitleBars(bool);
    void InitializeHotkeys();
    void ToggleFullscreen();
    void ToggleSecondaryFullscreen();
    void ChangeScreenLayout();
    void UpdateSecondaryWindowVisibility();
    void ToggleScreenLayout();
    void OnSwapScreens();
    void OnRotateScreens();
    void TriggerSwapScreens();
    void TriggerRotateScreens();
    void ShowFullscreen();
    void HideFullscreen();
    void ToggleWindowMode();
    void OnCreateGraphicsSurfaceViewer();
    void OnRecordMovie();
    void OnPlayMovie();
    void OnCloseMovie();
    void OnSaveMovie();
    void OnCaptureScreenshot();
    void OnDumpVideo();
#ifdef _WIN32
    void OnOpenFFmpeg();
#endif
    void OnStartVideoDumping();
    void StartVideoDumping(const QString& path);
    void OnStopVideoDumping();
    void OnCoreError(Core::System::ResultStatus, std::string);
    /// Called whenever a user selects Help->About Citra
    void OnMenuAboutCitra();

#if ENABLE_QT_UPDATER
    void OnUpdateFound(bool found, bool error);
    void OnCheckForUpdates();
    void OnOpenUpdater();
#endif

    void OnLanguageChanged(const QString& locale);
    void OnMouseActivity();

    void OnDecreaseVolume();
    void OnIncreaseVolume();
    void OnMute();

private:
    Q_INVOKABLE void OnMoviePlaybackCompleted();
    void UpdateStatusBar();
    void UpdateBootHomeMenuState();
    void LoadTranslation();
    void UpdateWindowTitle();
    void UpdateUISettings();
    void RetranslateStatusBar();
    void RemovePlayTimeData(u64 program_id);
    void InstallCIA(QStringList filepaths);
    void HideMouseCursor();
    void ShowMouseCursor();
    void OpenPerGameConfiguration(u64 title_id, const QString& file_name);
    void UpdateVolumeUI();
    void UpdateAPIIndicator(bool update = false);
    void UpdateStatusButtons();
#ifdef __unix__
    void SetGamemodeEnabled(bool state);
#endif

    std::unique_ptr<Ui::MainWindow> ui;
    Core::System& system;
    Core::Movie& movie;

    GRenderWindow* render_window;
    GRenderWindow* secondary_window;

    GameListPlaceholder* game_list_placeholder;
    LoadingScreen* loading_screen;

    // Status bar elements
    QProgressBar* progress_bar = nullptr;
    QLabel* message_label = nullptr;
    bool show_artic_label = false;
    QLabel* artic_traffic_label = nullptr;
    QLabel* emu_speed_label = nullptr;
    QLabel* game_fps_label = nullptr;
    QLabel* emu_frametime_label = nullptr;
    QPushButton* graphics_api_button = nullptr;
    QPushButton* volume_button = nullptr;
    QWidget* volume_popup = nullptr;
    QSlider* volume_slider = nullptr;
    QTimer status_bar_update_timer;
    bool message_label_used_for_movie = false;

    MultiplayerState* multiplayer_state = nullptr;
    std::unique_ptr<Config> config;

    // Whether emulation is currently running in Citra.
    bool emulation_running = false;
    std::unique_ptr<EmuThread> emu_thread;
    // The title of the game currently running
    QString game_title;
    QString game_title_long;
    // The path to the game currently running
    QString game_path;
    // The title id of the game currently running
    u64 game_title_id;

    bool auto_paused = false;
    bool auto_muted = false;
    QTimer mouse_hide_timer;

    // Movie
    bool movie_record_on_start = false;
    QString movie_record_path;
    QString movie_record_author;

    bool movie_playback_on_start = false;
    QString movie_playback_path;

    // Video dumping
    bool video_dumping_on_start = false;
    QString video_dumping_path;
    // Whether game shutdown is delayed due to video dumping
    bool game_shutdown_delayed = false;
    // Whether game was paused due to stopping video dumping
    bool game_paused_for_dumping = false;

    QString gl_renderer;
    std::vector<QString> physical_devices;

    // Debugger panes
    ProfilerWidget* profilerWidget;
    MicroProfileDialog* microProfileDialog;
    RegistersWidget* registersWidget;
    GPUCommandStreamWidget* graphicsWidget;
    GPUCommandListWidget* graphicsCommandsWidget;
    GraphicsBreakPointsWidget* graphicsBreakpointsWidget;
    GraphicsVertexShaderWidget* graphicsVertexShaderWidget;
    GraphicsTracingWidget* graphicsTracingWidget;
    IPCRecorderWidget* ipcRecorderWidget;
    LLEServiceModulesWidget* lleServiceModulesWidget;
    WaitTreeWidget* waitTreeWidget;
#if ENABLE_QT_UPDATER
    Updater* updater;
#endif

    bool explicit_update_check = false;
    bool defer_update_prompt = false;

    QAction* actions_recent_files[max_recent_files_item];
    std::array<QAction*, Core::SaveStateSlotCount> actions_load_state;
    std::array<QAction*, Core::SaveStateSlotCount> actions_save_state;

    u32 oldest_slot;
    u64 oldest_slot_time;
    u32 newest_slot;
    u64 newest_slot_time;

    // Secondary window actions
    QAction* action_secondary_fullscreen;
    QAction* action_secondary_toggle_screen;
    QAction* action_secondary_swap_screen;
    QAction* action_secondary_rotate_screen;

    QTranslator translator;

    // stores default icon theme search paths for the platform
    QStringList default_theme_paths;

    HotkeyRegistry hotkey_registry;

    std::shared_ptr<Camera::QtMultimediaCameraHandlerFactory> qt_cameras;

#ifdef __unix__
    QDBusObjectPath wake_lock{};
#endif

protected:
    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
};

class GApplicationEventFilter : public QObject {
    Q_OBJECT

signals:
    void FileOpen(const QFileOpenEvent* event);

protected:
    bool eventFilter(QObject* object, QEvent* event) override;
};

Q_DECLARE_METATYPE(std::size_t);
Q_DECLARE_METATYPE(Service::AM::InstallStatus);
