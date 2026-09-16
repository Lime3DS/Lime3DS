include(GenerateSCMRev)

add_library(citra_common STATIC EXCLUDE_FROM_ALL
    aarch64/cpu_detect.cpp
    aarch64/cpu_detect.h
    aarch64/oaknut_abi.h
    aarch64/oaknut_util.h
    alignment.h
    announce_multiplayer_room.h
    arch.h
    archives.h
    assert.h
    atomic_ops.h
    detached_tasks.cpp
    detached_tasks.h
    bit_field.h
    bit_field.natvis
    bit_set.h
    bounded_threadsafe_queue.h
    cityhash.cpp
    cityhash.h
    color.h
    common_funcs.h
    common_paths.h
    common_precompiled_headers.h
    common_types.h
    construct.h
    dynamic_library/dynamic_library.cpp
    dynamic_library/dynamic_library.h
    dynamic_library/ffmpeg.cpp
    dynamic_library/ffmpeg.h
    error.cpp
    error.h
    expected.h
    file_derived.cpp
    file_derived.h
    file_util.cpp
    file_util.h
    hash.h
    hacks/hack_list.h
    hacks/hack_list.cpp
    hacks/hack_manager.h
    hacks/hack_manager.cpp
    literals.h
    logging/backend.cpp
    logging/backend.h
    logging/filter.cpp
    logging/filter.h
    logging/formatter.h
    logging/log.h
    logging/log_entry.h
    logging/text_formatter.cpp
    logging/text_formatter.h
    logging/types.h
    math_util.cpp
    math_util.h
    memory_detect.cpp
    memory_detect.h
    memory_ref.h
    memory_ref.cpp
    microprofile.cpp
    microprofile.h
    microprofileui.h
    optional_helper.h
    param_package.cpp
    param_package.h
    play_time_manager.cpp
    play_time_manager.h
    polyfill_thread.h
    precompiled_headers.h
    quaternion.h
    ring_buffer.h
    scm_rev.cpp
    scm_rev.h
    scope_exit.h
    setting_keys.h
    settings.cpp
    settings.h
    slot_vector.h
    serialization/atomic.h
    serialization/boost_discrete_interval.hpp
    serialization/boost_flat_set.h
    serialization/boost_small_vector.hpp
    serialization/boost_std_variant.hpp
    serialization/boost_vector.hpp
    static_lru_cache.h
    string_literal.h
    string_util.cpp
    string_util.h
    swap.h
    texture.cpp
    texture.h
    thread.cpp
    thread.h
    thread_queue_list.h
    thread_worker.h
    threadsafe_queue.h
    timer.cpp
    timer.h
    unique_function.h
    vector_math.h
    web_util.cpp
    web_util.h
    x64/cpu_detect.cpp
    x64/cpu_detect.h
    x64/xbyak_abi.h
    x64/xbyak_util.h
    zstd_compression.cpp
    zstd_compression.h
)

target_include_directories(citra_common PUBLIC ${CMAKE_BINARY_DIR}/src)

if (UNIX AND NOT APPLE)
  target_sources(citra_common PRIVATE
    linux/gamemode.cpp
    linux/gamemode.h
  )

  target_link_libraries(citra_common PRIVATE gamemode)
endif()

if (APPLE AND NOT ENABLE_LIBRETRO)
  target_sources(citra_common PUBLIC
    apple_authorization.h
    apple_authorization.cpp
    apple_utils.cpp
    apple_utils.h
    apple_utils.mm
  )
endif()

# Android storage is only used for non-libretro Android builds
if (ANDROID AND NOT ENABLE_LIBRETRO)
  target_sources(citra_common PRIVATE
    android_utils.cpp
    android_utils.h
  )
endif()

if (MSVC)
  target_compile_options(citra_common PRIVATE
    /W4

    /we4242 # 'identifier': conversion from 'type1' to 'type2', possible loss of data
    /we4254 # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
    /we4800 # Implicit conversion from 'type' to bool. Possible information loss
  )
else()
  target_compile_options(citra_common PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-fsized-deallocation>
  )
endif()

create_target_directory_groups(citra_common)

target_link_libraries(citra_common PUBLIC fmt library-headers microprofile Boost::boost Boost::serialization Boost::iostreams)
target_link_libraries(citra_common PRIVATE cryptopp zstd)

if ("x86_64" IN_LIST ARCHITECTURE)
    target_link_libraries(citra_common PRIVATE xbyak)
endif()

if ("arm64" IN_LIST ARCHITECTURE)
    target_link_libraries(citra_common PRIVATE oaknut)
endif()

if (CITRA_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(citra_common PRIVATE precompiled_headers.h)
endif()

find_library(BACKTRACE_LIBRARY backtrace)
if (BACKTRACE_LIBRARY AND ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    target_link_libraries(citra_common PRIVATE ${BACKTRACE_LIBRARY} dl)
    target_compile_definitions(citra_common PRIVATE CITRA_LINUX_GCC_BACKTRACE)
endif()

if (ENABLE_RETRO_ACHIEVEMENTS)
    target_compile_definitions(citra_common PUBLIC ENABLE_RETRO_ACHIEVEMENTS)
endif()

target_link_libraries(citra_common PUBLIC xxHash::xxhash)