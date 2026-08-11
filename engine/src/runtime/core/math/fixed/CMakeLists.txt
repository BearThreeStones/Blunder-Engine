# Physics fixed-point math (Q32.32). Standalone static lib — no glm/libm on the hot path.
# Cross-platform bit-identical peer for v0 goldens: Linux Clang x64 (see tasks 1.6).
add_library(blunder_fixed_math STATIC
    fixed.cpp
)

target_include_directories(blunder_fixed_math
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../../.."
)

if(MSVC)
    target_compile_options(blunder_fixed_math PRIVATE /Zc:preprocessor)
endif()
