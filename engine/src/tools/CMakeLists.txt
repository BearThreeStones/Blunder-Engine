add_executable(asset_compiler
    "asset_compiler/main.cpp"
)

target_link_libraries(asset_compiler
    PRIVATE engine_runtime cgltf
)

if(MSVC)
    target_compile_options(asset_compiler PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(asset_compiler
    REUSE_FROM engine_runtime
)

add_custom_target(cook-assets
    COMMAND $<TARGET_FILE:asset_compiler> --project-root "${CMAKE_SOURCE_DIR}"
    DEPENDS asset_compiler
    COMMENT "Cooking project assets into .blunder/cooked/"
    VERBATIM
)
