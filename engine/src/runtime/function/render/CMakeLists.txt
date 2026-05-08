# Render system sources
set(RENDER_SOURCES
    "function/render/editor_camera.cpp"
    "function/render/editor_camera.h"
    "function/render/offscreen_render_target.cpp"
    "function/render/offscreen_render_target.h"
    "function/render/render_system.cpp"
    "function/render/render_system.h"
    "function/render/slang/slang_compiler.cpp"
    "function/render/slang/slang_compiler.h"
    "function/render/vulkan/vulkan_context.cpp"
    "function/render/vulkan/vulkan_context.h"
    "function/render/vulkan/vulkan_allocator.cpp"
    "function/render/vulkan/vulkan_allocator.h"
    "function/render/vulkan/vulkan_sync.cpp"
    "function/render/vulkan/vulkan_sync.h"
    "function/render/vulkan/vulkan_shader.cpp"
    "function/render/vulkan/vulkan_shader.h"
    "function/render/vulkan/vulkan_pipeline.cpp"
    "function/render/vulkan/vulkan_pipeline.h"
    "function/render/vulkan/vulkan_buffer.cpp"
    "function/render/vulkan/vulkan_buffer.h"
)

# Add sources to parent scope for engine_runtime library
set(RENDER_SOURCES ${RENDER_SOURCES} PARENT_SCOPE)
