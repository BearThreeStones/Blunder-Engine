add_executable(project_manager
    "src/main.cpp"
)

target_link_libraries(project_manager
    PRIVATE
        engine_runtime
        blunder_engine_c_static
)

set_target_properties(project_manager PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/RelWithDebInfo"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin/MinSizeRel"
)

# Open spawns sibling engine_editor from the same bin/<Config> directory.
add_dependencies(project_manager engine_editor)

if(MSVC)
    target_compile_options(project_manager PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(project_manager
    REUSE_FROM engine_runtime
)

blunder_copy_runtime_dlls(project_manager)

# Stage Blunder.Api.dll (+ blunder_engine_c.dll) beside bin/<Config>/.
if(TARGET blunder_api)
    add_dependencies(project_manager blunder_api)
endif()
