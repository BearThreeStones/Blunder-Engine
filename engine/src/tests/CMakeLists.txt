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
    PRIVATE engine_runtime
)

if(MSVC)
    target_compile_options(engine_c_abi_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(engine_c_abi_test
    REUSE_FROM engine_runtime
)

add_test(NAME engine_c_abi_test
         COMMAND engine_c_abi_test)

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
    PRIVATE engine_runtime
)

if(MSVC)
    target_compile_options(scene_serializer_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(scene_serializer_test
    REUSE_FROM engine_runtime
)

add_test(NAME scene_serializer_test
         COMMAND scene_serializer_test)
