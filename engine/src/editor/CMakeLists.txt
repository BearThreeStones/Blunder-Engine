add_executable(engine_editor
    "src/main.cpp"
)

target_link_libraries(engine_editor
    PRIVATE
        engine_runtime
        blunder_engine_c_static
)

# When ON, link to profile/engine_editor_profile.exe so a locked Debug/engine_editor.exe
# does not block instrumented rebuilds (profiling/nsight/run-group.ps1 sets this).
option(BLUNDER_EDITOR_PROFILE_EXE "Link editor to engine_editor_profile.exe in profile/" OFF)

# Ship beside project_manager in bin/<Config>/ (ADR 0009 Open via relaunch).
# Working directory stays the engine checkout so Slang can find engine/shaders/
# via cwd walk. Local checkout compiles in BLUNDER_PROJECT_ROOT; VS F5 also
# passes --project-root for the Test Project. Packaged builds omit the define.
if(NOT DEFINED BLUNDER_DEFAULT_PROJECT_ROOT OR BLUNDER_DEFAULT_PROJECT_ROOT STREQUAL "")
    set(BLUNDER_DEFAULT_PROJECT_ROOT "E:/Blunder Projects/Test")
endif()
file(TO_CMAKE_PATH "${BLUNDER_DEFAULT_PROJECT_ROOT}" _blunder_editor_debug_root)
set_target_properties(engine_editor PROPERTIES
    WIN32_EXECUTABLE TRUE
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    VS_DEBUGGER_COMMAND_ARGUMENTS "--project-root \"${_blunder_editor_debug_root}\""
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/RelWithDebInfo"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin/MinSizeRel"
)

if(BLUNDER_EDITOR_PROFILE_EXE)
    set_target_properties(engine_editor PROPERTIES
        OUTPUT_NAME "engine_editor_profile"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/profile"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/profile"
    )
endif()

if(MSVC)
    target_compile_options(engine_editor PRIVATE /Zc:preprocessor)
endif()

blunder_msvc_limit_cl_mp(engine_editor)

target_precompile_headers(engine_editor
    REUSE_FROM engine_runtime
)

if(WIN32 AND SLANG_DLL_PATH)
    add_custom_command(TARGET engine_editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SLANG_DLL_PATH}"
            "$<TARGET_FILE_DIR:engine_editor>/"
        COMMENT "Copying slang.dll to output directory"
    )
    if(SLANG_GLSLANG_DLL_PATH)
        add_custom_command(TARGET engine_editor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SLANG_GLSLANG_DLL_PATH}"
                "$<TARGET_FILE_DIR:engine_editor>/"
            COMMENT "Copying slang-glslang.dll to output directory"
        )
    endif()
endif()

if(WIN32)
    add_custom_command(TARGET engine_editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_RUNTIME_DLLS:engine_editor>
            $<TARGET_FILE_DIR:engine_editor>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime DLLs to output directory"
    )
endif()


# Stage Blunder.Api.dll (+ blunder_engine_c.dll) beside bin/<Config>/.
if(TARGET blunder_api)
    add_dependencies(engine_editor blunder_api)
endif()
if(TARGET blunder_script_host)
    add_dependencies(engine_editor blunder_script_host)
endif()

# nethost.dll must sit beside the editor for DotNetHost (linked via nethost.lib).
if(WIN32 AND BLUNDER_HAS_NETHOST AND EXISTS "${BLUNDER_NETHOST_DIR}/nethost.dll")
    add_custom_command(TARGET engine_editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${BLUNDER_NETHOST_DIR}/nethost.dll"
            "$<TARGET_FILE_DIR:engine_editor>/nethost.dll"
        COMMENT "Copying nethost.dll beside engine_editor"
    )
endif()
