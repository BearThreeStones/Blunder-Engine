enable_testing()

add_executable(translate_modal_session_tests
    "translate_modal_session_tests.cpp"
)

target_link_libraries(translate_modal_session_tests
    PRIVATE engine_runtime
)

if(MSVC)
    target_compile_options(translate_modal_session_tests PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(translate_modal_session_tests
    REUSE_FROM engine_runtime
)

add_test(NAME translate_modal_session_tests
         COMMAND translate_modal_session_tests)

add_executable(outline_color_tests
    "outline_color_tests.cpp"
)

target_link_libraries(outline_color_tests
    PRIVATE
)

target_include_directories(outline_color_tests
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

if(MSVC)
    target_compile_options(outline_color_tests PRIVATE /Zc:preprocessor)
endif()

add_test(NAME outline_color_tests
         COMMAND outline_color_tests)

add_executable(outline_aa_test
    "outline_aa_test.cpp"
    "../runtime/function/render/overlay/outline_aa.cpp"
)

target_include_directories(outline_aa_test
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

if(MSVC)
    target_compile_options(outline_aa_test PRIVATE /Zc:preprocessor)
endif()

add_test(NAME outline_aa_test
         COMMAND outline_aa_test)

add_executable(inspector_transform_ops_test
    "inspector_transform_ops_test.cpp"
)

target_link_libraries(inspector_transform_ops_test
    PRIVATE engine_runtime
)

if(MSVC)
    target_compile_options(inspector_transform_ops_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(inspector_transform_ops_test
    REUSE_FROM engine_runtime
)

add_test(NAME inspector_transform_ops_test
         COMMAND inspector_transform_ops_test)

add_executable(classdb_test
    "classdb_test.cpp"
)

target_link_libraries(classdb_test
    PRIVATE engine_runtime
)

if(MSVC)
    target_compile_options(classdb_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(classdb_test
    REUSE_FROM engine_runtime
)

add_test(NAME classdb_test
         COMMAND classdb_test)

add_executable(variant_test
    "variant_test.cpp"
)

target_link_libraries(variant_test
    PRIVATE engine_runtime
)

if(MSVC)
    target_compile_options(variant_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(variant_test
    REUSE_FROM engine_runtime
)

add_test(NAME variant_test
         COMMAND variant_test)

add_executable(object_db_test
    "object_db_test.cpp"
)

target_link_libraries(object_db_test
    PRIVATE engine_runtime
)

if(MSVC)
    target_compile_options(object_db_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(object_db_test
    REUSE_FROM engine_runtime
)

add_test(NAME object_db_test
         COMMAND object_db_test)

add_executable(behaviour_list_test
    "behaviour_list_test.cpp"
)

target_link_libraries(behaviour_list_test
    PRIVATE engine_runtime
)

if(MSVC)
    target_compile_options(behaviour_list_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(behaviour_list_test
    REUSE_FROM engine_runtime
)

add_test(NAME behaviour_list_test
         COMMAND behaviour_list_test)

add_executable(ptrcall_lifecycle_test
    "ptrcall_lifecycle_test.cpp"
)

target_link_libraries(ptrcall_lifecycle_test
    PRIVATE engine_runtime
)

if(MSVC)
    target_compile_options(ptrcall_lifecycle_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(ptrcall_lifecycle_test
    REUSE_FROM engine_runtime
)

add_test(NAME ptrcall_lifecycle_test
         COMMAND ptrcall_lifecycle_test)

add_executable(engine_c_abi_test
    "engine_c_abi_test.cpp"
)

target_link_libraries(engine_c_abi_test
    PRIVATE blunder_engine_c_static
)

if(MSVC)
    target_compile_options(engine_c_abi_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(engine_c_abi_test
    REUSE_FROM engine_runtime
)

add_test(NAME engine_c_abi_test
         COMMAND engine_c_abi_test)

# Native ABI fill helpers: process-linked symbols + LoadLibrary SHARED module.
add_executable(native_abi_test
    "native_abi_test.cpp"
)

target_link_libraries(native_abi_test
    PRIVATE blunder_engine_c_static
)

target_compile_definitions(native_abi_test
    PRIVATE
        "BLUNDER_ENGINE_C_SHARED_PATH=R\"($<TARGET_FILE:blunder_engine_c>)\""
)

if(MSVC)
    target_compile_options(native_abi_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(native_abi_test
    REUSE_FROM engine_runtime
)

add_dependencies(native_abi_test blunder_engine_c)

add_test(NAME native_abi_test
         COMMAND native_abi_test)

# DotNetHost e2e: fixture game + managed Tick via SHARED blunder_engine_c
# (Approach A ??one ObjectDB: create/lifecycle only through the ScriptHost-
# staged C-ABI DLL, loaded via LoadLibrary ??do not link the import lib or a
# second copy next to the test exe).
# Requires .NET 10 SDK/runtime (DOTNET_ROOT) and nethost from the host pack.
add_executable(dotnet_host_test
    "dotnet_host_test.cpp"
)

# engine_runtime provides DotNetHost only. Object traffic uses LoadLibrary on
# the staged SHARED blunder_engine_c; RegisterNativeAbi fills from that module
# (Approach A / single-image host). In-process CoreCLR uses registered
# BlunderNativeAbi pointers ??Api does not DllImport SHARED.
target_link_libraries(dotnet_host_test
    PRIVATE
        engine_runtime
)

set(BLUNDER_DOTNET_HOST_GAME_CSPROJ
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/dotnet_host_game/DotnetHostGame.csproj")
set(BLUNDER_DOTNET_HOST_GAME_OUT
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/dotnet_host_game/bin/$<CONFIG>/net10.0")
set(BLUNDER_DOTNET_HOST_GAME_STAGE
    "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

target_compile_definitions(dotnet_host_test
    PRIVATE
        "BLUNDER_SCRIPT_HOST_DIR=R\"(${CMAKE_BINARY_DIR}/bin/$<CONFIG>)\""
        "BLUNDER_DOTNET_HOST_GAME_DLL=R\"(${BLUNDER_DOTNET_HOST_GAME_STAGE}/DotnetHostGame.dll)\""
)

if(MSVC)
    target_compile_options(dotnet_host_test PRIVATE /Zc:preprocessor)
endif()

# Do not REUSE_FROM engine_runtime PCH -- keeps the link graph minimal
# (DotNetHost must not pull Slint/ProjectManager via global_context).

# Build fixture game assembly and stage beside ScriptHost + test exe.
if(NOT BLUNDER_DOTNET_EXE)
    find_program(BLUNDER_DOTNET_EXE NAMES dotnet)
endif()
if(BLUNDER_DOTNET_EXE)
    add_custom_command(TARGET dotnet_host_test PRE_BUILD
        COMMAND "${BLUNDER_DOTNET_EXE}" build "${BLUNDER_DOTNET_HOST_GAME_CSPROJ}"
                -c $<CONFIG> --nologo
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BLUNDER_DOTNET_HOST_GAME_STAGE}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BLUNDER_DOTNET_HOST_GAME_OUT}/DotnetHostGame.dll"
                "${BLUNDER_DOTNET_HOST_GAME_STAGE}/DotnetHostGame.dll"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Building DotnetHostGame fixture for dotnet_host_test"
        VERBATIM
    )
endif()

if(BLUNDER_HAS_NETHOST AND WIN32)
    add_custom_command(TARGET dotnet_host_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BLUNDER_NETHOST_DIR}/nethost.dll"
                "$<TARGET_FILE_DIR:dotnet_host_test>/nethost.dll"
        VERBATIM
    )
endif()

if(TARGET blunder_script_host)
    add_dependencies(dotnet_host_test blunder_script_host)
endif()
if(TARGET blunder_api)
    add_dependencies(dotnet_host_test blunder_api)
endif()
add_dependencies(dotnet_host_test blunder_engine_c)

add_test(NAME dotnet_host_test
         COMMAND dotnet_host_test)

# Editor-style single ObjectDB: process ObjectDB + fill_from_process + managed
# Tick. Links engine_runtime + blunder_engine_c_static; does NOT LoadLibrary
# SHARED blunder_engine_c for Object create/Tick.
add_executable(editor_dotnet_host_test
    "editor_dotnet_host_test.cpp"
)

target_link_libraries(editor_dotnet_host_test
    PRIVATE
        blunder_engine_c_static
)

target_compile_definitions(editor_dotnet_host_test
    PRIVATE
        "BLUNDER_SCRIPT_HOST_DIR=R\"(${CMAKE_BINARY_DIR}/bin/$<CONFIG>)\""
        "BLUNDER_DOTNET_HOST_GAME_DLL=R\"(${BLUNDER_DOTNET_HOST_GAME_STAGE}/DotnetHostGame.dll)\""
)

if(MSVC)
    target_compile_options(editor_dotnet_host_test PRIVATE /Zc:preprocessor)
endif()

# Same fixture staging as dotnet_host_test (reuse game csproj / stage paths).
if(BLUNDER_DOTNET_EXE)
    add_custom_command(TARGET editor_dotnet_host_test PRE_BUILD
        COMMAND "${BLUNDER_DOTNET_EXE}" build "${BLUNDER_DOTNET_HOST_GAME_CSPROJ}"
                -c $<CONFIG> --nologo
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BLUNDER_DOTNET_HOST_GAME_STAGE}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BLUNDER_DOTNET_HOST_GAME_OUT}/DotnetHostGame.dll"
                "${BLUNDER_DOTNET_HOST_GAME_STAGE}/DotnetHostGame.dll"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Building DotnetHostGame fixture for editor_dotnet_host_test"
        VERBATIM
    )
endif()

if(BLUNDER_HAS_NETHOST AND WIN32)
    add_custom_command(TARGET editor_dotnet_host_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BLUNDER_NETHOST_DIR}/nethost.dll"
                "$<TARGET_FILE_DIR:editor_dotnet_host_test>/nethost.dll"
        VERBATIM
    )
endif()

if(TARGET blunder_script_host)
    add_dependencies(editor_dotnet_host_test blunder_script_host)
endif()
if(TARGET blunder_api)
    add_dependencies(editor_dotnet_host_test blunder_api)
endif()

add_test(NAME editor_dotnet_host_test
         COMMAND editor_dotnet_host_test)

add_executable(editor_history_test
    "editor_history_test.cpp"
)

target_link_libraries(editor_history_test
    PRIVATE engine_runtime
)

if(MSVC)
    target_compile_options(editor_history_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(editor_history_test
    REUSE_FROM engine_runtime
)

add_test(NAME editor_history_test
         COMMAND editor_history_test)

add_executable(scene_soft_delete_test
    "scene_soft_delete_test.cpp"
)

target_link_libraries(scene_soft_delete_test
    PRIVATE engine_runtime
)

target_include_directories(scene_soft_delete_test
    PRIVATE "${CMAKE_SOURCE_DIR}/engine/3rdparty/cgltf"
)

if(MSVC)
    target_compile_options(scene_soft_delete_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(scene_soft_delete_test
    REUSE_FROM engine_runtime
)

add_test(NAME scene_soft_delete_test
         COMMAND scene_soft_delete_test)

add_executable(editor_commands_test
    "editor_commands_test.cpp"
)

target_link_libraries(editor_commands_test
    PRIVATE engine_runtime
)

target_include_directories(editor_commands_test
    PRIVATE "${CMAKE_SOURCE_DIR}/engine/3rdparty/cgltf"
)

if(MSVC)
    target_compile_options(editor_commands_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(editor_commands_test
    REUSE_FROM engine_runtime
)

add_test(NAME editor_commands_test
         COMMAND editor_commands_test)


add_executable(project_create_test
    "project_create_test.cpp"
)

target_link_libraries(project_create_test
    PRIVATE engine_runtime
)

if(MSVC)
    target_compile_options(project_create_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(project_create_test
    REUSE_FROM engine_runtime
)

add_test(NAME project_create_test
         COMMAND project_create_test)

add_executable(godot_editor_icons_test
    "godot_editor_icons_test.cpp"
)

target_compile_definitions(godot_editor_icons_test
    PRIVATE "BLUNDER_REPO_ROOT=R\"(${CMAKE_SOURCE_DIR})\""
)

if(MSVC)
    target_compile_options(godot_editor_icons_test PRIVATE /Zc:preprocessor)
endif()

add_test(NAME godot_editor_icons_test
         COMMAND godot_editor_icons_test)

add_executable(asset_yaml_test
    "asset_yaml_test.cpp"
)

target_link_libraries(asset_yaml_test
    PRIVATE engine_runtime
)

if(MSVC)
    target_compile_options(asset_yaml_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(asset_yaml_test
    REUSE_FROM engine_runtime
)

add_test(NAME asset_yaml_test
         COMMAND asset_yaml_test)

add_executable(asset_registry_test
    "asset_registry_test.cpp"
)

target_link_libraries(asset_registry_test
    PRIVATE engine_runtime
)

if(MSVC)
    target_compile_options(asset_registry_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(asset_registry_test
    REUSE_FROM engine_runtime
)

add_test(NAME asset_registry_test
         COMMAND asset_registry_test)

add_executable(scene_serializer_test
    "scene_serializer_test.cpp"
)

target_link_libraries(scene_serializer_test
    PRIVATE blunder_engine_c_static
)

if(MSVC)
    target_compile_options(scene_serializer_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(scene_serializer_test
    REUSE_FROM engine_runtime
)

add_test(NAME scene_serializer_test
         COMMAND scene_serializer_test)

add_executable(scene_behaviour_instantiate_test
    "scene_behaviour_instantiate_test.cpp"
)

target_link_libraries(scene_behaviour_instantiate_test
    PRIVATE blunder_engine_c_static
)

target_include_directories(scene_behaviour_instantiate_test
    PRIVATE "${CMAKE_SOURCE_DIR}/engine/3rdparty/cgltf"
)

if(MSVC)
    target_compile_options(scene_behaviour_instantiate_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(scene_behaviour_instantiate_test
    REUSE_FROM engine_runtime
)

add_test(NAME scene_behaviour_instantiate_test
         COMMAND scene_behaviour_instantiate_test)

add_executable(asset_manager_guid_test
    "asset_manager_guid_test.cpp"
)

target_link_libraries(asset_manager_guid_test
    PRIVATE engine_runtime
)

target_include_directories(asset_manager_guid_test
    PRIVATE "${CMAKE_SOURCE_DIR}/engine/3rdparty/cgltf"
)

if(MSVC)
    target_compile_options(asset_manager_guid_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(asset_manager_guid_test
    REUSE_FROM engine_runtime
)

add_test(NAME asset_manager_guid_test
         COMMAND asset_manager_guid_test)

add_executable(asset_compiler_freshness_test
    "asset_compiler_freshness_test.cpp"
)

target_link_libraries(asset_compiler_freshness_test
    PRIVATE engine_runtime
)

target_include_directories(asset_compiler_freshness_test
    PRIVATE "${CMAKE_SOURCE_DIR}/engine/3rdparty/cgltf"
)

if(MSVC)
    target_compile_options(asset_compiler_freshness_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(asset_compiler_freshness_test
    REUSE_FROM engine_runtime
)

add_test(NAME asset_compiler_freshness_test
         COMMAND asset_compiler_freshness_test)

add_executable(asset_manager_fast_path_test
    "asset_manager_fast_path_test.cpp"
)

target_link_libraries(asset_manager_fast_path_test
    PRIVATE engine_runtime
)

target_include_directories(asset_manager_fast_path_test
    PRIVATE "${CMAKE_SOURCE_DIR}/engine/3rdparty/cgltf"
)

if(MSVC)
    target_compile_options(asset_manager_fast_path_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(asset_manager_fast_path_test
    REUSE_FROM engine_runtime
)

add_test(NAME asset_manager_fast_path_test
         COMMAND asset_manager_fast_path_test)

add_executable(asset_dependency_graph_test
    "asset_dependency_graph_test.cpp"
)

target_link_libraries(asset_dependency_graph_test
    PRIVATE engine_runtime
)

if(MSVC)
    target_compile_options(asset_dependency_graph_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(asset_dependency_graph_test
    REUSE_FROM engine_runtime
)

add_test(NAME asset_dependency_graph_test
         COMMAND asset_dependency_graph_test)

add_executable(asset_compiler_invalidate_test
    "asset_compiler_invalidate_test.cpp"
)

target_link_libraries(asset_compiler_invalidate_test
    PRIVATE engine_runtime
)

target_include_directories(asset_compiler_invalidate_test
    PRIVATE "${CMAKE_SOURCE_DIR}/engine/3rdparty/cgltf"
)

if(MSVC)
    target_compile_options(asset_compiler_invalidate_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(asset_compiler_invalidate_test
    REUSE_FROM engine_runtime
)

add_test(NAME asset_compiler_invalidate_test
         COMMAND asset_compiler_invalidate_test)

add_executable(asset_watch_path_test
    "asset_watch_path_test.cpp"
)

target_link_libraries(asset_watch_path_test
    PRIVATE engine_runtime
)

target_include_directories(asset_watch_path_test
    PRIVATE "${CMAKE_SOURCE_DIR}/engine/3rdparty/cgltf"
)

if(MSVC)
    target_compile_options(asset_watch_path_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(asset_watch_path_test
    REUSE_FROM engine_runtime
)

add_test(NAME asset_watch_path_test
         COMMAND asset_watch_path_test)

add_executable(asset_import_test
    "asset_import_test.cpp"
)

target_link_libraries(asset_import_test
    PRIVATE engine_runtime
)

target_include_directories(asset_import_test
    PRIVATE "${CMAKE_SOURCE_DIR}/engine/3rdparty/cgltf"
)

if(MSVC)
    target_compile_options(asset_import_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(asset_import_test
    REUSE_FROM engine_runtime
)

add_test(NAME asset_import_test
         COMMAND asset_import_test)

add_executable(asset_pipeline_smoke_test
    "asset_pipeline_smoke_test.cpp"
)

target_link_libraries(asset_pipeline_smoke_test
    PRIVATE engine_runtime
)

target_include_directories(asset_pipeline_smoke_test
    PRIVATE "${CMAKE_SOURCE_DIR}/engine/3rdparty/cgltf"
)

if(MSVC)
    target_compile_options(asset_pipeline_smoke_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(asset_pipeline_smoke_test
    REUSE_FROM engine_runtime
)

add_test(NAME asset_pipeline_smoke_test
         COMMAND asset_pipeline_smoke_test)
