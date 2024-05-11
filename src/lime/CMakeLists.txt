set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

add_executable(lime
    lime.cpp
    lime.rc
    config.cpp
    config.h
    default_ini.h
    emu_window/emu_window_sdl2.cpp
    emu_window/emu_window_sdl2.h
    precompiled_headers.h
    resource.h
)

set_target_properties(lime PROPERTIES OUTPUT_NAME "lime3ds-cli")

if (ENABLE_SOFTWARE_RENDERER)
    target_sources(lime PRIVATE
        emu_window/emu_window_sdl2_sw.cpp
        emu_window/emu_window_sdl2_sw.h
    )
endif()
if (ENABLE_OPENGL)
    target_sources(lime PRIVATE
        emu_window/emu_window_sdl2_gl.cpp
        emu_window/emu_window_sdl2_gl.h
    )
endif()
if (ENABLE_VULKAN)
    target_sources(lime PRIVATE
        emu_window/emu_window_sdl2_vk.cpp
        emu_window/emu_window_sdl2_vk.h
    )
endif()

create_target_directory_groups(lime)

target_link_libraries(lime PRIVATE lime_common lime_core input_common network)
target_link_libraries(lime PRIVATE inih)
if (MSVC)
    target_link_libraries(lime PRIVATE getopt)
endif()
target_link_libraries(lime PRIVATE ${PLATFORM_LIBRARIES} SDL2::SDL2 Threads::Threads)

if (ENABLE_OPENGL)
    target_link_libraries(lime PRIVATE glad)
endif()

if(UNIX AND NOT APPLE)
    install(TARGETS lime RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (CITRA_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(lime PRIVATE precompiled_headers.h)
endif()

# Bundle in-place on MSVC so dependencies can be resolved by builds.
if (MSVC)
    include(BundleTarget)
    bundle_target_in_place(lime)
endif()
