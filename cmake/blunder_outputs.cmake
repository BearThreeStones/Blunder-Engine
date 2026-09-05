# Shared output-layout helpers for Blunder product and test binaries.
#
# Product:  ${CMAKE_BINARY_DIR}/bin/<Config>/
# Tests:    ${CMAKE_BINARY_DIR}/tests/<Config>/

# VS 2022+/2026 debugger uses Path.IsPathFullyQualified: "E:/foo" is rejected as
# "not a complete path". Write a native Windows cwd for F5.
function(blunder_set_debugger_working_directory target)
    cmake_path(NATIVE_PATH CMAKE_SOURCE_DIR NORMALIZE _blunder_debugger_wd)
    set_property(TARGET ${target} PROPERTY
        VS_DEBUGGER_WORKING_DIRECTORY "${_blunder_debugger_wd}")
    set_property(TARGET ${target} PROPERTY
        DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endfunction()

macro(blunder_use_bin_outputs)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG
        "${CMAKE_BINARY_DIR}/bin/Debug")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE
        "${CMAKE_BINARY_DIR}/bin/Release")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO
        "${CMAKE_BINARY_DIR}/bin/RelWithDebInfo")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL
        "${CMAKE_BINARY_DIR}/bin/MinSizeRel")
    if(NOT CMAKE_CONFIGURATION_TYPES AND CMAKE_BUILD_TYPE)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}")
    endif()
endmacro()

macro(blunder_use_test_outputs)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG
        "${CMAKE_BINARY_DIR}/tests/Debug")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE
        "${CMAKE_BINARY_DIR}/tests/Release")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO
        "${CMAKE_BINARY_DIR}/tests/RelWithDebInfo")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL
        "${CMAKE_BINARY_DIR}/tests/MinSizeRel")
    if(NOT CMAKE_CONFIGURATION_TYPES AND CMAKE_BUILD_TYPE)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_BINARY_DIR}/tests/${CMAKE_BUILD_TYPE}")
    endif()
endmacro()

# Copy Slang + target runtime DLLs next to a product executable (Windows).
function(blunder_copy_runtime_dlls target)
    if(NOT WIN32)
        return()
    endif()
    if(SLANG_DLL_PATH)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SLANG_DLL_PATH}"
                "$<TARGET_FILE_DIR:${target}>/"
            COMMENT "Copying slang.dll next to ${target}"
        )
        get_filename_component(_slang_bin_dir "${SLANG_DLL_PATH}" DIRECTORY)
        if(EXISTS "${_slang_bin_dir}/slang-compiler.dll")
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_slang_bin_dir}/slang-compiler.dll"
                    "$<TARGET_FILE_DIR:${target}>/"
                COMMENT "Copying slang-compiler.dll next to ${target}"
            )
        endif()
        if(EXISTS "${_slang_bin_dir}/slang-glsl-module.dll")
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_slang_bin_dir}/slang-glsl-module.dll"
                    "$<TARGET_FILE_DIR:${target}>/"
                COMMENT "Copying slang-glsl-module.dll next to ${target}"
            )
        endif()
        if(SLANG_GLSLANG_DLL_PATH)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${SLANG_GLSLANG_DLL_PATH}"
                    "$<TARGET_FILE_DIR:${target}>/"
                COMMENT "Copying slang-glslang.dll next to ${target}"
            )
        endif()
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_RUNTIME_DLLS:${target}>
            $<TARGET_FILE_DIR:${target}>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime DLLs next to ${target}"
    )
endfunction()
