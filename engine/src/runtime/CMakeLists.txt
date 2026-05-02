# engine/src/runtime/CMakeLists.txt

add_library(engine_runtime STATIC
    engine.cpp
    engine.h
    "platform/input/input_system.cpp"
    "platform/input/input_system.h"
    "function/global/global_context.cpp"
    "function/global/global_context.h"
    "function/slint/slint_system.h"
    "function/slint/slint_system.cpp"
    "core/log/log_system.h"
    "core/log/log_system.cpp"
    "core/base/high_precision_timer.h"
    "core/base/high_precision_timer.cpp"
    "core/base/macro.h"
    "core/memory/allocators.h"
    "core/memory/allocators.cpp"
    "core/memory/memory_system.h"
    "core/memory/memory_system.cpp"
    "core/memory/eastl_allocator.cpp"
    "core/math/math.h"
    "core/math/math_types.h"
    "core/math/math_utils.h"
    "core/math/math_utils.cpp"
    "core/math/transform.h"
    "core/math/geometry.h"
    "core/math/interpolation.h"
    "core/event/event.h"
    "core/event/application_event.h"
    "core/event/key_event.h"
    "core/event/mouse_event.h"
    "core/layer/layer.h"
    "core/layer/layer.cpp"
    "core/layer/layer_stack.h"
    "core/layer/layer_stack.cpp"
    "platform/window/window_system.h"
    "platform/window/window_system.cpp"
    "platform/file_system/file_system.h"
    "platform/file_system/file_system.cpp"
)

add_subdirectory(function/render)

slint_target_sources(engine_runtime
    "function/slint/editor_window.slint"
)

target_sources(engine_runtime PRIVATE ${RENDER_SOURCES})

target_include_directories(engine_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# ============================================================================
# Precompiled Headers (PCH) - Reduces compilation time
# ============================================================================
target_precompile_headers(engine_runtime
    PRIVATE
        pch.h
)

# Tell EASTL to skip its default allocator implementations;
# we provide our own in core/memory/eastl_allocator.cpp.
# Applied to the EASTL target directly so the define is active when
# EASTL's own source files (allocator_eastl.cpp) and headers compile.
target_compile_definitions(EASTL
    PUBLIC
        EASTL_USER_DEFINED_ALLOCATOR
)

# Enable conformant preprocessor for __VA_OPT__ support (C++20)
if(MSVC)
    target_compile_options(engine_runtime PRIVATE /Zc:preprocessor)
endif()

target_link_libraries(engine_runtime
    PUBLIC
        EASTL
        glm
        spdlog
        Vulkan::Vulkan
        Slint::Slint
    PRIVATE
        cgltf
        SDL3::SDL3
        vma
        slang::slang
)

# Inject the project source root so FileSystem can resolve asset/shader paths
# during dev builds without relying on the executable's working directory.
target_compile_definitions(engine_runtime
    PRIVATE
        BLUNDER_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)
