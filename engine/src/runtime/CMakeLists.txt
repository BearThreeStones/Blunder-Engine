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
    "core/log/log_access.h"
    "core/log/log_access.cpp"
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
    "core/math/coordinate_system.h"
    "core/math/coordinate_system.cpp"
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
    "resource/asset/asset.h"
    "resource/asset/guid.h"
    "resource/asset/guid.cpp"
    "resource/asset/asset_descriptor.h"
    "resource/asset/asset_yaml.h"
    "resource/asset/asset_yaml.cpp"
    "resource/asset_registry/asset_registry.h"
    "resource/asset_registry/asset_registry.cpp"
    "resource/asset_cook/asset_cook_types.h"
    "resource/asset_cook/mesh_cooker.h"
    "resource/asset_cook/mesh_cooker.cpp"
    "resource/asset_cook/texture_cooker.h"
    "resource/asset_cook/texture_cooker.cpp"
    "resource/asset_cook/asset_compiler_service.h"
    "resource/asset_cook/asset_compiler_service.cpp"
    "resource/asset_import/asset_import_service.h"
    "resource/asset_import/asset_import_service.cpp"
    "resource/asset/material_asset.h"
    "resource/asset/shader_asset.h"
    "resource/asset/mesh_asset.h"
    "resource/asset/texture2d_asset.h"
    "resource/asset_manager/asset_manager.h"
    "resource/asset_manager/asset_manager.cpp"
    "resource/asset_manager/asset_manager_gltf.h"
    "resource/asset_manager/asset_manager_gltf.cpp"
    "resource/content/content_entry.h"
    "resource/content/content_index.h"
    "resource/content/content_index.cpp"
    "resource/thumbnail/thumbnail_types.h"
    "resource/thumbnail/thumbnail_cache.h"
    "resource/thumbnail/thumbnail_cache.cpp"
    "resource/thumbnail/thumbnail_resize.h"
    "resource/thumbnail/thumbnail_resize.cpp"
    "resource/thumbnail/thumbnail_placeholders.h"
    "resource/thumbnail/thumbnail_placeholders.cpp"
    "resource/thumbnail/thumbnail_generator.h"
    "resource/thumbnail/thumbnail_generator.cpp"
    "resource/content_browser/content_browser_types.h"
    "resource/content_browser/content_browser_drag.h"
    "resource/content_browser/content_browser_drag.cpp"
    "resource/content_browser/content_browser_system.h"
    "resource/content_browser/content_browser_system.cpp"
    "resource/content_browser/content_browser_watch.h"
    "resource/content_browser/content_browser_watch.cpp"
    "function/scene/entity_id.h"
    "function/scene/entity.h"
    "function/scene/entity.cpp"
    "function/scene/scene.h"
    "function/scene/scene_instance.h"
    "function/scene/scene_instance.cpp"
    "function/scene/scene_serializer.h"
    "function/scene/scene_serializer.cpp"
    "function/scene/scene_system.h"
    "function/scene/scene_system.cpp"
    "function/scene/mesh_renderer_component.h"
    "function/scene/gltf_scene_importer.h"
    "function/scene/gltf_scene_importer.cpp"
    "function/scene/scene_render_bridge.h"
    "function/scene/scene_render_bridge.cpp"
    "function/editor/editor_selection_system.h"
    "function/editor/editor_selection_system.cpp"
    "function/editor/hierarchy_types.h"
    "function/editor/hierarchy_system.h"
    "function/editor/hierarchy_system.cpp"
    "function/editor/editor_scene_edit_system.h"
    "function/editor/editor_scene_edit_system.cpp"
    "resource/asset/scene_asset.h"
)

add_subdirectory(function/render)

slint_target_sources(engine_runtime
    "function/slint/content_browser.slint"
    "function/slint/import_mesh_dialog.slint"
    "function/slint/hierarchy.slint"
    "function/slint/editor_window.slint"
)

target_sources(engine_runtime PRIVATE ${RENDER_SOURCES})

target_include_directories(engine_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        # Vendored RenderDoc In-Application API header (renderdoc_app.h).
        # Used by function/render/debug/renderdoc_capture.cpp to drive
        # Start/EndFrameCapture explicitly under headless Vulkan, where
        # RenderDoc's vkQueuePresentKHR hook never fires.
        ${CMAKE_SOURCE_DIR}/engine/3rdparty/renderdoc
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
        Recast
        Detour
        DetourCrowd
        DetourTileCache
        Vulkan::Vulkan
        Slint::Slint
    PRIVATE
        cgltf
        stb
        SDL3::SDL3
        vma
        slang::slang
        yaml-cpp
)

if(TARGET efsw)
    target_compile_definitions(engine_runtime PRIVATE BLUNDER_HAS_EFSW=1)
    target_link_libraries(engine_runtime PRIVATE efsw)
endif()

# Inject the project source root so FileSystem can resolve asset/shader paths
# during dev builds without relying on the executable's working directory.
target_compile_definitions(engine_runtime
    PRIVATE
        BLUNDER_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

if(BLUNDER_RENDER_BACKEND STREQUAL "d3d12")
    target_compile_definitions(engine_runtime PRIVATE BLUNDER_RENDER_BACKEND_D3D12=1)
else()
    target_compile_definitions(engine_runtime PRIVATE BLUNDER_RENDER_BACKEND_VULKAN=1)
endif()

if(WIN32)
    target_link_libraries(engine_runtime PRIVATE d3d12 dxgi)
endif()
