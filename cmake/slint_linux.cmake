# cmake/slint_linux.cmake
# Linux adaptation: uses pre-extracted Slint C++ SDK tarball
# (The original slint.cmake downloads a Windows NSIS installer)

set(SLINT_VERSION "1.16.1" CACHE STRING "Slint C++ SDK version")

set(_slint_cache_dir "${FETCHCONTENT_BASE_DIR}/slint-sdk-${SLINT_VERSION}")
set(_slint_install_dir "${_slint_cache_dir}/install")
set(_slint_stamp "${_slint_cache_dir}/.installed")
set(_slint_archive_name "Slint-cpp-${SLINT_VERSION}-Linux-x86_64.tar.gz")
set(_slint_url "https://github.com/slint-ui/slint/releases/download/v${SLINT_VERSION}/${_slint_archive_name}")

if(NOT EXISTS "${_slint_stamp}")
    file(MAKE_DIRECTORY "${_slint_cache_dir}")

    set(_slint_tarball "${_slint_cache_dir}/${_slint_archive_name}")
    if(NOT EXISTS "${_slint_tarball}")
        message(STATUS "[Slint] Downloading Slint C++ SDK v${SLINT_VERSION} for Linux ...")
        file(DOWNLOAD "${_slint_url}" "${_slint_tarball}" SHOW_PROGRESS STATUS _slint_dl_status)
        list(GET _slint_dl_status 0 _slint_dl_code)
        if(NOT _slint_dl_code EQUAL 0)
            file(REMOVE "${_slint_tarball}")
            message(FATAL_ERROR "[Slint] Download failed: ${_slint_dl_status}")
        endif()
    endif()

    message(STATUS "[Slint] Extracting SDK to ${_slint_install_dir} ...")
    file(ARCHIVE_EXTRACT INPUT "${_slint_tarball}" DESTINATION "${_slint_cache_dir}")
    file(RENAME "${_slint_cache_dir}/Slint-cpp-${SLINT_VERSION}-Linux-x86_64" "${_slint_install_dir}")
    file(WRITE "${_slint_stamp}" "Slint ${SLINT_VERSION} installed at ${_slint_install_dir}\n")
    message(STATUS "[Slint] SDK v${SLINT_VERSION} installed successfully.")
endif()

set(_slint_cmake_dir "${_slint_install_dir}/lib/cmake/Slint")
if(NOT EXISTS "${_slint_cmake_dir}/SlintConfig.cmake")
    message(FATAL_ERROR "[Slint] SlintConfig.cmake not found at ${_slint_cmake_dir}")
endif()

list(APPEND CMAKE_PREFIX_PATH "${_slint_install_dir}")
find_package(Slint REQUIRED)
message(STATUS "[Slint] Using SDK at: ${_slint_install_dir}")
