# Linux development wrapper for BlunderEngine
# This file mirrors root CMakelists.txt but uses Linux-compatible SDK scripts.

cmake_minimum_required(VERSION 3.5)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
endif()

project(BlunderEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.cmake_deps" CACHE PATH "Directory to store FetchContent data")
include(cmake/compiler.cmake)

find_package(Vulkan REQUIRED)

include(cmake/slint_linux.cmake)
include(cmake/slang_linux.cmake)

add_subdirectory(engine)
