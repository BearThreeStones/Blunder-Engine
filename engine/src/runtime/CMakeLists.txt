# engine/src/runtime/CMakeLists.txt

add_library(engine_runtime STATIC
    engine.cpp
    engine.h
    "platform/input/input_system.cpp"
    "platform/input/input_system.h"
    "function/global/global_context.cpp"
    "function/global/global_context.h"
    "function/ui/ui_context.cpp"
    "function/ui/ui_context.h"
    "function/ui/ui_event_queue.cpp"
    "function/ui/ui_event_queue.h"
    "function/ui/ui_events.h"
    "function/ui/editor_service_handles.h"
    "function/ui/editor_ui_presentation.h"
    "function/ui/editor_preview_settings.h"
    "function/ui/ui_callback_binder.h"
    "function/ui/ui_host.cpp"
    "function/ui/ui_host.h"
    "function/ui/view_models/editor_panels_view_model.cpp"
    "function/ui/view_models/editor_panels_view_model.h"
    "function/ui/viewport/viewport_cpu_frame.h"
    "function/ui/viewport/i_viewport_sink.h"
    "function/ui/viewport/slint_viewport_sink.cpp"
    "function/ui/viewport/slint_viewport_sink.h"
    "function/ui/viewport/ui_viewport_bridge.cpp"
    "function/ui/viewport/ui_viewport_bridge.h"
    "function/ui/viewport/viewport_input_layer.cpp"
    "function/ui/viewport/viewport_input_layer.h"
    "function/ui/docking/dock_types.h"
    "function/ui/docking/dock_widget.h"
    "function/ui/docking/dock_widget.cpp"
    "function/ui/docking/dock_node.h"
    "function/ui/docking/dock_node.cpp"
    "function/ui/docking/dock_layout_model.h"
    "function/ui/docking/dock_drag_controller.h"
    "function/ui/docking/dock_drag_controller.cpp"
    "function/ui/docking/dock_manager.h"
    "function/ui/docking/dock_manager.cpp"
    "function/ui/docking/dock_auto_hide.h"
    "function/ui/docking/dock_auto_hide.cpp"
    "function/ui/docking/dock_guide_hit.h"
    "function/ui/docking/dock_guide_hit.cpp"
    "function/ui/docking/dock_floating.h"
    "function/ui/docking/dock_floating_window_host.h"
    "function/ui/docking/dock_floating_window_host.cpp"
    "platform/window/child_window_registry.h"
    "platform/window/child_window_registry.cpp"
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
    "core/reflection/export_macros.h"
    "core/reflection/variant.h"
    "core/reflection/variant.cpp"
    "core/reflection/type_info.h"
    "core/reflection/class_db.h"
    "core/reflection/class_db.cpp"
    "core/reflection/lifecycle.h"
    "core/reflection/lifecycle.cpp"
    "core/reflection/engine_c_abi.h"
    "core/reflection/generated/register_generated.h"
    "core/reflection/generated/object.gen.cpp"
    "core/object/object_id.h"
    "core/object/behaviour_id.h"
    "core/object/entity_store.h"
    "core/object/object.h"
    "core/object/object.cpp"
    "core/object/object_db.h"
    "core/object/object_db.cpp"
    "platform/window/window_system.h"
    "platform/window/window_system.cpp"
    "platform/file_system/file_system.h"
    "platform/file_system/file_system.cpp"
    "project/project_file.h"
    "project/project_file.cpp"
    "project/project_create.h"
    "project/project_create.cpp"
    "project/editor_launch.h"
    "project/editor_launch.cpp"
    "function/script/scripts_builder.h"
    "function/script/scripts_builder.cpp"
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
    "function/editor/inspector_transform_ops.h"
    "function/editor/inspector_transform_ops.cpp"
    "function/editor/hierarchy_types.h"
    "function/editor/hierarchy_system.h"
    "function/editor/hierarchy_system.cpp"
    "function/editor/editor_scene_edit_system.h"
    "function/editor/editor_scene_edit_system.cpp"
    "function/editor/document_history.h"
    "function/editor/document_history.cpp"
    "function/editor/editor_commands.h"
    "function/editor/editor_commands.cpp"
    "function/editor/document_history_helpers.h"
    "function/editor/document_history_helpers.cpp"
    "function/editor/viewport_pick_system.h"
    "function/editor/viewport_pick_system.cpp"
    "function/script/dotnet_host.h"
    "function/script/dotnet_host.cpp"
    "resource/asset/scene_asset.h"
)

add_subdirectory(function/render)

slint_target_sources(engine_runtime
    "function/slint/content_browser.slint"
    "function/slint/import_mesh_dialog.slint"
    "function/slint/piercing_menu.slint"
    "function/slint/hierarchy.slint"
    "function/slint/inspector_panel.slint"
    "function/slint/axis_number_field.slint"
    "function/slint/viewport_projection_toggle.slint"
    "function/slint/editor_window.slint"
    "function/slint/transform_toolbar.slint"
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

# C-ABI lives outside engine_runtime so SHARED and static consumers do not
# duplicate ObjectDB/ClassDB globals in one process.
add_library(blunder_engine_c_static STATIC
    "core/reflection/engine_c_abi.cpp"
)
target_include_directories(blunder_engine_c_static
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(blunder_engine_c_static
    PUBLIC
        engine_runtime
)
if(MSVC)
    target_compile_options(blunder_engine_c_static PRIVATE /Zc:preprocessor)
endif()

# Shared C-ABI DLL for managed DllImport (Blunder.Api). Do not also link
# blunder_engine_c_static into the same process.
add_library(blunder_engine_c SHARED
    "core/reflection/engine_c_abi.cpp"
)
target_include_directories(blunder_engine_c
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(blunder_engine_c
    PRIVATE
        engine_runtime
)
target_compile_definitions(blunder_engine_c
    PRIVATE
        BLUNDER_ENGINE_C_EXPORTS
)
# Keep DLL basename stable for managed DllImport("blunder_engine_c").
set_target_properties(blunder_engine_c PROPERTIES
    OUTPUT_NAME "blunder_engine_c"
    DEBUG_POSTFIX ""
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/RelWithDebInfo"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin/MinSizeRel"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)
if(MSVC)
    target_compile_options(blunder_engine_c PRIVATE /Zc:preprocessor)
endif()

# ---------------------------------------------------------------------------
# Blunder.Api (net10.0) - hand-authored managed bindings over blunder_engine_c.
# Stages Blunder.Api.dll beside bin/<Config>/ for Create HintPath:
#   $(BlunderEngineDir)/Blunder.Api.dll where BlunderEngineDir is the
#   directory containing engine_editor.exe (same as bin/<Config>/).
# ---------------------------------------------------------------------------
find_program(BLUNDER_DOTNET_EXE NAMES dotnet)
set(BLUNDER_API_CSPROJ
    "${CMAKE_SOURCE_DIR}/engine/managed/Blunder.Api/Blunder.Api.csproj")
set(BLUNDER_API_DLL_SRC
    "${CMAKE_SOURCE_DIR}/engine/managed/Blunder.Api/bin/$<CONFIG>/net10.0/Blunder.Api.dll")
set(BLUNDER_API_DLL_DST
    "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/Blunder.Api.dll")

if(BLUNDER_DOTNET_EXE)
    add_custom_target(blunder_api ALL
        COMMAND "${BLUNDER_DOTNET_EXE}" build "${BLUNDER_API_CSPROJ}"
                -c $<CONFIG> --nologo
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BLUNDER_API_DLL_SRC}"
                "${BLUNDER_API_DLL_DST}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:blunder_engine_c>"
                "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Building Blunder.Api and staging beside bin/<Config>/"
        VERBATIM
        SOURCES "${BLUNDER_API_CSPROJ}"
    )
    add_dependencies(blunder_api blunder_engine_c)
else()
    message(WARNING
        "dotnet SDK not found on PATH; skipping Blunder.Api (net10.0) build. "
        "Install .NET 10 SDK to enable managed scripting bindings.")
endif()

# ---------------------------------------------------------------------------
# CoreCLR host (nethost / hostfxr) + Blunder.ScriptHost
# Requires .NET 10 host pack. Prefer DOTNET_ROOT, else directory of `dotnet`.
# ---------------------------------------------------------------------------
set(BLUNDER_HAS_NETHOST FALSE)
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_blunder_dotnet_rid "win-x64")
    else()
        set(_blunder_dotnet_rid "win-x86")
    endif()
elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(_blunder_dotnet_rid "osx-arm64")
    else()
        set(_blunder_dotnet_rid "osx-x64")
    endif()
else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(_blunder_dotnet_rid "linux-arm64")
    else()
        set(_blunder_dotnet_rid "linux-x64")
    endif()
endif()

if(DEFINED ENV{DOTNET_ROOT} AND NOT "$ENV{DOTNET_ROOT}" STREQUAL "")
    set(_blunder_dotnet_root "$ENV{DOTNET_ROOT}")
elseif(BLUNDER_DOTNET_EXE)
    get_filename_component(_blunder_dotnet_root "${BLUNDER_DOTNET_EXE}" DIRECTORY)
else()
    set(_blunder_dotnet_root "")
endif()

set(_blunder_host_pack_root
    "${_blunder_dotnet_root}/packs/Microsoft.NETCore.App.Host.${_blunder_dotnet_rid}")
if(EXISTS "${_blunder_host_pack_root}")
    file(GLOB _blunder_host_versions LIST_DIRECTORIES true
         "${_blunder_host_pack_root}/*")
    if(_blunder_host_versions)
        list(SORT _blunder_host_versions COMPARE NATURAL ORDER DESCENDING)
        list(GET _blunder_host_versions 0 _blunder_host_ver_dir)
        set(BLUNDER_NETHOST_DIR
            "${_blunder_host_ver_dir}/runtimes/${_blunder_dotnet_rid}/native")
        if(EXISTS "${BLUNDER_NETHOST_DIR}/nethost.h" AND
           (EXISTS "${BLUNDER_NETHOST_DIR}/nethost.lib" OR
            EXISTS "${BLUNDER_NETHOST_DIR}/libnethost.so" OR
            EXISTS "${BLUNDER_NETHOST_DIR}/libnethost.dylib" OR
            EXISTS "${BLUNDER_NETHOST_DIR}/libnethost.a"))
            set(BLUNDER_HAS_NETHOST TRUE)
        endif()
    endif()
endif()

# Visible to engine/src/tests (sibling) for nethost.dll copy beside test exe.
set(BLUNDER_HAS_NETHOST "${BLUNDER_HAS_NETHOST}" PARENT_SCOPE)
set(BLUNDER_NETHOST_DIR "${BLUNDER_NETHOST_DIR}" PARENT_SCOPE)
set(BLUNDER_DOTNET_EXE "${BLUNDER_DOTNET_EXE}" PARENT_SCOPE)

if(BLUNDER_HAS_NETHOST)
    message(STATUS "Blunder nethost: ${BLUNDER_NETHOST_DIR}")
    target_compile_definitions(engine_runtime PUBLIC BLUNDER_HAS_NETHOST=1)
    # PUBLIC: static engine_runtime leaves nethost symbols unresolved for
    # SHARED consumers (blunder_engine_c) that must also link nethost.
    target_include_directories(engine_runtime PRIVATE "${BLUNDER_NETHOST_DIR}")
    target_link_directories(engine_runtime PUBLIC "${BLUNDER_NETHOST_DIR}")
    target_link_libraries(engine_runtime PUBLIC nethost)
else()
    target_compile_definitions(engine_runtime PUBLIC BLUNDER_HAS_NETHOST=0)
    message(WARNING
        "nethost not found (need .NET 10 host pack under DOTNET_ROOT or "
        "beside `dotnet` on PATH). DotNetHost::start will return false. "
        "Install .NET 10 SDK and/or set DOTNET_ROOT.")
endif()

set(BLUNDER_SCRIPTHOST_CSPROJ
    "${CMAKE_SOURCE_DIR}/engine/managed/Blunder.ScriptHost/Blunder.ScriptHost.csproj")
set(BLUNDER_SCRIPTHOST_OUT_DIR
    "${CMAKE_SOURCE_DIR}/engine/managed/Blunder.ScriptHost/bin/$<CONFIG>/net10.0")
set(BLUNDER_SCRIPTHOST_STAGE_DIR
    "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

if(BLUNDER_DOTNET_EXE)
    add_custom_target(blunder_script_host ALL
        COMMAND "${BLUNDER_DOTNET_EXE}" build "${BLUNDER_SCRIPTHOST_CSPROJ}"
                -c $<CONFIG> --nologo
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${BLUNDER_SCRIPTHOST_STAGE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BLUNDER_SCRIPTHOST_OUT_DIR}/Blunder.ScriptHost.dll"
                "${BLUNDER_SCRIPTHOST_STAGE_DIR}/Blunder.ScriptHost.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BLUNDER_SCRIPTHOST_OUT_DIR}/Blunder.ScriptHost.runtimeconfig.json"
                "${BLUNDER_SCRIPTHOST_STAGE_DIR}/Blunder.ScriptHost.runtimeconfig.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BLUNDER_SCRIPTHOST_OUT_DIR}/Blunder.Api.dll"
                "${BLUNDER_SCRIPTHOST_STAGE_DIR}/Blunder.Api.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:blunder_engine_c>"
                "${BLUNDER_SCRIPTHOST_STAGE_DIR}/"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Building Blunder.ScriptHost and staging beside bin/<Config>/"
        VERBATIM
        SOURCES "${BLUNDER_SCRIPTHOST_CSPROJ}"
    )
    add_dependencies(blunder_script_host blunder_engine_c)
    if(TARGET blunder_api)
        add_dependencies(blunder_script_host blunder_api)
    endif()
else()
    message(WARNING
        "dotnet SDK not found on PATH; skipping Blunder.ScriptHost build. "
        "Install .NET 10 SDK (and set DOTNET_ROOT if needed) for CoreCLR hosting.")
endif()
