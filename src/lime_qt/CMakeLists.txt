set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)
if (POLICY CMP0071)
    cmake_policy(SET CMP0071 NEW)
endif()

add_executable(lime-qt
    aboutdialog.cpp
    aboutdialog.h
    aboutdialog.ui
    applets/mii_selector.cpp
    applets/mii_selector.h
    applets/swkbd.cpp
    applets/swkbd.h
    bootmanager.cpp
    bootmanager.h
    compatibility_list.cpp
    compatibility_list.h
    camera/camera_util.cpp
    camera/camera_util.h
    camera/still_image_camera.cpp
    camera/still_image_camera.h
    camera/qt_camera_base.cpp
    camera/qt_camera_base.h
    camera/qt_multimedia_camera.cpp
    camera/qt_multimedia_camera.h
    lime-qt.rc
    compatdb.cpp
    compatdb.h
    compatdb.ui
    configuration/config.cpp
    configuration/config.h
    configuration/configure.ui
    configuration/configuration_shared.cpp
    configuration/configuration_shared.h
    configuration/configure_audio.cpp
    configuration/configure_audio.h
    configuration/configure_audio.ui
    configuration/configure_camera.cpp
    configuration/configure_camera.h
    configuration/configure_camera.ui
    configuration/configure_debug.cpp
    configuration/configure_debug.h
    configuration/configure_debug.ui
    configuration/configure_enhancements.cpp
    configuration/configure_enhancements.h
    configuration/configure_enhancements.ui
    configuration/configure_layout.cpp
    configuration/configure_layout.h
    configuration/configure_layout.ui
    configuration/configure_dialog.cpp
    configuration/configure_dialog.h
    configuration/configure_general.cpp
    configuration/configure_general.h
    configuration/configure_general.ui
    configuration/configure_graphics.cpp
    configuration/configure_graphics.h
    configuration/configure_graphics.ui
    configuration/configure_hotkeys.cpp
    configuration/configure_hotkeys.h
    configuration/configure_hotkeys.ui
    configuration/configure_input.cpp
    configuration/configure_input.h
    configuration/configure_input.ui
    configuration/configure_motion_touch.cpp
    configuration/configure_motion_touch.h
    configuration/configure_motion_touch.ui
    configuration/configure_per_game.cpp
    configuration/configure_per_game.h
    configuration/configure_per_game.ui
    configuration/configure_storage.cpp
    configuration/configure_storage.h
    configuration/configure_storage.ui
    configuration/configure_system.cpp
    configuration/configure_system.h
    configuration/configure_system.ui
    configuration/configure_touch_from_button.cpp
    configuration/configure_touch_from_button.h
    configuration/configure_touch_from_button.ui
    configuration/configure_touch_widget.h
    configuration/configure_ui.cpp
    configuration/configure_ui.h
    configuration/configure_ui.ui
    configuration/configure_web.cpp
    configuration/configure_web.h
    configuration/configure_web.ui
    configuration/configure_cheats.cpp
    configuration/configure_cheats.h
    configuration/configure_cheats.ui
    debugger/console.h
    debugger/console.cpp
    debugger/graphics/graphics.cpp
    debugger/graphics/graphics.h
    debugger/graphics/graphics_breakpoint_observer.cpp
    debugger/graphics/graphics_breakpoint_observer.h
    debugger/graphics/graphics_breakpoints.cpp
    debugger/graphics/graphics_breakpoints.h
    debugger/graphics/graphics_breakpoints_p.h
    debugger/graphics/graphics_cmdlists.cpp
    debugger/graphics/graphics_cmdlists.h
    debugger/graphics/graphics_surface.cpp
    debugger/graphics/graphics_surface.h
    debugger/graphics/graphics_tracing.cpp
    debugger/graphics/graphics_tracing.h
    debugger/graphics/graphics_vertex_shader.cpp
    debugger/graphics/graphics_vertex_shader.h
    debugger/ipc/record_dialog.cpp
    debugger/ipc/record_dialog.h
    debugger/ipc/record_dialog.ui
    debugger/ipc/recorder.cpp
    debugger/ipc/recorder.h
    debugger/ipc/recorder.ui
    debugger/lle_service_modules.cpp
    debugger/lle_service_modules.h
    debugger/profiler.cpp
    debugger/profiler.h
    debugger/registers.cpp
    debugger/registers.h
    debugger/registers.ui
    debugger/wait_tree.cpp
    debugger/wait_tree.h
    discord.h
    dumping/dumping_dialog.cpp
    dumping/dumping_dialog.h
    dumping/dumping_dialog.ui
    dumping/option_set_dialog.cpp
    dumping/option_set_dialog.h
    dumping/option_set_dialog.ui
    dumping/options_dialog.cpp
    dumping/options_dialog.h
    dumping/options_dialog.ui
    game_list.cpp
    game_list.h
    game_list_p.h
    game_list_worker.cpp
    game_list_worker.h
    hotkeys.cpp
    hotkeys.h
    loading_screen.cpp
    loading_screen.h
    loading_screen.ui
    main.cpp
    main.h
    main.ui
    movie/movie_play_dialog.cpp
    movie/movie_play_dialog.h
    movie/movie_play_dialog.ui
    movie/movie_record_dialog.cpp
    movie/movie_record_dialog.h
    movie/movie_record_dialog.ui
    multiplayer/chat_room.cpp
    multiplayer/chat_room.h
    multiplayer/chat_room.ui
    multiplayer/client_room.h
    multiplayer/client_room.cpp
    multiplayer/client_room.ui
    multiplayer/direct_connect.cpp
    multiplayer/direct_connect.h
    multiplayer/direct_connect.ui
    multiplayer/host_room.cpp
    multiplayer/host_room.h
    multiplayer/host_room.ui
    multiplayer/lobby.cpp
    multiplayer/lobby.h
    multiplayer/lobby.ui
    multiplayer/lobby_p.h
    multiplayer/message.cpp
    multiplayer/message.h
    multiplayer/moderation_dialog.cpp
    multiplayer/moderation_dialog.h
    multiplayer/moderation_dialog.ui
    multiplayer/state.cpp
    multiplayer/state.h
    multiplayer/validation.h
    play_time_manager.cpp
    play_time_manager.h
    precompiled_headers.h
    uisettings.cpp
    uisettings.h
    qt_image_interface.cpp
    qt_image_interface.h
    util/clickable_label.cpp
    util/clickable_label.h
    util/graphics_device_info.cpp
    util/graphics_device_info.h
    util/sequence_dialog/sequence_dialog.cpp
    util/sequence_dialog/sequence_dialog.h
    util/spinbox.cpp
    util/spinbox.h
    util/util.cpp
    util/util.h
)

set_target_properties(lime-qt PROPERTIES OUTPUT_NAME "lime3ds-gui")

file(GLOB COMPAT_LIST
     ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.qrc
     ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.json)
file(GLOB_RECURSE ICONS ${PROJECT_SOURCE_DIR}/dist/icons/*)
file(GLOB_RECURSE THEMES ${PROJECT_SOURCE_DIR}/dist/qt_themes/*)

if (ENABLE_QT_UPDATER)
    target_sources(lime-qt PRIVATE
        updater/updater.cpp
        updater/updater.h
        updater/updater_p.h
    )
    target_compile_definitions(lime-qt PUBLIC ENABLE_QT_UPDATER)
endif()

if (ENABLE_QT_TRANSLATION)
    set(CITRA_QT_LANGUAGES "${PROJECT_SOURCE_DIR}/dist/languages" CACHE PATH "Path to the translation bundle for the Qt frontend")
    option(GENERATE_QT_TRANSLATION "Generate en.ts as the translation source file" OFF)

    # Update source TS file if enabled
    if (GENERATE_QT_TRANSLATION)
        get_target_property(QT_SRCS lime-qt SOURCES)
        get_target_property(QT_INCLUDES lime-qt INCLUDE_DIRECTORIES)
        qt_add_lupdate(lime-qt TS_FILES ${CITRA_QT_LANGUAGES}/en.ts
            SOURCES ${QT_SRCS} ${UIS}
            INCLUDE_DIRECTORIES ${QT_INCLUDES}
            NO_GLOBAL_TARGET)
        add_custom_target(translation ALL DEPENDS lime-qt_lupdate)
    endif()

    # Find all TS files except en.ts
    file(GLOB_RECURSE LANGUAGES_TS ${CITRA_QT_LANGUAGES}/*.ts)
    list(REMOVE_ITEM LANGUAGES_TS ${CITRA_QT_LANGUAGES}/en.ts)

    # Compile TS files to QM files
    qt_add_lrelease(lime-qt TS_FILES ${LANGUAGES_TS} NO_GLOBAL_TARGET QM_FILES_OUTPUT_VARIABLE LANGUAGES_QM)

    # Build a QRC file from the QM file list
    set(LANGUAGES_QRC ${CMAKE_CURRENT_BINARY_DIR}/languages.qrc)
    file(WRITE ${LANGUAGES_QRC} "<RCC><qresource prefix=\"languages\">\n")
    foreach (QM ${LANGUAGES_QM})
        get_filename_component(QM_FILE ${QM} NAME)
        file(APPEND ${LANGUAGES_QRC} "<file>${QM_FILE}</file>\n")
    endforeach (QM)
    file(APPEND ${LANGUAGES_QRC} "</qresource></RCC>")

    # Add the QRC file to package in all QM files
    qt_add_resources(LANGUAGES ${LANGUAGES_QRC})
else()
    set(LANGUAGES)
endif()

target_sources(lime-qt
    PRIVATE
        ${COMPAT_LIST}
        ${ICONS}
        ${LANGUAGES}
        ${THEMES}
)

if (APPLE)
    set(DIST_DIR "../../dist/apple")
    set(APPLE_RESOURCES
        "${DIST_DIR}/lime.icns"
        "${DIST_DIR}/LaunchScreen.storyboard"
        "${DIST_DIR}/launch_logo.png"
    )
    target_sources(lime-qt PRIVATE ${APPLE_RESOURCES})

    # Define app bundle metadata.
    include(GenerateBuildInfo)
    set_target_properties(lime-qt PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${DIST_DIR}/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "Lime3DS"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.lime3ds.Lime3DS"
        MACOSX_BUNDLE_BUNDLE_VERSION "${BUILD_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${BUILD_FULLNAME}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${BUILD_FULLNAME}"
        MACOSX_BUNDLE_ICON_FILE "lime.icns"
        RESOURCE "${APPLE_RESOURCES}"
    )
elseif(WIN32)
    # compile as a win32 gui application instead of a console application
    target_link_libraries(lime-qt PRIVATE Qt6::EntryPointImplementation)
    if(MSVC)
        set_target_properties(lime-qt PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
    elseif(MINGW)
        set_target_properties(lime-qt PROPERTIES LINK_FLAGS_RELEASE "-mwindows")
    endif()
endif()

if(ENABLE_SDL2)
    target_link_libraries(lime-qt PRIVATE SDL2::SDL2)
    target_compile_definitions(lime-qt PRIVATE HAVE_SDL2)
endif()

create_target_directory_groups(lime-qt)

target_link_libraries(lime-qt PRIVATE audio_core lime_common lime_core input_common network video_core)
target_link_libraries(lime-qt PRIVATE Boost::boost nihstro-headers Qt6::Widgets Qt6::Multimedia Qt6::Concurrent)
target_link_libraries(lime-qt PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

if (ENABLE_OPENGL)
    target_link_libraries(lime-qt PRIVATE glad)
endif()

if (ENABLE_VULKAN)
    target_link_libraries(lime-qt PRIVATE vulkan-headers)
endif()

if (NOT WIN32)
    target_include_directories(lime-qt PRIVATE ${Qt6Gui_PRIVATE_INCLUDE_DIRS})
endif()

if (UNIX AND NOT APPLE)
    target_link_libraries(lime-qt PRIVATE Qt6::DBus gamemode)
endif()

target_compile_definitions(lime-qt PRIVATE
    # Use QStringBuilder for string concatenation to reduce
    # the overall number of temporary strings created.
    -DQT_USE_QSTRINGBUILDER

    # Disable implicit type narrowing in signal/slot connect() calls.
    -DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT

    # Disable unsafe overloads of QProcess' start() function.
    -DQT_NO_PROCESS_COMBINED_ARGUMENT_START

    # Disable implicit QString->QUrl conversions to enforce use of proper resolving functions.
    -DQT_NO_URL_CAST_FROM_STRING

    # Disable implicit conversions from/to C strings
    -DQT_NO_CAST_FROM_ASCII
    -DQT_NO_CAST_TO_ASCII
)

if (CITRA_ENABLE_COMPATIBILITY_REPORTING)
    target_compile_definitions(lime-qt PRIVATE -DCITRA_ENABLE_COMPATIBILITY_REPORTING)
endif()

if (USE_DISCORD_PRESENCE)
    target_sources(lime-qt PUBLIC
        discord_impl.cpp
        discord_impl.h
    )
    target_link_libraries(lime-qt PRIVATE discord-rpc)
    target_compile_definitions(lime-qt PRIVATE -DUSE_DISCORD_PRESENCE)
endif()

if (ENABLE_WEB_SERVICE)
    target_link_libraries(lime-qt PRIVATE web_service)
endif()

if(UNIX AND NOT APPLE)
    install(TARGETS lime-qt RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (CITRA_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(lime-qt PRIVATE precompiled_headers.h)
endif()

# Bundle in-place on MSVC so dependencies can be resolved by builds.
if (MSVC)
    include(BundleTarget)
    bundle_target_in_place(lime-qt)
endif()
