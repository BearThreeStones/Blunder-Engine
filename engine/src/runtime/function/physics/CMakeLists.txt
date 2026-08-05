# Physics kernel v0 — isolated World, bodies, colliders. Links blunder_fixed_math only.
add_library(blunder_physics STATIC
    physics_world.cpp
)

target_include_directories(blunder_physics
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

target_link_libraries(blunder_physics
    PUBLIC
        blunder_fixed_math
)

if(MSVC)
    target_compile_options(blunder_physics PRIVATE /Zc:preprocessor)
endif()
