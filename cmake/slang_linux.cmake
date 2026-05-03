# cmake/slang_linux.cmake
# Linux adaptation: uses pre-extracted Slang SDK tarball
# (The original slang.cmake only supports Windows)

include(FetchContent)

set(SLANG_VERSION "2026.5.2" CACHE STRING "Slang release version to download")

set(_slang_arch "x86_64")
set(_slang_archive "slang-${SLANG_VERSION}-linux-${_slang_arch}.tar.gz")
set(_slang_url "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/${_slang_archive}")

set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
  slang_sdk
  URL "${_slang_url}"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(slang_sdk)

set(_slang_root "${slang_sdk_SOURCE_DIR}")

find_file(SLANG_SO_PATH
  NAMES libslang.so
  PATHS "${_slang_root}/lib" "${_slang_root}"
  NO_DEFAULT_PATH
)

find_path(SLANG_INCLUDE_DIR
  NAMES slang.h
  PATHS "${_slang_root}/include" "${_slang_root}"
  NO_DEFAULT_PATH
)

if(NOT SLANG_SO_PATH OR NOT SLANG_INCLUDE_DIR)
  message(FATAL_ERROR
    "Slang SDK layout not found in ${_slang_root}.\n"
    "  SO:  ${SLANG_SO_PATH}\n"
    "  INC: ${SLANG_INCLUDE_DIR}\n"
  )
endif()

if(NOT TARGET slang::slang)
  add_library(slang::slang SHARED IMPORTED GLOBAL)
  set_target_properties(slang::slang PROPERTIES
    IMPORTED_LOCATION "${SLANG_SO_PATH}"
    IMPORTED_NO_SONAME TRUE
    INTERFACE_INCLUDE_DIRECTORIES "${SLANG_INCLUDE_DIR}"
  )
endif()

message(STATUS "Slang SDK: ${_slang_root}")
message(STATUS "  SO:  ${SLANG_SO_PATH}")
message(STATUS "  INC: ${SLANG_INCLUDE_DIR}")
