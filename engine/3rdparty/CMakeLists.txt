# engine/3rdparty/CMakeLists.txt

add_subdirectory(glm)
add_subdirectory(spdlog)

# EASTL's CMakeLists uses add_library(EASTL ...) without STATIC/SHARED,
# so pin it to static build locally to avoid inheriting global BUILD_SHARED_LIBS.
set(BLUNDER_BUILD_SHARED_LIBS_SAVED ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(EASTL)
set(BUILD_SHARED_LIBS ${BLUNDER_BUILD_SHARED_LIBS_SAVED})
unset(BLUNDER_BUILD_SHARED_LIBS_SAVED)

add_subdirectory(assimp)
add_subdirectory(cgltf)

# SDL3 - active windowing and event backend
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL)

# VMA - Vulkan Memory Allocator (header-only)
add_library(vma INTERFACE)
target_include_directories(vma INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator/include)

# STB - Single-file image loaders (header-only)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/stb)

# KTX - Khronos Texture Container Library
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_VULKAN ON CACHE BOOL "" FORCE)
add_subdirectory(ktx)
