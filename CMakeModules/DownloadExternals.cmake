
set(CURRENT_MODULE_DIR ${CMAKE_CURRENT_LIST_DIR})

# Determines parameters based on the host and target for downloading the right Qt binaries.
function(determine_qt_parameters target host_out type_out arch_out arch_path_out host_type_out host_arch_out host_arch_path_out)
    if (target MATCHES "tools_.*")
        set(tool ON)
    else()
        set(tool OFF)
    endif()

    # Determine installation parameters for OS, architecture, and compiler
    if (WIN32)
        set(host "windows")
        set(type "desktop")

        if (NOT tool)
            if (MINGW)
                set(arch "win64_mingw")
                set(arch_path "mingw_64")
            elseif (MSVC)
                if ("arm64" IN_LIST ARCHITECTURE)
                    set(arch_path "msvc2019_arm64")
                elseif ("x86_64" IN_LIST ARCHITECTURE)
                    set(arch_path "msvc2019_64")
                else()
                    message(FATAL_ERROR "Unsupported bundled Qt architecture. Enable USE_SYSTEM_QT and provide your own.")
                endif()
                set(arch "win64_${arch_path}")

                # In case we're cross-compiling, prepare to also fetch the correct host Qt tools.
                if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "AMD64")
                    set(host_arch_path "msvc2019_64")
                elseif (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "ARM64")
                    # TODO: msvc2019_arm64 doesn't include some of the required tools for some reason,
                    # TODO: so until it does, just use msvc2019_64 under x86_64 emulation.
                    # set(host_arch_path "msvc2019_arm64")
                    set(host_arch_path "msvc2019_64")
                endif()
                set(host_arch "win64_${host_arch_path}")
            else()
                message(FATAL_ERROR "Unsupported bundled Qt toolchain. Enable USE_SYSTEM_QT and provide your own.")
            endif()
        endif()
    elseif (APPLE)
        set(host "mac")
        set(type "desktop")
        set(arch "clang_64")
        set(arch_path "macos")
    else()
        set(host "linux")
        set(type "desktop")
        set(arch "gcc_64")
        set(arch_path "linux")
    endif()

    set(${host_out} "${host}" PARENT_SCOPE)
    set(${type_out} "${type}" PARENT_SCOPE)
    set(${arch_out} "${arch}" PARENT_SCOPE)
    set(${arch_path_out} "${arch_path}" PARENT_SCOPE)
    if (DEFINED host_type)
        set(${host_type_out} "${host_type}" PARENT_SCOPE)
    else()
        set(${host_type_out} "${type}" PARENT_SCOPE)
    endif()
    if (DEFINED host_arch)
        set(${host_arch_out} "${host_arch}" PARENT_SCOPE)
    else()
        set(${host_arch_out} "${arch}" PARENT_SCOPE)
    endif()
    if (DEFINED host_arch_path)
        set(${host_arch_path_out} "${host_arch_path}" PARENT_SCOPE)
    else()
        set(${host_arch_path_out} "${arch_path}" PARENT_SCOPE)
    endif()
endfunction()

# Download Qt binaries for a specifc configuration.
function(download_qt_configuration prefix_out target host type arch arch_path base_path)
    if (target MATCHES "tools_.*")
        set(tool ON)
    else()
        set(tool OFF)
    endif()

    set(install_args -c "${CURRENT_MODULE_DIR}/aqt_config.ini")
    if (tool)
        set(prefix "${base_path}/Tools")
        set(install_args ${install_args} install-tool --outputdir ${base_path} ${host} desktop ${target})
    else()
        set(prefix "${base_path}/${target}/${arch_path}")
        set(install_args ${install_args} install-qt --outputdir ${base_path} ${host} ${type} ${target} ${arch}
                -m qtmultimedia --archives qttranslations qttools qtsvg qtbase)
    endif()

    if (NOT EXISTS "${prefix}")
        message(STATUS "Downloading Qt binaries for ${target}:${host}:${type}:${arch}:${arch_path}")
        set(AQT_PREBUILD_BASE_URL "https://github.com/miurahr/aqtinstall/releases/download/v3.1.17")
        if (WIN32)
            set(aqt_path "${base_path}/aqt.exe")
            if (NOT EXISTS "${aqt_path}")
                file(DOWNLOAD
                        ${AQT_PREBUILD_BASE_URL}/aqt.exe
                        ${aqt_path} SHOW_PROGRESS)
            endif()
            execute_process(COMMAND ${aqt_path} ${install_args}
                    WORKING_DIRECTORY ${base_path})
        elseif (APPLE)
            set(aqt_path "${base_path}/aqt-macos")
            if (NOT EXISTS "${aqt_path}")
                file(DOWNLOAD
                        ${AQT_PREBUILD_BASE_URL}/aqt-macos
                        ${aqt_path} SHOW_PROGRESS)
            endif()
            execute_process(COMMAND chmod +x ${aqt_path})
            execute_process(COMMAND ${aqt_path} ${install_args}
                    WORKING_DIRECTORY ${base_path})
        else()
            # aqt does not offer binary releases for other platforms, so download and run from pip.
            set(aqt_install_path "${base_path}/aqt")
            file(MAKE_DIRECTORY "${aqt_install_path}")

            execute_process(COMMAND python3 -m pip install --target=${aqt_install_path} aqtinstall
                    WORKING_DIRECTORY ${base_path})
            execute_process(COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${aqt_install_path} python3 -m aqt ${install_args}
                    WORKING_DIRECTORY ${base_path})
        endif()

        message(STATUS "Downloaded Qt binaries for ${target}:${host}:${type}:${arch}:${arch_path} to ${prefix}")
    endif()

    set(${prefix_out} "${prefix}" PARENT_SCOPE)
endfunction()

# This function downloads Qt using aqt.
# The path of the downloaded content will be added to the CMAKE_PREFIX_PATH.
# QT_TARGET_PATH is set to the Qt for the compile target platform.
# QT_HOST_PATH is set to a host-compatible Qt, for running tools.
# Params:
#   target: Qt dependency to install. Specify a version number to download Qt, or "tools_(name)" for a specific build tool.
function(download_qt target)
    determine_qt_parameters("${target}" host type arch arch_path host_type host_arch host_arch_path)

    get_external_prefix(qt base_path)
    file(MAKE_DIRECTORY "${base_path}")

    download_qt_configuration(prefix "${target}" "${host}" "${type}" "${arch}" "${arch_path}" "${base_path}")
    if (DEFINED host_arch_path AND NOT "${host_arch_path}" STREQUAL "${arch_path}")
        download_qt_configuration(host_prefix "${target}" "${host}" "${host_type}" "${host_arch}" "${host_arch_path}" "${base_path}")
    else()
        set(host_prefix "${prefix}")
    endif()

    set(QT_TARGET_PATH "${prefix}" CACHE STRING "")
    set(QT_HOST_PATH "${host_prefix}" CACHE STRING "")

    # Add the target Qt prefix path so CMake can locate it.
    list(APPEND CMAKE_PREFIX_PATH "${prefix}")
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} PARENT_SCOPE)
endfunction()

function(download_moltenvk)
    set(MOLTENVK_PLATFORM "macOS")

    set(MOLTENVK_DIR "${CMAKE_BINARY_DIR}/externals/MoltenVK")
    set(MOLTENVK_TAR "${CMAKE_BINARY_DIR}/externals/MoltenVK.tar")
    if (NOT EXISTS ${MOLTENVK_DIR})
        if (NOT EXISTS ${MOLTENVK_TAR})
            file(DOWNLOAD https://github.com/KhronosGroup/MoltenVK/releases/download/v1.2.9/MoltenVK-all.tar
            ${MOLTENVK_TAR} SHOW_PROGRESS)
        endif()

        execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${MOLTENVK_TAR}"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/externals")
    endif()

    # Add the MoltenVK library path to the prefix so find_library can locate it.
    list(APPEND CMAKE_PREFIX_PATH "${MOLTENVK_DIR}/MoltenVK/dylib/${MOLTENVK_PLATFORM}")
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} PARENT_SCOPE)
endfunction()

function(get_external_prefix lib_name prefix_var)
    set(${prefix_var} "${CMAKE_BINARY_DIR}/externals/${lib_name}" PARENT_SCOPE)
endfunction()
