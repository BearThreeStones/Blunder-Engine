# engine/3rdparty/CMakeLists.txt

# Slint C++ - source-built UI toolkit with Skia Vulkan renderer
include(${CMAKE_CURRENT_SOURCE_DIR}/slint.cmake)

# GLM - header-only math library; disable library build to avoid
# producing an empty glm.lib / glmd.lib that the linker can't find.
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
add_subdirectory(glm)
add_subdirectory(spdlog)

if(MSVC)
    if(TARGET spdlog)
        target_precompile_headers(spdlog PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include/spdlog/common.h>"
        )
    endif()
endif()

# EASTL's CMakeLists uses add_library(EASTL ...) without STATIC/SHARED,
# so pin it to static build locally to avoid inheriting global BUILD_SHARED_LIBS.
set(BLUNDER_BUILD_SHARED_LIBS_SAVED ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(EASTL)
set(BUILD_SHARED_LIBS ${BLUNDER_BUILD_SHARED_LIBS_SAVED})
unset(BLUNDER_BUILD_SHARED_LIBS_SAVED)

add_subdirectory(assimp)
# CGLTF - Header-only glTF loader
# Instead of modifying the submodule, we generate the implementation file in the build directory
set(CGLTF_IMPL_FILE "${CMAKE_CURRENT_BINARY_DIR}/cgltf_impl.c")
file(WRITE "${CGLTF_IMPL_FILE}" "#define CGLTF_IMPLEMENTATION\n#include \"cgltf.h\"\n")
add_library(cgltf STATIC "${CGLTF_IMPL_FILE}")
target_include_directories(cgltf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cgltf)

# SDL3 - active windowing and event backend
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL)

if(MSVC)
    if(TARGET SDL3-shared)
        target_precompile_headers(SDL3-shared PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_SOURCE_DIR}/SDL/src/SDL_internal.h>"
        )
    endif()

    if(TARGET SDL_uclibc)
        target_precompile_headers(SDL_uclibc PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_SOURCE_DIR}/SDL/src/SDL_internal.h>"
        )
    endif()
endif()

# VMA - Vulkan Memory Allocator (header-only)
add_library(vma INTERFACE)
target_include_directories(vma INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator/include)

# STB - Single-file image loaders.
# stb is distributed as header-only sources; we generate a single
# implementation translation unit per used header so the symbols end up in
# exactly one static library that the rest of the engine can link against.
set(STB_IMPL_FILE "${CMAKE_CURRENT_BINARY_DIR}/stb_impl.c")
file(WRITE "${STB_IMPL_FILE}"
"#define STB_IMAGE_IMPLEMENTATION\n"
"#include \"stb_image.h\"\n"
"#define STB_IMAGE_WRITE_IMPLEMENTATION\n"
"#include \"stb_image_write.h\"\n")
add_library(stb STATIC "${STB_IMPL_FILE}")
target_include_directories(stb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/stb)
if(MSVC)
    # stb's C sources do not bother with the "secure" CRT helpers.
    target_compile_definitions(stb PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# KTX - Khronos Texture Container Library
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_VULKAN ON CACHE BOOL "" FORCE)
add_subdirectory(ktx)

# RecastNavigation - navigation mesh generation and query libraries
set(RECASTNAVIGATION_DEMO OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_TESTS OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_EXAMPLES OFF CACHE BOOL "" FORCE)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/recastnavigation/CMakeLists.txt")
    add_subdirectory(recastnavigation)
else()
    message(STATUS
        "[3rdparty] RecastNavigation submodule not present. Run: git submodule update --init --recursive engine/3rdparty/recastnavigation"
    )
endif()
