
if (BUNDLE_TARGET_EXECUTE)
    # --- Bundling method logic ---

    function(symlink_safe_copy from to)
        if (WIN32)
            # Use cmake copy for maximum compatibility.
            execute_process(COMMAND ${CMAKE_COMMAND} -E copy "${from}" "${to}"
                RESULT_VARIABLE cp_result)
        else()
            # Use native copy to turn symlinks into normal files.
            execute_process(COMMAND cp -L "${from}" "${to}"
                RESULT_VARIABLE cp_result)
        endif()
        if (NOT cp_result EQUAL "0")
            message(FATAL_ERROR "cp \"${from}\" \"${to}\" failed: ${cp_result}")
        endif()
    endfunction()

    function(bundle_qt executable_path)
        if (WIN32)
            # Perform standalone bundling first to copy over all used libraries, as windeployqt does not do this.
            bundle_standalone("${executable_path}" "${EXECUTABLE_PATH}" "${BUNDLE_LIBRARY_PATHS}")

            get_filename_component(executable_parent_dir "${executable_path}" DIRECTORY)

            # Create a qt.conf file pointing to the app directory.
            # This ensures Qt can find its plugins.
            file(WRITE "${executable_parent_dir}/qt.conf" "[Paths]\nPrefix = .")

            find_program(windeployqt_executable windeployqt6 PATHS "${QT_HOST_PATH}/bin")
            find_program(qtpaths_executable qtpaths6 PATHS "${QT_HOST_PATH}/bin")

            # TODO: Hack around windeployqt's poor cross-compilation support by
            # TODO: making a local copy with a prefix pointing to the target Qt.
            if (NOT "${QT_HOST_PATH}" STREQUAL "${QT_TARGET_PATH}")
                set(windeployqt_dir "${BINARY_PATH}/windeployqt_copy")
                file(MAKE_DIRECTORY "${windeployqt_dir}")
                symlink_safe_copy("${windeployqt_executable}" "${windeployqt_dir}/windeployqt.exe")
                symlink_safe_copy("${qtpaths_executable}" "${windeployqt_dir}/qtpaths.exe")
                symlink_safe_copy("${QT_HOST_PATH}/bin/Qt6Core.dll" "${windeployqt_dir}")

                if (EXISTS "${QT_TARGET_PATH}/share")
                    # Unix-style Qt; we need to wire up the paths manually.
                    file(WRITE "${windeployqt_dir}/qt.conf" "\
                        [Paths]\n
                        Prefix = ${QT_TARGET_PATH}\n \
                        ArchData = ${QT_TARGET_PATH}/share/qt6\n \
                        Binaries = ${QT_TARGET_PATH}/bin\n \
                        Data = ${QT_TARGET_PATH}/share/qt6\n \
                        Documentation = ${QT_TARGET_PATH}/share/qt6/doc\n \
                        Headers = ${QT_TARGET_PATH}/include/qt6\n \
                        Libraries = ${QT_TARGET_PATH}/lib\n \
                        LibraryExecutables = ${QT_TARGET_PATH}/share/qt6/bin\n \
                        Plugins = ${QT_TARGET_PATH}/share/qt6/plugins\n \
                        QmlImports = ${QT_TARGET_PATH}/share/qt6/qml\n \
                        Translations = ${QT_TARGET_PATH}/share/qt6/translations\n \
                    ")
                else()
                    # Windows-style Qt; the defaults should suffice.
                    file(WRITE "${windeployqt_dir}/qt.conf" "[Paths]\nPrefix = ${QT_TARGET_PATH}")
                endif()

                set(windeployqt_executable "${windeployqt_dir}/windeployqt.exe")
                set(qtpaths_executable "${windeployqt_dir}/qtpaths.exe")
            endif()

            message(STATUS "Executing windeployqt for executable ${executable_path}")
            execute_process(COMMAND "${windeployqt_executable}" "${executable_path}"
                --qtpaths "${qtpaths_executable}"
                --no-compiler-runtime --no-system-d3d-compiler --no-opengl-sw --no-translations
                --plugindir "${executable_parent_dir}/plugins"
                RESULT_VARIABLE windeployqt_result)
            if (NOT windeployqt_result EQUAL "0")
                message(FATAL_ERROR "windeployqt failed: ${windeployqt_result}")
            endif()

            # Remove the FFmpeg multimedia plugin as we don't include FFmpeg.
            # We want to use the Windows media plugin instead, which is also included.
            file(REMOVE "${executable_parent_dir}/plugins/multimedia/ffmpegmediaplugin.dll")
        elseif (APPLE)
            get_filename_component(executable_name "${executable_path}" NAME_WE)
            find_program(macdeployqt_executable macdeployqt6 PATHS "${QT_HOST_PATH}/bin")

            message(STATUS "Executing macdeployqt at \"${macdeployqt_executable}\" for executable \"${executable_path}\"")
            execute_process(
                COMMAND "${macdeployqt_executable}"
                "${executable_path}"
                "-executable=${executable_path}/Contents/MacOS/${executable_name}"
                -always-overwrite
                RESULT_VARIABLE macdeployqt_result)
            if (NOT macdeployqt_result EQUAL "0")
                message(FATAL_ERROR "macdeployqt failed: ${macdeployqt_result}")
            endif()

            # Bundling libraries can rewrite path information and break code signatures of system libraries.
            # Perform an ad-hoc re-signing on the whole app bundle to fix this.
            execute_process(COMMAND codesign --deep -fs - "${executable_path}"
                            RESULT_VARIABLE codesign_result)
            if (NOT codesign_result EQUAL "0")
                message(FATAL_ERROR "codesign failed: ${codesign_result}")
            endif()
        else()
            message(FATAL_ERROR "Unsupported OS for Qt bundling.")
        endif()
    endfunction()

    function(bundle_appimage bundle_dir executable_path source_path binary_path linuxdeploy_executable enable_qt)
        get_filename_component(executable_name "${executable_path}" NAME_WE)
        set(appdir_path "${binary_path}/AppDir-${executable_name}")

        if (enable_qt)
            # Find qmake to make sure the plugin uses the right version of Qt.
            find_program(qmake_executable qmake6 PATHS "${QT_HOST_PATH}/bin")

            set(extra_linuxdeploy_env "QMAKE=${qmake_executable}")
            set(extra_linuxdeploy_args --plugin qt)
        endif()

        message(STATUS "Creating AppDir for executable ${executable_path}")
        execute_process(COMMAND ${CMAKE_COMMAND} -E env
            ${extra_linuxdeploy_env}
            "${linuxdeploy_executable}"
            ${extra_linuxdeploy_args}
            --plugin checkrt
            --executable "${executable_path}"
            --icon-file "${source_path}/dist/lime.svg"
            --desktop-file "${source_path}/dist/${executable_name}.desktop"
            --appdir "${appdir_path}"
            RESULT_VARIABLE linuxdeploy_appdir_result)
        if (NOT linuxdeploy_appdir_result EQUAL "0")
            message(FATAL_ERROR "linuxdeploy failed to create AppDir: ${linuxdeploy_appdir_result}")
        endif()

        if (enable_qt)
            set(qt_hook_file "${appdir_path}/apprun-hooks/linuxdeploy-plugin-qt-hook.sh")
            file(READ "${qt_hook_file}" qt_hook_contents)
            # Add Cinnamon to list of DEs for GTK3 theming.
            string(REPLACE
                "*XFCE*"
                "*X-Cinnamon*|*XFCE*"
                qt_hook_contents "${qt_hook_contents}")
            # Wayland backend crashes due to changed schemas in Gnome 40.
            string(REPLACE
                "export QT_QPA_PLATFORMTHEME=gtk3"
                "export QT_QPA_PLATFORMTHEME=gtk3; export GDK_BACKEND=x11"
                qt_hook_contents "${qt_hook_contents}")
            file(WRITE "${qt_hook_file}" "${qt_hook_contents}")
        endif()

        message(STATUS "Creating AppImage for executable ${executable_path}")
        execute_process(COMMAND ${CMAKE_COMMAND} -E env
            "OUTPUT=${bundle_dir}/${executable_name}.AppImage"
            "${linuxdeploy_executable}"
            --output appimage
            --appdir "${appdir_path}"
            RESULT_VARIABLE linuxdeploy_appimage_result)
        if (NOT linuxdeploy_appimage_result EQUAL "0")
            message(FATAL_ERROR "linuxdeploy failed to create AppImage: ${linuxdeploy_appimage_result}")
        endif()
    endfunction()

    function(bundle_standalone executable_path original_executable_path bundle_library_paths)
        get_filename_component(executable_parent_dir "${executable_path}" DIRECTORY)

        # Resolve dependent library files if they were not passed in.
        message(STATUS "Determining runtime dependencies of ${executable_path} using library paths ${bundle_library_paths}")
        file(GET_RUNTIME_DEPENDENCIES
                EXECUTABLES ${original_executable_path}
                RESOLVED_DEPENDENCIES_VAR resolved_deps
                UNRESOLVED_DEPENDENCIES_VAR unresolved_deps
                DIRECTORIES ${bundle_library_paths}
                POST_EXCLUDE_REGEXES ".*system32.*")

        if (WIN32)
            # Same directory since we don't have rpath.
            set(lib_dir "${executable_parent_dir}")
        else()
            set(lib_dir "${executable_parent_dir}/libs")
        endif()

        # Copy files to bundled output.
        if (resolved_deps)
            file(MAKE_DIRECTORY ${lib_dir})
            foreach (lib_file IN LISTS resolved_deps)
                message(STATUS "Bundling library ${lib_file}")
                symlink_safe_copy("${lib_file}" "${lib_dir}")
            endforeach()
        endif()

        # Add libs directory to executable rpath where applicable.
        if (APPLE)
            execute_process(COMMAND install_name_tool -add_rpath "@loader_path/libs" "${executable_path}"
                            RESULT_VARIABLE install_name_tool_result)
            if (NOT install_name_tool_result EQUAL "0")
                message(FATAL_ERROR "install_name_tool failed: ${install_name_tool_result}")
            endif()
        elseif (UNIX)
            execute_process(COMMAND patchelf --set-rpath '$ORIGIN/../libs' "${executable_path}"
                            RESULT_VARIABLE patchelf_result)
            if (NOT patchelf_result EQUAL "0")
                message(FATAL_ERROR "patchelf failed: ${patchelf_result}")
            endif()
        endif()
    endfunction()

    # --- Root bundling logic ---

    set(bundle_dir ${BINARY_PATH}/bundle)

    # On Linux, always bundle an AppImage.
    if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        if (IN_PLACE)
            message(FATAL_ERROR "Cannot bundle for Linux in-place.")
        endif()

        bundle_appimage("${bundle_dir}" "${EXECUTABLE_PATH}" "${SOURCE_PATH}" "${BINARY_PATH}" "${LINUXDEPLOY}" ${BUNDLE_QT})
    else()
        if (IN_PLACE)
            message(STATUS "Bundling dependencies in-place")
            set(bundled_executable_path "${EXECUTABLE_PATH}")
        else()
            message(STATUS "Copying base executable ${EXECUTABLE_PATH} to output directory ${bundle_dir}")
            file(COPY ${EXECUTABLE_PATH} DESTINATION ${bundle_dir})
            get_filename_component(bundled_executable_name "${EXECUTABLE_PATH}" NAME)
            set(bundled_executable_path "${bundle_dir}/${bundled_executable_name}")
        endif()

        if (BUNDLE_QT)
            bundle_qt("${bundled_executable_path}")
        else()
            bundle_standalone("${bundled_executable_path}" "${EXECUTABLE_PATH}" "${BUNDLE_LIBRARY_PATHS}")
        endif()
    endif()
elseif (BUNDLE_TARGET_DOWNLOAD_LINUXDEPLOY)
    # --- linuxdeploy download logic ---

    # Downloads and extracts a linuxdeploy component.
    function(download_linuxdeploy_component base_dir name executable_name)
        set(executable_file "${base_dir}/${executable_name}")
        if (NOT EXISTS "${executable_file}")
            message(STATUS "Downloading ${executable_name}")
            file(DOWNLOAD
                "https://github.com/${name}/releases/download/continuous/${executable_name}"
                "${executable_file}" SHOW_PROGRESS)
            file(CHMOD "${executable_file}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

            get_filename_component(executable_ext "${executable_file}" LAST_EXT)
            if (executable_ext STREQUAL ".AppImage")
                message(STATUS "Extracting ${executable_name}")
                execute_process(
                    COMMAND "${executable_file}" --appimage-extract
                    WORKING_DIRECTORY "${base_dir}"
                    RESULT_VARIABLE extract_result)
                if (NOT extract_result EQUAL "0")
                    message(FATAL_ERROR "AppImage extract failed: ${extract_result}")
                endif()
            else()
                message(STATUS "Copying ${executable_name}")
                file(COPY "${executable_file}" DESTINATION "${base_dir}/squashfs-root/usr/bin/")
            endif()
        endif()
    endfunction()

    # Download plugins first so they don't overwrite linuxdeploy's AppRun file.
    download_linuxdeploy_component("${LINUXDEPLOY_PATH}" "linuxdeploy/linuxdeploy-plugin-qt" "linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage")
    download_linuxdeploy_component("${LINUXDEPLOY_PATH}" "darealshinji/linuxdeploy-plugin-checkrt" "linuxdeploy-plugin-checkrt.sh")
    download_linuxdeploy_component("${LINUXDEPLOY_PATH}" "linuxdeploy/linuxdeploy" "linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage")
else()
    # --- Bundling target creation logic ---

    # Creates the base bundle target with common files and pre-bundle steps.
    function(create_base_bundle_target)
        message(STATUS "Creating base bundle target")

        add_custom_target(bundle)
        add_custom_command(
            TARGET bundle
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bundle/")
        add_custom_command(
            TARGET bundle
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bundle/dist/")
        add_custom_command(
            TARGET bundle
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/dist/icon.png" "${CMAKE_BINARY_DIR}/bundle/dist/lime.png")
        add_custom_command(
            TARGET bundle
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/license.txt" "${CMAKE_BINARY_DIR}/bundle/")
        add_custom_command(
            TARGET bundle
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/README.md" "${CMAKE_BINARY_DIR}/bundle/")
        add_custom_command(
            TARGET bundle
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/dist/scripting" "${CMAKE_BINARY_DIR}/bundle/scripting")

        # On Linux, add a command to prepare linuxdeploy and any required plugins before any bundling occurs.
        if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
            add_custom_command(
                TARGET bundle
                COMMAND ${CMAKE_COMMAND}
                "-DBUNDLE_TARGET_DOWNLOAD_LINUXDEPLOY=1"
                "-DLINUXDEPLOY_PATH=${CMAKE_BINARY_DIR}/externals/linuxdeploy"
                "-DLINUXDEPLOY_ARCH=${CMAKE_HOST_SYSTEM_PROCESSOR}"
                -P "${CMAKE_SOURCE_DIR}/CMakeModules/BundleTarget.cmake"
                WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
        endif()
    endfunction()

    # Adds a target to the bundle target, packing in required libraries.
    # If in_place is true, the bundling will be done in-place as part of the specified target.
    function(bundle_target_internal target_name in_place)
        # Create base bundle target if it does not exist.
        if (NOT in_place AND NOT TARGET bundle)
            create_base_bundle_target()
        endif()

        set(bundle_executable_path "$<TARGET_FILE:${target_name}>")
        if (target_name MATCHES ".*qt")
            set(bundle_qt ON)
            if (APPLE)
                # For Qt targets on Apple, expect an app bundle.
                set(bundle_executable_path "$<TARGET_BUNDLE_DIR:${target_name}>")
            endif()
        else()
            set(bundle_qt OFF)
        endif()

        # Build a list of library search paths from prefix paths.
        foreach(prefix_path IN LISTS CMAKE_FIND_ROOT_PATH CMAKE_PREFIX_PATH CMAKE_SYSTEM_PREFIX_PATH)
            if (WIN32)
                list(APPEND bundle_library_paths "${prefix_path}/bin")
            endif()
            list(APPEND bundle_library_paths "${prefix_path}/lib")
        endforeach()
        foreach(library_path IN LISTS CMAKE_SYSTEM_LIBRARY_PATH)
            list(APPEND bundle_library_paths "${library_path}")
        endforeach()

        if (in_place)
            message(STATUS "Adding in-place bundling to ${target_name}")
            set(dest_target ${target_name})
        else()
            message(STATUS "Adding ${target_name} to bundle target")
            set(dest_target bundle)
            add_dependencies(bundle ${target_name})
        endif()

        add_custom_command(TARGET ${dest_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
            "-DQT_HOST_PATH=\"${QT_HOST_PATH}\""
            "-DQT_TARGET_PATH=\"${QT_TARGET_PATH}\""
            "-DBUNDLE_TARGET_EXECUTE=1"
            "-DTARGET=${target_name}"
            "-DSOURCE_PATH=${CMAKE_SOURCE_DIR}"
            "-DBINARY_PATH=${CMAKE_BINARY_DIR}"
            "-DEXECUTABLE_PATH=${bundle_executable_path}"
            "-DBUNDLE_LIBRARY_PATHS=\"${bundle_library_paths}\""
            "-DBUNDLE_QT=${bundle_qt}"
            "-DIN_PLACE=${in_place}"
            "-DLINUXDEPLOY=${CMAKE_BINARY_DIR}/externals/linuxdeploy/squashfs-root/AppRun"
            -P "${CMAKE_SOURCE_DIR}/CMakeModules/BundleTarget.cmake"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    endfunction()

    # Adds a target to the bundle target, packing in required libraries.
    function(bundle_target target_name)
        bundle_target_internal("${target_name}" OFF)
    endfunction()

    # Bundles the target in-place, packing in required libraries.
    function(bundle_target_in_place target_name)
        bundle_target_internal("${target_name}" ON)
    endfunction()
endif()
