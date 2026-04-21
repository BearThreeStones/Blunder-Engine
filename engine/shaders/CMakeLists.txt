# Shader source files - copied to build output for runtime compilation by Slang
set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADER_BINARY_DIR ${CMAKE_BINARY_DIR}/shaders)

file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})

# List of Slang shader files to deploy
set(SHADERS
    basic.slang
)

# Copy each shader to the build output directory
foreach(SHADER ${SHADERS})
    set(SHADER_OUTPUT ${SHADER_BINARY_DIR}/${SHADER})

    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SHADER_SOURCE_DIR}/${SHADER} ${SHADER_OUTPUT}
        DEPENDS ${SHADER_SOURCE_DIR}/${SHADER}
        COMMENT "Copying ${SHADER} to build output"
        VERBATIM
    )

    list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach()

add_custom_target(deploy_shaders ALL DEPENDS ${SHADER_OUTPUTS})
