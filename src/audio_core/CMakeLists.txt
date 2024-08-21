add_library(audio_core STATIC
    audio_types.h
    codec.cpp
    codec.h
    dsp_interface.cpp
    dsp_interface.h
    hle/aac_decoder.cpp
    hle/aac_decoder.h
    hle/common.h
    hle/decoder.cpp
    hle/decoder.h
    hle/filter.cpp
    hle/filter.h
    hle/hle.cpp
    hle/hle.h
    hle/mixers.cpp
    hle/mixers.h
    hle/shared_memory.h
    hle/source.cpp
    hle/source.h
    lle/lle.cpp
    lle/lle.h
    input.h
    input_details.cpp
    input_details.h
    interpolate.cpp
    interpolate.h
    null_input.h
    null_sink.h
    precompiled_headers.h
    sink.h
    sink_details.cpp
    sink_details.h
    static_input.cpp
    static_input.h
    time_stretch.cpp
    time_stretch.h

    $<$<BOOL:${ENABLE_SDL2}>:sdl2_sink.cpp sdl2_sink.h>
    $<$<BOOL:${ENABLE_CUBEB}>:cubeb_sink.cpp cubeb_sink.h cubeb_input.cpp cubeb_input.h>
    $<$<BOOL:${ENABLE_OPENAL}>:openal_input.cpp openal_input.h openal_sink.cpp openal_sink.h>
)

create_target_directory_groups(audio_core)

target_link_libraries(audio_core PUBLIC lime_common lime_core)
target_link_libraries(audio_core PRIVATE faad2 SoundTouch teakra)

if(ENABLE_SDL2)
    target_link_libraries(audio_core PRIVATE SDL2::SDL2)
    target_compile_definitions(audio_core PRIVATE HAVE_SDL2)
endif()

if(ENABLE_CUBEB)
    target_link_libraries(audio_core PRIVATE cubeb)
    target_compile_definitions(audio_core PUBLIC HAVE_CUBEB)
endif()

if(ENABLE_OPENAL)
    target_link_libraries(audio_core PRIVATE OpenAL)
    target_compile_definitions(audio_core PUBLIC HAVE_OPENAL)
    add_definitions(-DAL_LIBTYPE_STATIC)
endif()

if (LIME3DS_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(audio_core PRIVATE precompiled_headers.h)
endif()
