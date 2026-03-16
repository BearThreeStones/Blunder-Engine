# engine/3rdparty/CMakeLists.txt

add_subdirectory(glm)
add_subdirectory(glfw)
add_subdirectory(spdlog)
add_subdirectory(imgui)
add_subdirectory(EASTL)
add_subdirectory(assimp)
add_subdirectory(cgltf)

# SDL3 - only events subsystem needed (windowing handled by GLFW)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL)

# VMA - Vulkan Memory Allocator (header-only)
add_library(vma INTERFACE)
target_include_directories(vma INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator/include)