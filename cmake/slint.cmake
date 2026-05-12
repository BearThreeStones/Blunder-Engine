# cmake/slint.cmake
# Builds Slint from source so the project can enable the Skia Vulkan renderer.

if(CMAKE_VERSION VERSION_LESS "3.21")
    message(FATAL_ERROR
        "[Slint] Source builds require CMake 3.21 or newer. "
        "Current version: ${CMAKE_VERSION}"
    )
endif()

include(FetchContent)

set(SLINT_VERSION "1.16.1" CACHE STRING "Slint version to build from source")
set(SLINT_GIT_TAG "v${SLINT_VERSION}" CACHE STRING "Slint git tag or commit to fetch")
set(BLUNDER_SLINT_SKIA_BINARIES_URL "" CACHE STRING
    "Optional SKIA_BINARIES_URL template for rust-skia (expects {tag} and {key})")
set(_blunder_slint_default_force_skia_build OFF)
if(WIN32 AND SLINT_FEATURE_RENDERER_SKIA_VULKAN)
    set(_blunder_slint_default_force_skia_build ON)
endif()
set(BLUNDER_SLINT_FORCE_SKIA_BUILD ${_blunder_slint_default_force_skia_build} CACHE BOOL
    "Force rust-skia to build Skia from source instead of downloading prebuilt binaries")
set(BLUNDER_SLINT_LLVM_HOME "" CACHE PATH
    "Optional LLVM root directory for rust-skia full builds (must contain bin/clang-cl.exe)")
set(BLUNDER_SLINT_NINJA_COMMAND "" CACHE FILEPATH
    "Optional ninja executable path for rust-skia full builds")
set(BLUNDER_SLINT_SKIA_SOURCE_DIR "" CACHE PATH
    "Optional pre-synced Skia source tree for rust-skia offline builds")
set(BLUNDER_SLINT_CARGO_TARGET_DIR "" CACHE PATH
    "Optional short Cargo target directory for rust-skia Windows builds")
set(BLUNDER_SLINT_SKIA_BUILD_OUTPUT_DIR "" CACHE PATH
    "Optional short gn/ninja output directory for rust-skia full builds")

set(_blunder_slint_git_executable_override "${CMAKE_SOURCE_DIR}/cmake/git-network-wrapper.bat")

find_program(SLINT_GIT_EXECUTABLE NAMES git)
find_program(SLINT_RUSTC_EXECUTABLE NAMES rustc)
find_program(SLINT_CARGO_EXECUTABLE NAMES cargo)

if(NOT SLINT_GIT_EXECUTABLE)
    message(FATAL_ERROR
        "[Slint] Source builds require Git in PATH."
    )
endif()

if(NOT SLINT_RUSTC_EXECUTABLE OR NOT SLINT_CARGO_EXECUTABLE)
    message(FATAL_ERROR
        "[Slint] Source builds require Rust and Cargo in PATH.\n"
        "  rustc: ${SLINT_RUSTC_EXECUTABLE}\n"
        "  cargo: ${SLINT_CARGO_EXECUTABLE}\n"
        "Install Rust 1.88 or newer from https://rustup.rs/ and re-run CMake."
    )
endif()

execute_process(
    COMMAND "${SLINT_RUSTC_EXECUTABLE}" --version
    OUTPUT_VARIABLE _slint_rustc_version_output
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _slint_rustc_result
)

if(NOT _slint_rustc_result EQUAL 0)
    message(FATAL_ERROR
        "[Slint] Failed to query rustc version from ${SLINT_RUSTC_EXECUTABLE}."
    )
endif()

string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" _slint_rustc_version
       "${_slint_rustc_version_output}")
if(NOT _slint_rustc_version)
    message(FATAL_ERROR
        "[Slint] Could not parse rustc version from: ${_slint_rustc_version_output}"
    )
endif()

if(_slint_rustc_version VERSION_LESS "1.88.0")
    message(FATAL_ERROR
        "[Slint] rustc ${_slint_rustc_version} is too old. Rust 1.88 or newer is required."
    )
endif()

execute_process(
    COMMAND "${SLINT_CARGO_EXECUTABLE}" --version
    OUTPUT_VARIABLE _slint_cargo_version_output
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _slint_cargo_result
)

if(NOT _slint_cargo_result EQUAL 0)
    message(FATAL_ERROR
        "[Slint] Failed to query cargo version from ${SLINT_CARGO_EXECUTABLE}."
    )
endif()

set(_blunder_slint_llvm_candidates
    "${BLUNDER_SLINT_LLVM_HOME}"
    "$ENV{LLVM_HOME}"
    "$ENV{LOCALAPPDATA}/Programs/LLVM"
    "$ENV{LOCALAPPDATA}/Programs/LLVM-portable"
    "C:/Program Files/LLVM"
    "C:/LLVM"
)

file(GLOB _blunder_slint_llvm_portable_candidates
    "$ENV{LOCALAPPDATA}/Programs/LLVM-portable/*"
)
list(APPEND _blunder_slint_llvm_candidates ${_blunder_slint_llvm_portable_candidates})

if(CMAKE_GENERATOR_INSTANCE)
    list(APPEND _blunder_slint_llvm_candidates
        "${CMAKE_GENERATOR_INSTANCE}/VC/Tools/Llvm/x64"
    )
endif()

set(_blunder_slint_effective_llvm_home "")
foreach(_blunder_slint_llvm_candidate IN LISTS _blunder_slint_llvm_candidates)
    if(_blunder_slint_llvm_candidate AND EXISTS "${_blunder_slint_llvm_candidate}/bin/clang-cl.exe")
        set(_blunder_slint_effective_llvm_home "${_blunder_slint_llvm_candidate}")
        break()
    endif()
endforeach()

set(_blunder_slint_ninja_hints
    "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"
    "C:/Program Files/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"
)

if(CMAKE_GENERATOR_INSTANCE)
    list(APPEND _blunder_slint_ninja_hints
        "${CMAKE_GENERATOR_INSTANCE}/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"
    )
endif()

find_program(_blunder_slint_detected_ninja_command
    NAMES ninja ninja.exe
    HINTS ${_blunder_slint_ninja_hints}
)

set(_blunder_slint_effective_ninja_command "${BLUNDER_SLINT_NINJA_COMMAND}")
if(NOT _blunder_slint_effective_ninja_command)
    set(_blunder_slint_effective_ninja_command "${_blunder_slint_detected_ninja_command}")
endif()

set(SLINT_FEATURE_RENDERER_SKIA ON CACHE BOOL "Enable Slint Skia renderer" FORCE)
set(SLINT_FEATURE_RENDERER_SKIA_VULKAN ON CACHE BOOL "Enable Slint Skia Vulkan renderer" FORCE)
set(SLINT_FEATURE_RENDERER_SKIA_OPENGL OFF CACHE BOOL "Disable Slint Skia OpenGL renderer" FORCE)

set(_slint_deps_dir "${CMAKE_SOURCE_DIR}/.cmake_deps")
set(_slint_source_dir "${_slint_deps_dir}/slint-src")

file(MAKE_DIRECTORY "${_slint_deps_dir}")

set(_slint_needs_clone TRUE)

if(EXISTS "${_slint_source_dir}/.git" AND EXISTS "${_slint_source_dir}/api/cpp/CMakeLists.txt")
    execute_process(
        COMMAND "${SLINT_GIT_EXECUTABLE}" -C "${_slint_source_dir}" describe --tags --exact-match
        OUTPUT_VARIABLE _slint_existing_tag
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _slint_existing_tag_result
        ERROR_QUIET
    )

    if(_slint_existing_tag_result EQUAL 0 AND _slint_existing_tag STREQUAL "${SLINT_GIT_TAG}")
        set(_slint_needs_clone FALSE)
    endif()
endif()

if(_slint_needs_clone)
    message(STATUS "[Slint] Preparing local source checkout for ${SLINT_GIT_TAG}")
    file(REMOVE_RECURSE "${_slint_source_dir}")

    execute_process(
        COMMAND "${SLINT_GIT_EXECUTABLE}"
            -c http.version=HTTP/1.1
            -c http.sslBackend=schannel
            -c http.schannelCheckRevoke=false
            clone
            --depth 1
            --branch "${SLINT_GIT_TAG}"
            https://github.com/slint-ui/slint.git
            "${_slint_source_dir}"
        WORKING_DIRECTORY "${_slint_deps_dir}"
        RESULT_VARIABLE _slint_clone_result
        OUTPUT_VARIABLE _slint_clone_output
        ERROR_VARIABLE _slint_clone_error
    )

    if(NOT _slint_clone_result EQUAL 0)
        message(FATAL_ERROR
            "[Slint] Failed to clone ${SLINT_GIT_TAG} into ${_slint_source_dir}.\n"
            "stdout:\n${_slint_clone_output}\n"
            "stderr:\n${_slint_clone_error}"
        )
    endif()
else()
    message(STATUS "[Slint] Reusing local source checkout: ${_slint_source_dir}")
endif()

file(REMOVE_RECURSE
    "${_slint_deps_dir}/slint-subbuild"
    "${_slint_deps_dir}/slint-build"
)

set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    Slint
    SOURCE_DIR "${_slint_source_dir}"
    SOURCE_SUBDIR api/cpp
)

FetchContent_MakeAvailable(Slint)

set(_blunder_slint_generated_private_dir "${slint_BINARY_DIR}/generated_include/private")
set(_blunder_slint_generated_internal_header
    "${_blunder_slint_generated_private_dir}/slint_internal.h")
set(_blunder_slint_sdk_private_headers_dir
    "${CMAKE_SOURCE_DIR}/.cmake_deps/slint-sdk-${SLINT_VERSION}/install/include/slint/private")

if(NOT EXISTS "${_blunder_slint_generated_internal_header}")
    if(EXISTS "${_blunder_slint_sdk_private_headers_dir}/slint_internal.h")
        file(MAKE_DIRECTORY "${_blunder_slint_generated_private_dir}")
        file(COPY "${_blunder_slint_sdk_private_headers_dir}/"
            DESTINATION "${_blunder_slint_generated_private_dir}"
            FILES_MATCHING PATTERN "*.h"
        )
        message(WARNING
            "[Slint] Missing generated C++ headers under ${slint_BINARY_DIR}/generated_include. "
            "Seeded them from ${_blunder_slint_sdk_private_headers_dir} so incremental source builds can continue. "
            "Run the cargo-clean_slint_cpp target if you need the headers regenerated from the current Rust build."
        )
    endif()
endif()

if(TARGET slint_cpp)
    if(BLUNDER_SLINT_SKIA_BINARIES_URL)
        set_property(
            TARGET slint_cpp
            APPEND
            PROPERTY CORROSION_ENVIRONMENT_VARIABLES
            "SKIA_BINARIES_URL=${BLUNDER_SLINT_SKIA_BINARIES_URL}"
        )
        message(STATUS
            "[Slint] Using custom SKIA_BINARIES_URL template: ${BLUNDER_SLINT_SKIA_BINARIES_URL}"
        )
    endif()

    if(BLUNDER_SLINT_FORCE_SKIA_BUILD)
        set_property(
            TARGET slint_cpp
            APPEND
            PROPERTY CORROSION_ENVIRONMENT_VARIABLES
            "FORCE_SKIA_BUILD=1"
        )
        message(STATUS "[Slint] Forcing rust-skia full build")
    endif()

    if(_blunder_slint_effective_llvm_home)
        set_property(
            TARGET slint_cpp
            APPEND
            PROPERTY CORROSION_ENVIRONMENT_VARIABLES
            "LLVM_HOME=${_blunder_slint_effective_llvm_home}"
        )
        set_property(
            TARGET slint_cpp
            APPEND
            PROPERTY CORROSION_ENVIRONMENT_VARIABLES
            "LIBCLANG_PATH=${_blunder_slint_effective_llvm_home}/bin"
        )
        message(STATUS "[Slint] Using LLVM_HOME: ${_blunder_slint_effective_llvm_home}")
        message(STATUS "[Slint] Using LIBCLANG_PATH: ${_blunder_slint_effective_llvm_home}/bin")
    elseif(BLUNDER_SLINT_FORCE_SKIA_BUILD)
        message(WARNING
            "[Slint] FORCE_SKIA_BUILD is enabled, but no LLVM root containing bin/clang-cl.exe was found. "
            "Set BLUNDER_SLINT_LLVM_HOME or LLVM_HOME before building."
        )
    endif()

    if(_blunder_slint_effective_ninja_command)
        set_property(
            TARGET slint_cpp
            APPEND
            PROPERTY CORROSION_ENVIRONMENT_VARIABLES
            "SKIA_NINJA_COMMAND=${_blunder_slint_effective_ninja_command}"
        )
        message(STATUS "[Slint] Using Skia ninja: ${_blunder_slint_effective_ninja_command}")
    elseif(BLUNDER_SLINT_FORCE_SKIA_BUILD)
        message(WARNING
            "[Slint] FORCE_SKIA_BUILD is enabled, but no ninja executable was found for Skia. "
            "Set BLUNDER_SLINT_NINJA_COMMAND if auto-detection fails."
        )
    endif()

    if(BLUNDER_SLINT_SKIA_SOURCE_DIR)
        if(EXISTS "${BLUNDER_SLINT_SKIA_SOURCE_DIR}/DEPS")
            set_property(
                TARGET slint_cpp
                APPEND
                PROPERTY CORROSION_ENVIRONMENT_VARIABLES
                "SKIA_SOURCE_DIR=${BLUNDER_SLINT_SKIA_SOURCE_DIR}"
            )
            message(STATUS "[Slint] Using offline Skia source: ${BLUNDER_SLINT_SKIA_SOURCE_DIR}")

            if(EXISTS "${BLUNDER_SLINT_SKIA_SOURCE_DIR}/bin/gn")
                set_property(
                    TARGET slint_cpp
                    APPEND
                    PROPERTY CORROSION_ENVIRONMENT_VARIABLES
                    "SKIA_GN_COMMAND=${BLUNDER_SLINT_SKIA_SOURCE_DIR}/bin/gn"
                )
                message(STATUS "[Slint] Using Skia gn: ${BLUNDER_SLINT_SKIA_SOURCE_DIR}/bin/gn")
            elseif(EXISTS "${BLUNDER_SLINT_SKIA_SOURCE_DIR}/bin/gn.exe")
                set_property(
                    TARGET slint_cpp
                    APPEND
                    PROPERTY CORROSION_ENVIRONMENT_VARIABLES
                    "SKIA_GN_COMMAND=${BLUNDER_SLINT_SKIA_SOURCE_DIR}/bin/gn.exe"
                )
                message(STATUS "[Slint] Using Skia gn: ${BLUNDER_SLINT_SKIA_SOURCE_DIR}/bin/gn.exe")
            endif()
        else()
            message(WARNING
                "[Slint] BLUNDER_SLINT_SKIA_SOURCE_DIR is set, but ${BLUNDER_SLINT_SKIA_SOURCE_DIR} does not look like a Skia checkout."
            )
        endif()
    endif()

    if(BLUNDER_SLINT_CARGO_TARGET_DIR)
        set_property(
            TARGET slint_cpp
            APPEND
            PROPERTY CORROSION_ENVIRONMENT_VARIABLES
            "CARGO_TARGET_DIR=${BLUNDER_SLINT_CARGO_TARGET_DIR}"
        )
        message(STATUS "[Slint] Using Cargo target dir: ${BLUNDER_SLINT_CARGO_TARGET_DIR}")
    endif()

    if(BLUNDER_SLINT_SKIA_BUILD_OUTPUT_DIR)
        set_property(
            TARGET slint_cpp
            APPEND
            PROPERTY CORROSION_ENVIRONMENT_VARIABLES
            "SKIA_BUILD_OUTPUT_DIR=${BLUNDER_SLINT_SKIA_BUILD_OUTPUT_DIR}"
        )
        message(STATUS "[Slint] Using Skia build output dir: ${BLUNDER_SLINT_SKIA_BUILD_OUTPUT_DIR}")
    endif()

    if(EXISTS "${_blunder_slint_git_executable_override}")
        set_property(
            TARGET slint_cpp
            APPEND
            PROPERTY CORROSION_ENVIRONMENT_VARIABLES
            "GIT_EXECUTABLE=${_blunder_slint_git_executable_override}"
        )
        message(STATUS "[Slint] Using git network wrapper: ${_blunder_slint_git_executable_override}")
    endif()
elseif(BLUNDER_SLINT_SKIA_BINARIES_URL OR BLUNDER_SLINT_FORCE_SKIA_BUILD
    OR BLUNDER_SLINT_LLVM_HOME OR BLUNDER_SLINT_NINJA_COMMAND)
    message(WARNING
        "[Slint] rust-skia environment overrides were requested, but slint_cpp is unavailable."
    )
endif()

if(NOT TARGET Slint::Slint)
    message(FATAL_ERROR
        "[Slint] Source build completed without defining Slint::Slint."
    )
endif()

get_target_property(_slint_enabled_features Slint::Slint SLINT_ENABLED_FEATURES)
get_target_property(_slint_disabled_features Slint::Slint SLINT_DISABLED_FEATURES)

if(NOT _slint_enabled_features)
    set(_slint_enabled_features "")
endif()

if(NOT _slint_disabled_features)
    set(_slint_disabled_features "")
endif()

message(STATUS "[Slint] Source build tag: ${SLINT_GIT_TAG}")
message(STATUS "[Slint] rustc: ${_slint_rustc_version_output}")
message(STATUS "[Slint] cargo: ${_slint_cargo_version_output}")
message(STATUS "[Slint] Enabled features: ${_slint_enabled_features}")
message(STATUS "[Slint] Disabled features: ${_slint_disabled_features}")

if("RENDERER_SKIA_VULKAN" IN_LIST _slint_disabled_features)
    message(FATAL_ERROR
        "[Slint] RENDERER_SKIA_VULKAN is still disabled after source configuration."
    )
endif()

if(NOT "RENDERER_SKIA_VULKAN" IN_LIST _slint_enabled_features)
    message(FATAL_ERROR
        "[Slint] RENDERER_SKIA_VULKAN was not enabled by the source configuration."
    )
endif()

if(NOT "RENDERER_SKIA" IN_LIST _slint_enabled_features)
    message(FATAL_ERROR
        "[Slint] RENDERER_SKIA must be enabled for the editor UI."
    )
endif()
