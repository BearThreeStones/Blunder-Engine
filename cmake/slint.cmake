# cmake/slint.cmake
# Downloads pre-built Slint C++ SDK and provides Slint::Slint target.
#
# The Slint C++ SDK is distributed as a self-extracting NSIS installer (.exe).
# This script downloads it, runs it in silent mode to extract to a local
# directory, and then uses find_package() to import the targets.

set(SLINT_VERSION "1.16.1" CACHE STRING "Slint C++ SDK version to download")

# Construct download URL and local paths
set(_slint_archive "Slint-cpp-${SLINT_VERSION}-win64-MSVC-AMD64.exe")
set(_slint_url "https://github.com/slint-ui/slint/releases/download/v${SLINT_VERSION}/${_slint_archive}")

# Use FETCHCONTENT_BASE_DIR as the persistent cache root (set in root CMakeLists.txt)
set(_slint_cache_dir "${FETCHCONTENT_BASE_DIR}/slint-sdk-${SLINT_VERSION}")
set(_slint_install_dir "${_slint_cache_dir}/install")
set(_slint_installer "${_slint_cache_dir}/${_slint_archive}")
set(_slint_stamp "${_slint_cache_dir}/.installed")

# Only download and install if not already done
if(NOT EXISTS "${_slint_stamp}")
    file(MAKE_DIRECTORY "${_slint_cache_dir}")

    # --- Download ---
    if(NOT EXISTS "${_slint_installer}")
        message(STATUS "[Slint] Downloading Slint C++ SDK v${SLINT_VERSION} ...")
        file(DOWNLOAD
            "${_slint_url}"
            "${_slint_installer}"
            SHOW_PROGRESS
            STATUS _slint_dl_status
        )
        list(GET _slint_dl_status 0 _slint_dl_code)
        list(GET _slint_dl_status 1 _slint_dl_msg)
        if(NOT _slint_dl_code EQUAL 0)
            file(REMOVE "${_slint_installer}")
            message(FATAL_ERROR
                "[Slint] Download failed (${_slint_dl_code}): ${_slint_dl_msg}\n"
                "  URL: ${_slint_url}"
            )
        endif()
        message(STATUS "[Slint] Download complete.")
    endif()

    # --- Silent install (NSIS) ---
    # NSIS installers may require elevated privileges on Windows.
    # We use PowerShell Start-Process with -Verb RunAs to trigger a UAC prompt
    # if needed, rather than failing silently with "permission denied".
    message(STATUS "[Slint] Extracting SDK to ${_slint_install_dir} ...")
    file(TO_NATIVE_PATH "${_slint_install_dir}" _slint_install_dir_native)

    # First try without elevation (works in admin terminals / CI)
    execute_process(
        COMMAND "${_slint_installer}" /S /D=${_slint_install_dir_native}
        RESULT_VARIABLE _slint_install_result
        TIMEOUT 120
    )

    # If the direct attempt failed, retry with UAC elevation via PowerShell
    if(NOT _slint_install_result EQUAL 0)
        message(STATUS "[Slint] Direct install failed (${_slint_install_result}), retrying with elevation ...")
        file(TO_NATIVE_PATH "${_slint_installer}" _slint_installer_native)
        execute_process(
            COMMAND powershell -NoProfile -Command
                "Start-Process -FilePath '${_slint_installer_native}' -ArgumentList '/S','/D=${_slint_install_dir_native}' -Verb RunAs -Wait"
            RESULT_VARIABLE _slint_install_result
            TIMEOUT 180
        )
    endif()

    if(NOT _slint_install_result EQUAL 0)
        message(FATAL_ERROR
            "[Slint] Silent install failed with code ${_slint_install_result}.\n"
            "  Installer: ${_slint_installer}\n"
            "  Target dir: ${_slint_install_dir_native}\n"
            "  Hint: Try running CMake configure from an Administrator terminal,\n"
            "        or manually run the installer:\n"
            "          \"${_slint_installer}\" /S /D=${_slint_install_dir_native}"
        )
    endif()

    # Write stamp file so we skip this on subsequent configures
    file(WRITE "${_slint_stamp}" "Slint ${SLINT_VERSION} installed at ${_slint_install_dir}\n")
    message(STATUS "[Slint] SDK v${SLINT_VERSION} installed successfully.")
endif()

# --- Verify installation and import ---
set(_slint_cmake_dir "${_slint_install_dir}/lib/cmake/Slint")
if(NOT EXISTS "${_slint_cmake_dir}/SlintConfig.cmake")
    message(FATAL_ERROR
        "[Slint] SlintConfig.cmake not found at ${_slint_cmake_dir}.\n"
        "  Try deleting ${_slint_cache_dir} and reconfiguring."
    )
endif()

list(APPEND CMAKE_PREFIX_PATH "${_slint_install_dir}")
find_package(Slint REQUIRED)

message(STATUS "[Slint] Using SDK at: ${_slint_install_dir}")
