set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

add_executable(lime-room
    precompiled_headers.h
    lime_room.cpp
    lime_room.rc
)

set_target_properties(lime-room PROPERTIES OUTPUT_NAME "lime3ds-room")

create_target_directory_groups(lime-room)

target_link_libraries(lime-room PRIVATE lime_common network)
if (ENABLE_WEB_SERVICE)
    target_link_libraries(lime-room PRIVATE web_service)
endif()

target_link_libraries(lime-room PRIVATE cryptopp)
if (MSVC)
    target_link_libraries(lime-room PRIVATE getopt)
endif()
target_link_libraries(lime-room PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

if(UNIX AND NOT APPLE)
    install(TARGETS lime-room RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (LIME3DS_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(lime-room PRIVATE precompiled_headers.h)
endif()

# Bundle in-place on MSVC so dependencies can be resolved by builds.
if (MSVC)
    include(BundleTarget)
    bundle_target_in_place(lime-room)
endif()
