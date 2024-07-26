add_subdirectory(host_shaders)

add_library(video_core STATIC
    custom_textures/custom_format.cpp
    custom_textures/custom_format.h
    custom_textures/custom_tex_manager.cpp
    custom_textures/custom_tex_manager.h
    custom_textures/material.cpp
    custom_textures/material.h
    debug_utils/debug_utils.cpp
    debug_utils/debug_utils.h
    gpu.cpp
    gpu.h
    gpu_debugger.h
    pica_types.h
    precompiled_headers.h
    rasterizer_accelerated.cpp
    rasterizer_accelerated.h
    rasterizer_interface.h
    renderer_base.cpp
    renderer_base.h
    pica/geometry_pipeline.cpp
    pica/geometry_pipeline.h
    pica/pica_core.cpp
    pica/pica_core.h
    pica/output_vertex.cpp
    pica/output_vertex.h
    pica/primitive_assembly.cpp
    pica/primitive_assembly.h
    pica/regs_external.h
    pica/regs_framebuffer.h
    pica/regs_internal.cpp
    pica/regs_internal.h
    pica/regs_lcd.h
    pica/regs_lighting.h
    pica/regs_pipeline.h
    pica/regs_rasterizer.h
    pica/regs_shader.h
    pica/regs_texturing.h
    pica/shader_setup.cpp
    pica/shader_setup.h
    pica/shader_unit.cpp
    pica/shader_unit.h
    pica/packed_attribute.h
    pica/vertex_loader.cpp
    pica/vertex_loader.h
    rasterizer_cache/framebuffer_base.h
    rasterizer_cache/pixel_format.cpp
    rasterizer_cache/pixel_format.h
    rasterizer_cache/rasterizer_cache.cpp
    rasterizer_cache/rasterizer_cache.h
    rasterizer_cache/rasterizer_cache_base.h
    rasterizer_cache/sampler_params.h
    rasterizer_cache/slot_id.h
    rasterizer_cache/surface_base.cpp
    rasterizer_cache/surface_base.h
    rasterizer_cache/surface_params.cpp
    rasterizer_cache/surface_params.h
    rasterizer_cache/texture_codec.h
    rasterizer_cache/texture_cube.h
    rasterizer_cache/utils.cpp
    rasterizer_cache/utils.h
    # Needed as a fallback regardless of enabled renderers.
    renderer_software/sw_blitter.cpp
    renderer_software/sw_blitter.h
    shader/debug_data.h
    shader/generator/glsl_fs_shader_gen.cpp
    shader/generator/glsl_fs_shader_gen.h
    shader/generator/glsl_shader_decompiler.cpp
    shader/generator/glsl_shader_decompiler.h
    shader/generator/glsl_shader_gen.cpp
    shader/generator/glsl_shader_gen.h
    shader/generator/pica_fs_config.cpp
    shader/generator/pica_fs_config.h
    shader/generator/profile.h
    shader/generator/shader_gen.cpp
    shader/generator/shader_gen.h
    shader/generator/shader_uniforms.cpp
    shader/generator/shader_uniforms.h
    shader/shader.cpp
    shader/shader.h
    shader/shader_interpreter.cpp
    shader/shader_interpreter.h
    shader/shader_jit.cpp
    shader/shader_jit.h
    shader/shader_jit_a64_compiler.cpp
    shader/shader_jit_a64_compiler.h
    shader/shader_jit_x64_compiler.cpp
    shader/shader_jit_x64_compiler.h
    texture/etc1.cpp
    texture/etc1.h
    texture/texture_decode.cpp
    texture/texture_decode.h
    utils.h
    video_core.cpp
    video_core.h
)

if (ENABLE_SOFTWARE_RENDERER)
    target_sources(video_core PRIVATE
        renderer_software/renderer_software.cpp
        renderer_software/renderer_software.h
        renderer_software/sw_clipper.cpp
        renderer_software/sw_clipper.h
        renderer_software/sw_framebuffer.cpp
        renderer_software/sw_framebuffer.h
        renderer_software/sw_lighting.cpp
        renderer_software/sw_lighting.h
        renderer_software/sw_proctex.cpp
        renderer_software/sw_proctex.h
        renderer_software/sw_rasterizer.cpp
        renderer_software/sw_rasterizer.h
        renderer_software/sw_texturing.cpp
        renderer_software/sw_texturing.h
    )
endif()
if (ENABLE_OPENGL)
    target_sources(video_core PRIVATE
        renderer_opengl/frame_dumper_opengl.cpp
        renderer_opengl/frame_dumper_opengl.h
        renderer_opengl/gl_blit_helper.cpp
        renderer_opengl/gl_blit_helper.h
        renderer_opengl/gl_driver.cpp
        renderer_opengl/gl_driver.h
        renderer_opengl/gl_rasterizer.cpp
        renderer_opengl/gl_rasterizer.h
        renderer_opengl/gl_rasterizer_cache.cpp
        renderer_opengl/gl_resource_manager.cpp
        renderer_opengl/gl_resource_manager.h
        renderer_opengl/gl_shader_disk_cache.cpp
        renderer_opengl/gl_shader_disk_cache.h
        renderer_opengl/gl_shader_manager.cpp
        renderer_opengl/gl_shader_manager.h
        renderer_opengl/gl_shader_util.cpp
        renderer_opengl/gl_shader_util.h
        renderer_opengl/gl_state.cpp
        renderer_opengl/gl_state.h
        renderer_opengl/gl_stream_buffer.cpp
        renderer_opengl/gl_stream_buffer.h
        renderer_opengl/gl_texture_mailbox.cpp
        renderer_opengl/gl_texture_mailbox.h
        renderer_opengl/gl_texture_runtime.cpp
        renderer_opengl/gl_texture_runtime.h
        renderer_opengl/gl_vars.cpp
        renderer_opengl/gl_vars.h
        renderer_opengl/pica_to_gl.h
        renderer_opengl/post_processing_opengl.cpp
        renderer_opengl/post_processing_opengl.h
        renderer_opengl/renderer_opengl.cpp
        renderer_opengl/renderer_opengl.h
    )
    target_link_libraries(video_core PRIVATE glad)
endif()
if (ENABLE_VULKAN)
    target_sources(video_core PRIVATE
        renderer_vulkan/pica_to_vk.h
        renderer_vulkan/renderer_vulkan.cpp
        renderer_vulkan/renderer_vulkan.h
        renderer_vulkan/vk_blit_helper.cpp
        renderer_vulkan/vk_blit_helper.h
        renderer_vulkan/vk_common.cpp
        renderer_vulkan/vk_common.h
        renderer_vulkan/vk_descriptor_update_queue.cpp
        renderer_vulkan/vk_descriptor_update_queue.h
        renderer_vulkan/vk_graphics_pipeline.cpp
        renderer_vulkan/vk_graphics_pipeline.h
        renderer_vulkan/vk_master_semaphore.cpp
        renderer_vulkan/vk_master_semaphore.h
        renderer_vulkan/vk_memory_util.cpp
        renderer_vulkan/vk_memory_util.h
        renderer_vulkan/vk_rasterizer.cpp
        renderer_vulkan/vk_rasterizer.h
        renderer_vulkan/vk_rasterizer_cache.cpp
        renderer_vulkan/vk_scheduler.cpp
        renderer_vulkan/vk_scheduler.h
        renderer_vulkan/vk_resource_pool.cpp
        renderer_vulkan/vk_resource_pool.h
        renderer_vulkan/vk_instance.cpp
        renderer_vulkan/vk_instance.h
        renderer_vulkan/vk_pipeline_cache.cpp
        renderer_vulkan/vk_pipeline_cache.h
        renderer_vulkan/vk_platform.cpp
        renderer_vulkan/vk_platform.h
        renderer_vulkan/vk_present_window.cpp
        renderer_vulkan/vk_present_window.h
        renderer_vulkan/vk_render_manager.cpp
        renderer_vulkan/vk_render_manager.h
        renderer_vulkan/vk_shader_util.cpp
        renderer_vulkan/vk_shader_util.h
        renderer_vulkan/vk_stream_buffer.cpp
        renderer_vulkan/vk_stream_buffer.h
        renderer_vulkan/vk_swapchain.cpp
        renderer_vulkan/vk_swapchain.h
        renderer_vulkan/vk_texture_runtime.cpp
        renderer_vulkan/vk_texture_runtime.h
        shader/generator/spv_fs_shader_gen.cpp
        shader/generator/spv_fs_shader_gen.h
    )
    target_link_libraries(video_core PRIVATE vulkan-headers vma sirit SPIRV glslang)
endif()

add_dependencies(video_core host_shaders)
target_include_directories(video_core PRIVATE ${HOST_SHADERS_INCLUDE})

create_target_directory_groups(video_core)

target_link_libraries(video_core PUBLIC lime_common lime_core)
target_link_libraries(video_core PRIVATE Boost::serialization dds-ktx json-headers nihstro-headers tsl::robin_map)

if ("x86_64" IN_LIST ARCHITECTURE)
    target_link_libraries(video_core PUBLIC xbyak)
endif()

if ("arm64" IN_LIST ARCHITECTURE)
    target_link_libraries(video_core PUBLIC oaknut)
endif()

if (LIME3DS_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(video_core PRIVATE precompiled_headers.h)
endif()
