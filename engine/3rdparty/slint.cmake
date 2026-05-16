# engine/3rdparty/slint.cmake
# Builds Slint from the local submodule so the project can enable the Skia Vulkan renderer.

if(CMAKE_VERSION VERSION_LESS "3.21")
    message(FATAL_ERROR
        "[Slint] Source builds require CMake 3.21 or newer. "
        "Current version: ${CMAKE_VERSION}"
    )
endif()

set(SLINT_VERSION "1.16.1" CACHE STRING "Slint version to build from source")
set(SLINT_GIT_TAG "v${SLINT_VERSION}" CACHE STRING "Slint git tag or commit to fetch")
include(FetchContent)

set(BLUNDER_SLINT_SKIA_BINARIES_URL "" CACHE STRING
    "Optional SKIA_BINARIES_URL template for rust-skia (expects {tag} and {key})")
# Full Skia source builds on Windows routinely hit MAX_PATH in ninja (harfbuzz
# paths under SKIA_SOURCE_DIR). Prebuilt skia-binaries include Vulkan; use them
# unless you have very short absolute paths and LLVM for a deliberate full build.
set(BLUNDER_SLINT_FORCE_SKIA_BUILD OFF CACHE BOOL
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
set(BLUNDER_SLINT_RUSTC "" CACHE FILEPATH
    "Optional path to rustc for Slint source builds")
set(BLUNDER_SLINT_CARGO "" CACHE FILEPATH
    "Optional path to cargo for Slint source builds")

set(_blunder_slint_rust_bin_hints)
if(DEFINED ENV{CARGO_HOME})
    list(APPEND _blunder_slint_rust_bin_hints "$ENV{CARGO_HOME}/bin")
endif()
if(DEFINED ENV{USERPROFILE})
    list(APPEND _blunder_slint_rust_bin_hints "$ENV{USERPROFILE}/.cargo/bin")
endif()
if(DEFINED ENV{HOME})
    list(APPEND _blunder_slint_rust_bin_hints "$ENV{HOME}/.cargo/bin")
endif()

find_program(SLINT_GIT_EXECUTABLE NAMES git)
find_program(SLINT_RUSTC_EXECUTABLE NAMES rustc rustc.exe
    HINTS ${_blunder_slint_rust_bin_hints}
)
find_program(SLINT_CARGO_EXECUTABLE NAMES cargo cargo.exe
    HINTS ${_blunder_slint_rust_bin_hints}
)

if(BLUNDER_SLINT_RUSTC)
    if(NOT EXISTS "${BLUNDER_SLINT_RUSTC}")
        message(FATAL_ERROR
            "[Slint] BLUNDER_SLINT_RUSTC does not exist: ${BLUNDER_SLINT_RUSTC}"
        )
    endif()
    set(SLINT_RUSTC_EXECUTABLE "${BLUNDER_SLINT_RUSTC}")
endif()
if(BLUNDER_SLINT_CARGO)
    if(NOT EXISTS "${BLUNDER_SLINT_CARGO}")
        message(FATAL_ERROR
            "[Slint] BLUNDER_SLINT_CARGO does not exist: ${BLUNDER_SLINT_CARGO}"
        )
    endif()
    set(SLINT_CARGO_EXECUTABLE "${BLUNDER_SLINT_CARGO}")
endif()

if(NOT SLINT_GIT_EXECUTABLE)
    message(FATAL_ERROR
        "[Slint] Source builds require Git in PATH."
    )
endif()

if(NOT SLINT_RUSTC_EXECUTABLE OR NOT SLINT_CARGO_EXECUTABLE)
    message(FATAL_ERROR
        "[Slint] Source builds require Rust and Cargo.\n"
        "  rustc: ${SLINT_RUSTC_EXECUTABLE}\n"
        "  cargo: ${SLINT_CARGO_EXECUTABLE}\n"
        "Install Rust 1.88 or newer from https://rustup.rs/ .\n"
        "If rustup is already installed, add %USERPROFILE%\\.cargo\\bin to PATH for this shell, "
        "or reconfigure with -DBLUNDER_SLINT_RUSTC=... -DBLUNDER_SLINT_CARGO=... ."
    )
endif()

get_filename_component(_blunder_slint_rust_bin_dir "${SLINT_CARGO_EXECUTABLE}" DIRECTORY)

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

set(_blunder_slint_ninja_hints)
if(CMAKE_GENERATOR_INSTANCE)
    list(APPEND _blunder_slint_ninja_hints
        "${CMAKE_GENERATOR_INSTANCE}/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"
    )
endif()
list(APPEND _blunder_slint_ninja_hints
    "C:/Program Files (x86)/Microsoft Visual Studio/2026/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"
    "C:/Program Files/Microsoft Visual Studio/2026/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"
    "C:/Program Files (x86)/Microsoft Visual Studio/2026/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"
    "C:/Program Files/Microsoft Visual Studio/2026/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"
    "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"
    "C:/Program Files/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"
)

find_program(_blunder_slint_detected_ninja_command
    NAMES ninja ninja.exe
    HINTS ${_blunder_slint_ninja_hints}
    NO_DEFAULT_PATH
)

if(BLUNDER_SLINT_NINJA_COMMAND)
    set(_blunder_slint_effective_ninja_command "${BLUNDER_SLINT_NINJA_COMMAND}")
elseif(CMAKE_GENERATOR_INSTANCE)
    set(_blunder_slint_vs_ninja
        "${CMAKE_GENERATOR_INSTANCE}/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
    )
    if(EXISTS "${_blunder_slint_vs_ninja}")
        set(_blunder_slint_effective_ninja_command "${_blunder_slint_vs_ninja}")
    endif()
endif()
if(NOT _blunder_slint_effective_ninja_command)
    set(_blunder_slint_effective_ninja_command "${_blunder_slint_detected_ninja_command}")
endif()

set(SLINT_FEATURE_RENDERER_SKIA ON CACHE BOOL "Enable Slint Skia renderer" FORCE)
set(SLINT_FEATURE_RENDERER_SKIA_VULKAN ON CACHE BOOL "Enable Slint Skia Vulkan renderer" FORCE)
set(SLINT_FEATURE_RENDERER_SKIA_OPENGL OFF CACHE BOOL "Disable Slint Skia OpenGL renderer" FORCE)

set(_slint_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/slint")
if(NOT EXISTS "${_slint_source_dir}/.git" OR NOT EXISTS "${_slint_source_dir}/api/cpp/CMakeLists.txt")
    message(FATAL_ERROR
        "[Slint] Missing engine/3rdparty/slint submodule. Run: git submodule update --init --recursive engine/3rdparty/slint"
    )
endif()

execute_process(
    COMMAND "${SLINT_GIT_EXECUTABLE}" -C "${_slint_source_dir}" describe --tags --exact-match
    OUTPUT_VARIABLE _slint_existing_tag
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _slint_existing_tag_result
    ERROR_QUIET
)

if(NOT _slint_existing_tag_result EQUAL 0 OR NOT _slint_existing_tag STREQUAL "${SLINT_GIT_TAG}")
    message(FATAL_ERROR
        "[Slint] Expected submodule ${_slint_source_dir} to be checked out at ${SLINT_GIT_TAG}."
    )
endif()

set(FETCHCONTENT_QUIET OFF)

# Corrosion's FindRust.cmake only checks $ENV{HOME}/.cargo/bin when rustc is not in
# PATH. On Windows HOME is often unset, so seed it from USERPROFILE and pass explicit
# tool paths into the Slint/Corrosion subproject.
if(NOT DEFINED ENV{HOME} AND DEFINED ENV{USERPROFILE})
    set(ENV{HOME} "$ENV{USERPROFILE}")
endif()

if(CMAKE_HOST_WIN32)
    set(ENV{PATH} "${_blunder_slint_rust_bin_dir};$ENV{PATH}")
else()
    set(ENV{PATH} "${_blunder_slint_rust_bin_dir}:$ENV{PATH}")
endif()

set(Rust_COMPILER "${SLINT_RUSTC_EXECUTABLE}" CACHE FILEPATH
    "Path to rustc for Corrosion/Slint (set by Blunder Engine)" FORCE)
set(Rust_CARGO "${SLINT_CARGO_EXECUTABLE}" CACHE FILEPATH
    "Path to cargo for Corrosion/Slint (set by Blunder Engine)" FORCE)

include("${CMAKE_SOURCE_DIR}/cmake/ensure_skia_binaries.cmake")
set(_blunder_resolved_skia_binaries_url "")
blunder_ensure_skia_binaries(_blunder_resolved_skia_binaries_url)
if(_blunder_resolved_skia_binaries_url)
    set(BLUNDER_SLINT_SKIA_BINARIES_URL "${_blunder_resolved_skia_binaries_url}"
        CACHE STRING
        "Optional SKIA_BINARIES_URL template for rust-skia (expects {tag} and {key})"
        FORCE
    )
endif()

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
    "${PROJECT_SOURCE_DIR}/.cmake_deps/slint-sdk-${SLINT_VERSION}/install/include/slint/private")

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
    # Do not set PATH via CORROSION_ENVIRONMENT_VARIABLES: semicolons in
    # $ENV{PATH} break cmake -E env under the VS generator (MSB8066 / "no such
    # file or directory"). Rust/Cargo are passed via Rust_COMPILER/Rust_CARGO cache
    # and CARGO_BUILD_RUSTC in the corrosion cargo invocation.

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

    if(WIN32 AND BLUNDER_SLINT_FORCE_SKIA_BUILD)
        message(STATUS
            "[Slint] Windows full Skia build: use short paths (e.g. subst S: D:\\skia-m142, "
            "BLUNDER_SLINT_CARGO_TARGET_DIR=D:/ct, BLUNDER_SLINT_SKIA_BUILD_OUTPUT_DIR=S:/out). "
            "Enable Windows Developer Mode so Skia dependency symlinks can be created. "
            "Offline source must be rust-skia/skia tag m142-0.89.1, not google/skia main."
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
        if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/patch_skia_for_rust_skia.ps1")
            set(_blunder_slint_patch_script
                "${CMAKE_CURRENT_SOURCE_DIR}/patch_skia_for_rust_skia.ps1")
            execute_process(
                COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
                    "${_blunder_slint_patch_script}"
                    -SkiaRoot "${BLUNDER_SLINT_SKIA_SOURCE_DIR}"
                RESULT_VARIABLE _blunder_slint_patch_result
            )
            if(NOT _blunder_slint_patch_result EQUAL 0)
                message(WARNING
                    "[Slint] patch_skia_for_rust_skia.ps1 failed (${_blunder_slint_patch_result}). "
                    "You may need to run it manually on ${BLUNDER_SLINT_SKIA_SOURCE_DIR}."
                )
            else()
                message(STATUS "[Slint] Applied Windows gn compatibility patches under ${BLUNDER_SLINT_SKIA_SOURCE_DIR}")
            endif()
        endif()
        if(EXISTS "${BLUNDER_SLINT_SKIA_SOURCE_DIR}/DEPS")
            if(NOT EXISTS "${BLUNDER_SLINT_SKIA_SOURCE_DIR}/gn/skia/BUILD.gn")
                message(WARNING
                    "[Slint] BLUNDER_SLINT_SKIA_SOURCE_DIR does not look like a rust-skia Skia tree (missing gn/skia/BUILD.gn). "
                    "skia-bindings 0.90 expects tag m142-0.89.1 from https://github.com/rust-skia/skia ."
                )
            endif()
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
            else()
                message(FATAL_ERROR
                    "[Slint] BLUNDER_SLINT_SKIA_SOURCE_DIR=${BLUNDER_SLINT_SKIA_SOURCE_DIR} has no bin/gn or bin/gn.exe. "
                    "Run: python bin/fetch-gn (from the Skia tree) or copy gn.exe into bin/."
                )
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
