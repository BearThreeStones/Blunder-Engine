add_executable(engine_player
    "src/main.cpp"
)

target_link_libraries(engine_player
    PRIVATE
        engine_runtime
        blunder_engine_c_static
)

set_target_properties(engine_player PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(engine_player PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(engine_player
    REUSE_FROM engine_runtime
)

if(WIN32 AND SLANG_DLL_PATH)
    add_custom_command(TARGET engine_player POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SLANG_DLL_PATH}"
            "$<TARGET_FILE_DIR:engine_player>/"
        COMMENT "Copying slang.dll to output directory"
    )
    if(SLANG_GLSLANG_DLL_PATH)
        add_custom_command(TARGET engine_player POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SLANG_GLSLANG_DLL_PATH}"
                "$<TARGET_FILE_DIR:engine_player>/"
            COMMENT "Copying slang-glslang.dll to output directory"
        )
    endif()
endif()

if(WIN32)
    add_custom_command(TARGET engine_player POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_RUNTIME_DLLS:engine_player>
            $<TARGET_FILE_DIR:engine_player>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime DLLs to output directory"
    )
endif()

# Stage Blunder.Api.dll (+ blunder_engine_c.dll) beside bin/<Config>/.
if(TARGET blunder_api)
    add_dependencies(engine_player blunder_api)
endif()
if(TARGET blunder_script_host)
    add_dependencies(engine_player blunder_script_host)
endif()

# nethost.dll must sit beside the player for DotNetHost (Task 2+).
if(WIN32 AND BLUNDER_HAS_NETHOST AND EXISTS "${BLUNDER_NETHOST_DIR}/nethost.dll")
    add_custom_command(TARGET engine_player POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${BLUNDER_NETHOST_DIR}/nethost.dll"
            "$<TARGET_FILE_DIR:engine_player>/nethost.dll"
        COMMENT "Copying nethost.dll beside engine_player"
    )
endif()
