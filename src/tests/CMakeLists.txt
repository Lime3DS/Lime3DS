add_executable(tests
    common/bit_field.cpp
    common/file_util.cpp
    common/param_package.cpp
    core/core_timing.cpp
    core/file_sys/path_parser.cpp
    core/hle/kernel/hle_ipc.cpp
    core/memory/memory.cpp
    core/memory/vm_manager.cpp
    precompiled_headers.h
    audio_core/hle/hle.cpp
    audio_core/hle/source.cpp
    audio_core/lle/lle.cpp
    audio_core/audio_fixures.h
    audio_core/decoder_tests.cpp
    video_core/shader/shader_jit_compiler.cpp
    audio_core/merryhime_3ds_audio/merry_audio/merry_audio.cpp
    audio_core/merryhime_3ds_audio/merry_audio/merry_audio.h
    audio_core/merryhime_3ds_audio/merry_audio/service_fixture.cpp
    audio_core/merryhime_3ds_audio/merry_audio/service_fixture.h
    audio_core/merryhime_3ds_audio/audio_test_biquad_filter.cpp
)

create_target_directory_groups(tests)

target_link_libraries(tests PRIVATE lime_common lime_core video_core audio_core)
target_link_libraries(tests PRIVATE ${PLATFORM_LIBRARIES} catch2 nihstro-headers Threads::Threads)

add_test(NAME tests COMMAND tests)

if (LIME3DS_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(tests PRIVATE precompiled_headers.h)
endif()

# Bundle in-place on MSVC so dependencies can be resolved by builds.
if (MSVC)
    include(BundleTarget)
    bundle_target_in_place(tests)
endif()
