add_executable(asset_compiler
    "asset_compiler/main.cpp"
)

target_link_libraries(asset_compiler
    PRIVATE
        engine_runtime
        blunder_engine_c_static
        cgltf
)

if(MSVC)
    target_compile_options(asset_compiler PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(asset_compiler
    REUSE_FROM engine_runtime
)

blunder_copy_runtime_dlls(asset_compiler)

add_executable(se_world_flatten
    "se_world_flatten/main.cpp"
)

target_link_libraries(se_world_flatten
    PRIVATE
        engine_runtime
        blunder_engine_c_static
        cgltf
)

if(MSVC)
    target_compile_options(se_world_flatten PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(se_world_flatten
    REUSE_FROM engine_runtime
)

blunder_copy_runtime_dlls(se_world_flatten)

add_custom_target(cook-assets
    COMMAND $<TARGET_FILE:asset_compiler> --project-root "${CMAKE_SOURCE_DIR}"
    DEPENDS asset_compiler
    COMMENT "Cooking project assets into .blunder/cooked/"
    VERBATIM
)
