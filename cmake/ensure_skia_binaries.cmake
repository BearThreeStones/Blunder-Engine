# Prefetch rust-skia prebuilt binaries for Slint (Windows / skia-bindings 0.90.0).
# The feature key Slint requests (d3d-gl-pdf-textlayout-vulkan) is not published on
# GitHub; the superset archive d3d-gl-pdf-svg-textlayout-vulkan is used instead.

function(blunder_ensure_skia_binaries out_url)
    set(${out_url} "" PARENT_SCOPE)

    if(NOT WIN32)
        return()
    endif()

    if(BLUNDER_SLINT_SKIA_BINARIES_URL)
        if(BLUNDER_SLINT_SKIA_BINARIES_URL MATCHES "^file:///")
            string(REGEX REPLACE "^file:///" "file://"
                _blunder_skia_fixed_url "${BLUNDER_SLINT_SKIA_BINARIES_URL}")
            set(BLUNDER_SLINT_SKIA_BINARIES_URL "${_blunder_skia_fixed_url}" CACHE STRING "" FORCE)
        endif()
        set(${out_url} "${BLUNDER_SLINT_SKIA_BINARIES_URL}" PARENT_SCOPE)
        return()
    endif()

    set(_blunder_skia_tag "0.90.0")
    set(_blunder_skia_key
        "da4579b39b75fa2187c5-x86_64-pc-windows-msvc-d3d-gl-pdf-svg-textlayout-vulkan"
    )
    set(_blunder_skia_name "skia-binaries-${_blunder_skia_key}.tar.gz")
    set(_blunder_skia_dir "${FETCHCONTENT_BASE_DIR}/skia-binaries-cache")
    set(_blunder_skia_archive "${_blunder_skia_dir}/${_blunder_skia_name}")
    set(_blunder_skia_https_url
        "https://github.com/rust-skia/skia-binaries/releases/download/${_blunder_skia_tag}/${_blunder_skia_name}"
    )

    if(NOT EXISTS "${_blunder_skia_archive}")
        file(MAKE_DIRECTORY "${_blunder_skia_dir}")
        message(STATUS "[Slint] Fetching Skia prebuilt archive (this may take a minute)...")
        message(STATUS "[Slint]   ${_blunder_skia_https_url}")

        file(DOWNLOAD "${_blunder_skia_https_url}" "${_blunder_skia_archive}"
            SHOW_PROGRESS
            TLS_VERIFY ON
            STATUS _blunder_skia_download_status
        )
        list(GET _blunder_skia_download_status 0 _blunder_skia_download_code)
        list(GET _blunder_skia_download_status 1 _blunder_skia_download_msg)

        if(NOT _blunder_skia_download_code EQUAL 0)
            find_program(_blunder_curl_executable NAMES curl curl.exe)
            if(_blunder_curl_executable)
                execute_process(
                    COMMAND "${_blunder_curl_executable}" --ssl-no-revoke -fsSL
                        -o "${_blunder_skia_archive}"
                        "${_blunder_skia_https_url}"
                    RESULT_VARIABLE _blunder_curl_result
                    ERROR_VARIABLE _blunder_curl_stderr
                )
            else()
                set(_blunder_curl_result 1)
            endif()

            if(NOT _blunder_curl_result EQUAL 0 OR NOT EXISTS "${_blunder_skia_archive}")
                message(FATAL_ERROR
                    "[Slint] Failed to download Skia prebuilt binaries.\n"
                    "  CMake: ${_blunder_skia_download_code} ${_blunder_skia_download_msg}\n"
                    "  URL: ${_blunder_skia_https_url}\n"
                    "Download the file in a browser, place it at:\n"
                    "  ${_blunder_skia_archive}\n"
                    "Then re-run CMake, or set BLUNDER_SLINT_SKIA_BINARIES_URL to\n"
                    "  file:///D:/path/to/${_blunder_skia_name}"
                )
            endif()
        endif()
    endif()

    file(TO_CMAKE_PATH "${_blunder_skia_archive}" _blunder_skia_archive_cmake)
    string(REPLACE "\\" "/" _blunder_skia_archive_uri "${_blunder_skia_archive_cmake}")
    # rust-skia strips the "file://" prefix and reads the remainder; use file://D:/...
    # (not file:///D:/...) so Windows paths stay valid (see skia-bindings utils.rs).
    set(_blunder_skia_file_url "file://${_blunder_skia_archive_uri}")

    message(STATUS "[Slint] Skia prebuilt archive: ${_blunder_skia_archive}")
    message(STATUS "[Slint] SKIA_BINARIES_URL=${_blunder_skia_file_url}")
    set(${out_url} "${_blunder_skia_file_url}" PARENT_SCOPE)
endfunction()
