# Linux development wrapper for BlunderEngine
# This file mirrors root CMakelists.txt but uses Linux-compatible SDK scripts.

cmake_minimum_required(VERSION 3.5)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
endif()

project(BlunderEngine LANGUAGES C CXX)

# GNU ld does not rescan static archives. engine_runtime's global_context.cpp
# needs blunder_native_abi_fill_from_process from blunder_engine_c_static;
# CMake de-duplicates a second C-ABI on the line, so repeating the lib is a
# no-op. Wrap executable links so the pair is searched until settled.
# project() already filled CMAKE_<LANG>_LINK_EXECUTABLE for this compiler.
foreach(_blunder_link_lang IN ITEMS C CXX)
  set(_blunder_link_rule "${CMAKE_${_blunder_link_lang}_LINK_EXECUTABLE}")
  if(_blunder_link_rule)
    string(REPLACE "<LINK_LIBRARIES>"
      "-Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group"
      _blunder_link_rule
      "${_blunder_link_rule}")
    set(CMAKE_${_blunder_link_lang}_LINK_EXECUTABLE "${_blunder_link_rule}")
  endif()
endforeach()
unset(_blunder_link_lang)
unset(_blunder_link_rule)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# SHARED blunder_engine_c links STATIC engine_runtime. ELF TLS/code in that
# archive needs -fPIC (MSVC does not).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.cmake_deps" CACHE PATH "Directory to store FetchContent data")
include(cmake/compiler.cmake)
include(cmake/blunder_outputs.cmake)

find_package(Vulkan REQUIRED)

include(cmake/slint_linux.cmake)
include(cmake/slang_linux.cmake)

# Root CTestTestfile.cmake so `ctest --test-dir build` sees engine/src/tests.
# enable_testing() only under tests/ leaves the build-tree root with no tests.
enable_testing()

add_subdirectory(engine)
