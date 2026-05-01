add_executable(engine_editor
    "src/main.cpp"
)

target_link_libraries(engine_editor
    PRIVATE engine_runtime
)

# ============================================================================
# Reuse PCH from engine_runtime (avoids duplicate PCH compilation)
# Note: Compiler options must match for REUSE_FROM to work correctly
# ============================================================================
if(MSVC)
    # Must match engine_runtime's /Zc:preprocessor for PCH compatibility
    target_compile_options(engine_editor PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(engine_editor
    REUSE_FROM engine_runtime
)

# ============================================================================
# Copy Slang runtime DLLs next to the executable
# ============================================================================
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
