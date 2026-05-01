# cmake/slang.cmake
# Downloads pre-built Slang compiler SDK and creates imported targets.
#
# Provides: slang::slang (IMPORTED SHARED library)
# Also defines SLANG_DLL_PATH for post-build copy.

include(FetchContent)

set(SLANG_VERSION "2026.5.2" CACHE STRING "Slang release version to download")

# Determine platform-specific release archive
if(WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_slang_arch "x86_64")
  else()
    set(_slang_arch "x86")
  endif()
  set(_slang_archive "slang-${SLANG_VERSION}-windows-${_slang_arch}.zip")
else()
  message(FATAL_ERROR "Slang pre-built binaries: only Windows is supported in this project")
endif()

set(_slang_url "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/${_slang_archive}")

set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
  slang_sdk
  URL "${_slang_url}"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  LOG_DOWNLOAD ON
  USES_TERMINAL_DOWNLOAD ON
)

FetchContent_MakeAvailable(slang_sdk)

# Find the extracted SDK layout
# Slang releases typically have: bin/, lib/, include/ at the root
set(_slang_root "${slang_sdk_SOURCE_DIR}")

# Locate key files
find_file(SLANG_DLL_PATH
  NAMES slang.dll
  PATHS "${_slang_root}/bin" "${_slang_root}"
  NO_DEFAULT_PATH
)

find_library(SLANG_IMPORT_LIB
  NAMES slang
  PATHS "${_slang_root}/lib" "${_slang_root}"
  NO_DEFAULT_PATH
)

find_path(SLANG_INCLUDE_DIR
  NAMES slang.h
  PATHS "${_slang_root}/include" "${_slang_root}"
  NO_DEFAULT_PATH
)

if(NOT SLANG_DLL_PATH OR NOT SLANG_IMPORT_LIB OR NOT SLANG_INCLUDE_DIR)
  message(FATAL_ERROR
    "Slang SDK layout not found in ${_slang_root}.\n"
    "  DLL: ${SLANG_DLL_PATH}\n"
    "  LIB: ${SLANG_IMPORT_LIB}\n"
    "  INC: ${SLANG_INCLUDE_DIR}\n"
    "Check the release archive layout matches expectations."
  )
endif()

# Create imported target
if(NOT TARGET slang::slang)
  add_library(slang::slang SHARED IMPORTED GLOBAL)
  set_target_properties(slang::slang PROPERTIES
    IMPORTED_LOCATION "${SLANG_DLL_PATH}"
    IMPORTED_IMPLIB "${SLANG_IMPORT_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${SLANG_INCLUDE_DIR}"
  )
endif()

# Also find slang-glslang.dll if present (optional runtime dependency)
find_file(SLANG_GLSLANG_DLL_PATH
  NAMES slang-glslang.dll
  PATHS "${_slang_root}/bin" "${_slang_root}"
  NO_DEFAULT_PATH
)

message(STATUS "Slang SDK: ${_slang_root}")
message(STATUS "  DLL: ${SLANG_DLL_PATH}")
message(STATUS "  LIB: ${SLANG_IMPORT_LIB}")
message(STATUS "  INC: ${SLANG_INCLUDE_DIR}")
