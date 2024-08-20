# Add a custom command to generate a new shader_cache_version hash when any of the following files change
# NOTE: This is an approximation of what files affect shader generation, its possible something else
# could affect the result, but much more unlikely than the following files. Keeping a list of files
# like this allows for much better caching since it doesn't force the user to recompile binary shaders every update
set(VIDEO_CORE "${CMAKE_SOURCE_DIR}/src/video_core")

# Pass the path to git to the GenerateSCMRev.cmake as well
find_package(Git QUIET)

add_custom_command(OUTPUT scm_rev.cpp
    COMMAND ${CMAKE_COMMAND}
      -DSRC_DIR="${CMAKE_SOURCE_DIR}"
      -DGIT_EXECUTABLE="${GIT_EXECUTABLE}"
      -P "${CMAKE_SOURCE_DIR}/CMakeModules/GenerateSCMRev.cmake"
    DEPENDS
      # WARNING! It was too much work to try and make a common location for this list,
      # so if you need to change it, please update CMakeModules/GenerateSCMRev.cmake as well
      "${VIDEO_CORE}/renderer_opengl/gl_shader_disk_cache.cpp"
      "${VIDEO_CORE}/renderer_opengl/gl_shader_disk_cache.h"
      "${VIDEO_CORE}/renderer_opengl/gl_shader_util.cpp"
      "${VIDEO_CORE}/renderer_opengl/gl_shader_util.h"
      "${VIDEO_CORE}/renderer_vulkan/vk_shader_util.cpp"
      "${VIDEO_CORE}/renderer_vulkan/vk_shader_util.h"
      "${VIDEO_CORE}/shader/generator/glsl_fs_shader_gen.cpp"
      "${VIDEO_CORE}/shader/generator/glsl_fs_shader_gen.h"
      "${VIDEO_CORE}/shader/generator/glsl_shader_decompiler.cpp"
      "${VIDEO_CORE}/shader/generator/glsl_shader_decompiler.h"
      "${VIDEO_CORE}/shader/generator/glsl_shader_gen.cpp"
      "${VIDEO_CORE}/shader/generator/glsl_shader_gen.h"
      "${VIDEO_CORE}/shader/generator/pica_fs_config.cpp"
      "${VIDEO_CORE}/shader/generator/pica_fs_config.h"
      "${VIDEO_CORE}/shader/generator/shader_gen.cpp"
      "${VIDEO_CORE}/shader/generator/shader_gen.h"
      "${VIDEO_CORE}/shader/generator/shader_uniforms.cpp"
      "${VIDEO_CORE}/shader/generator/shader_uniforms.h"
      "${VIDEO_CORE}/shader/generator/spv_fs_shader_gen.cpp"
      "${VIDEO_CORE}/shader/generator/spv_fs_shader_gen.h"
      "${VIDEO_CORE}/shader/shader.cpp"
      "${VIDEO_CORE}/shader/shader.h"
      "${VIDEO_CORE}/pica/regs_framebuffer.h"
      "${VIDEO_CORE}/pica/regs_lighting.h"
      "${VIDEO_CORE}/pica/regs_pipeline.h"
      "${VIDEO_CORE}/pica/regs_rasterizer.h"
      "${VIDEO_CORE}/pica/regs_shader.h"
      "${VIDEO_CORE}/pica/regs_texturing.h"
      "${VIDEO_CORE}/pica/regs_internal.cpp"
      "${VIDEO_CORE}/pica/regs_internal.h"
      # and also check that the scm_rev files haven't changed
      "${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp.in"
      "${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.h"
      # technically we should regenerate if the git version changed, but its not worth the effort imo
      "${CMAKE_SOURCE_DIR}/CMakeModules/GenerateSCMRev.cmake"
)

add_library(lime_common STATIC
    aarch64/cpu_detect.cpp
    aarch64/cpu_detect.h
    aarch64/oaknut_abi.h
    aarch64/oaknut_util.h
    alignment.h
    android_storage.h
    android_storage.cpp
    announce_multiplayer_room.h
    arch.h
    archives.h
    assert.h
    atomic_ops.h
    detached_tasks.cpp
    detached_tasks.h
    bit_field.h
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
    file_util.cpp
    file_util.h
    hash.h
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
    math_util.h
    memory_detect.cpp
    memory_detect.h
    memory_ref.h
    memory_ref.cpp
    microprofile.cpp
    microprofile.h
    microprofileui.h
    param_package.cpp
    param_package.h
    polyfill_thread.h
    precompiled_headers.h
    quaternion.h
    ring_buffer.h
    scm_rev.cpp
    scm_rev.h
    scope_exit.h
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
    telemetry.cpp
    telemetry.h
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
    web_result.h
    x64/cpu_detect.cpp
    x64/cpu_detect.h
    x64/xbyak_abi.h
    x64/xbyak_util.h
    zstd_compression.cpp
    zstd_compression.h
)

if (UNIX AND NOT APPLE)
  target_sources(lime_common PRIVATE
    linux/gamemode.cpp
    linux/gamemode.h
  )

  target_link_libraries(lime_common PRIVATE gamemode)
endif()

if (APPLE)
    target_sources(lime_common PUBLIC
        apple_authorization.h
        apple_authorization.cpp
    )
endif()

if (MSVC)
  target_compile_options(lime_common PRIVATE
    /W4

    /we4242 # 'identifier': conversion from 'type1' to 'type2', possible loss of data
    /we4254 # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
    /we4800 # Implicit conversion from 'type' to bool. Possible information loss
  )
else()
  target_compile_options(lime_common PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-fsized-deallocation>
  )
endif()

create_target_directory_groups(lime_common)

target_link_libraries(lime_common PUBLIC fmt library-headers microprofile Boost::boost Boost::serialization Boost::iostreams)
target_link_libraries(lime_common PRIVATE zstd)

if ("x86_64" IN_LIST ARCHITECTURE)
    target_link_libraries(lime_common PRIVATE xbyak)
endif()

if ("arm64" IN_LIST ARCHITECTURE)
    target_link_libraries(lime_common PRIVATE oaknut)
endif()

if (LIME3DS_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(lime_common PRIVATE precompiled_headers.h)
endif()

find_library(BACKTRACE_LIBRARY backtrace)
if (BACKTRACE_LIBRARY AND ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    target_link_libraries(lime_common PRIVATE ${BACKTRACE_LIBRARY} dl)
    target_compile_definitions(lime_common PRIVATE LIME3DS_LINUX_GCC_BACKTRACE)
endif()
