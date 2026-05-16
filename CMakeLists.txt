# 根 CMakeLists.txt

cmake_minimum_required(VERSION 4.0)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM "3.21" CACHE STRING
        "Minimum policy version for third-party subdirectories")
endif()

project(BlunderEngine
    LANGUAGES CXX
)

# ---------- C++ 标准配置 ----------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------- 平台 / 编译器统一配置 ----------
# 将 FetchContent 下载目录固定在项目根目录下的 .cmake_deps，避免删除 build 目录后重复下载
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.cmake_deps" CACHE PATH "Directory to store FetchContent data")

if(DEFINED CMAKE_GENERATOR_INSTANCE AND NOT CMAKE_GENERATOR_INSTANCE STREQUAL "")
    file(TO_CMAKE_PATH "${CMAKE_GENERATOR_INSTANCE}" _blunder_generator_instance)
    file(GLOB _blunder_fetchcontent_caches
        "${FETCHCONTENT_BASE_DIR}/*-subbuild/CMakeCache.txt"
    )

    foreach(_blunder_fetchcontent_cache IN LISTS _blunder_fetchcontent_caches)
        file(STRINGS "${_blunder_fetchcontent_cache}" _blunder_cached_generator_line
            REGEX "^CMAKE_GENERATOR_INSTANCE:INTERNAL="
            LIMIT_COUNT 1
        )

        if(NOT _blunder_cached_generator_line)
            continue()
        endif()

        string(REPLACE "CMAKE_GENERATOR_INSTANCE:INTERNAL=" ""
            _blunder_cached_generator_instance
            "${_blunder_cached_generator_line}"
        )
        file(TO_CMAKE_PATH "${_blunder_cached_generator_instance}" _blunder_cached_generator_instance)

        if(_blunder_cached_generator_instance STREQUAL _blunder_generator_instance)
            continue()
        endif()

        get_filename_component(_blunder_subbuild_dir "${_blunder_fetchcontent_cache}" DIRECTORY)
        get_filename_component(_blunder_subbuild_name "${_blunder_subbuild_dir}" NAME)
        string(REGEX REPLACE "-subbuild$" "" _blunder_dependency_name "${_blunder_subbuild_name}")
        set(_blunder_build_dir "${FETCHCONTENT_BASE_DIR}/${_blunder_dependency_name}-build")

        message(STATUS
            "[FetchContent] Removing stale ${_blunder_dependency_name} subbuild cached for generator instance "
            "${_blunder_cached_generator_instance}"
        )
        file(REMOVE_RECURSE "${_blunder_subbuild_dir}" "${_blunder_build_dir}")
    endforeach()
endif()

include(cmake/compiler.cmake)

set(BLUNDER_RENDER_BACKEND "vulkan" CACHE STRING "Engine RHI backend: vulkan | d3d12")

# ---------- 核心外部依赖 ----------
find_package(Vulkan REQUIRED)

# ---------- Slang Shader Compiler SDK ----------
include(cmake/slang.cmake)

# ---------- 子模块 ----------
add_subdirectory(engine)
