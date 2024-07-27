
if(WIN32)
    set(PLATFORM "windows")
elseif(APPLE)
    set(PLATFORM "mac")
elseif(UNIX)
    set(PLATFORM "linux")
else()
    message(FATAL_ERROR "Cannot build installer for this unsupported platform")
endif()

set(BUILD_DIR "${CMAKE_BINARY_DIR}/installer")
set(DIST_DIR "${BUILD_DIR}/dist")
set(TARGET_FILE "${DIST_DIR}/lime3ds-setup-${PLATFORM}")

file(MAKE_DIRECTORY "${BUILD_DIR}" "${DIST_DIR}")

# Adds a custom target that will run the BuildInstaller.cmake file
# CMake can't just run a cmake function as a custom command, so this is a way around it.
# Calls the cmake command and runs a cmake file in "scripting" mode passing in variables with -D
add_custom_command(OUTPUT "${TARGET_FILE}"
    COMMAND ${CMAKE_COMMAND}
        -DBASE_DIR=${CMAKE_SOURCE_DIR}
        -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DBUILD_DIR=${BUILD_DIR}
        -DTARGET_FILE=${TARGET_FILE}
        -P ${CMAKE_SOURCE_DIR}/CMakeModules/BuildInstaller.cmake
    WORKING_DIRECTORY ${BUILD_DIR}
)

add_custom_target(installer DEPENDS ${TARGET_FILE})
