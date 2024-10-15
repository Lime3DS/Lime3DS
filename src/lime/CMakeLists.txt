add_executable(lime
    lime.rc
    main.cpp
)

set_target_properties(lime PROPERTIES OUTPUT_NAME "lime3ds")

if (ENABLE_SDL2_FRONTEND)
    target_link_libraries(lime PRIVATE lime_sdl)
endif()

if (ENABLE_QT)
    target_link_libraries(lime PRIVATE lime_qt)
    target_link_libraries(lime PRIVATE Qt6::Widgets Qt6::DBus)
endif()

# Bundle in-place on MSVC so dependencies can be resolved by builds.
if (ENABLE_QT AND MSVC)
    include(BundleTarget)
    bundle_target_in_place(lime)
endif()
