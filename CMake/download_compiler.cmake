cmake_minimum_required(VERSION 3.30)

# Helper: download a file, falling back to curl on HTTP/2 errors
function(_gcep_download url dest label)
    file(DOWNLOAD "${url}" "${dest}"
        SHOW_PROGRESS STATUS _dl_status TLS_VERIFY ON)
    list(GET _dl_status 0 _dl_code)
    list(GET _dl_status 1 _dl_msg)

    if(NOT _dl_code EQUAL 0)
        message(STATUS "[GCEP] file(DOWNLOAD) failed (${_dl_code}: ${_dl_msg}), retrying with curl...")
        find_program(_curl curl)
        if(NOT _curl)
            message(FATAL_ERROR "[GCEP] Download of ${label} failed and curl is not available.")
        endif()
        execute_process(
            COMMAND ${_curl} -L --http1.1 --show-error --fail -o "${dest}" "${url}"
            RESULT_VARIABLE _curl_result
        )
        if(NOT _curl_result EQUAL 0)
            message(FATAL_ERROR "[GCEP] curl download of ${label} also failed.")
        endif()
    endif()
endfunction()

# Helper: download + extract a tar.gz
function(_gcep_download_and_extract label url dest_dir)
    if(EXISTS "${dest_dir}")
        message(STATUS "[GCEP] ${label} already present at ${dest_dir}")
        return()
    endif()

    set(_archive "${CMAKE_BINARY_DIR}/_gcep_tmp/${label}.tar.gz")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_gcep_tmp")

    message(STATUS "[GCEP] Downloading ${label} from ${url}")
    _gcep_download("${url}" "${_archive}" "${label}")

    message(STATUS "[GCEP] Extracting ${label} to ${dest_dir}")
    file(MAKE_DIRECTORY "${dest_dir}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf "${_archive}"
        WORKING_DIRECTORY "${dest_dir}"
        RESULT_VARIABLE _extract_result
    )
    if(NOT _extract_result EQUAL 0)
        message(FATAL_ERROR "[GCEP] Extraction of ${label} failed")
    endif()

    file(REMOVE "${_archive}")
    message(STATUS "[GCEP] ${label} ready at ${dest_dir}")
endfunction()

# Platform detection
if(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(_gcep_dl_url            "https://dylan-hollemaert.fr/downloads/clang++/windows/ARM64/clang++.exe")
        set(_gcep_clang_headers_url "https://dylan-hollemaert.fr/downloads/clang++/windows/ARM64/clang-headers.tar.gz")
    else()
        set(_gcep_dl_url            "https://dylan-hollemaert.fr/downloads/clang++/windows/x64/clang++.exe")
        set(_gcep_clang_headers_url "https://dylan-hollemaert.fr/downloads/clang++/windows/x64/clang-headers.tar.gz")
    endif()
    set(_gcep_bin_name "clang++.exe")
elseif(APPLE)
    set(_gcep_dl_url            "https://dylan-hollemaert.fr/downloads/clang++/macos/clang++")
    set(_gcep_clang_headers_url "https://dylan-hollemaert.fr/downloads/clang++/macos/clang-headers.tar.gz")
    set(_gcep_bin_name "clang++")
else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        set(_gcep_dl_url            "https://dylan-hollemaert.fr/downloads/clang++/linux/ARM64/clang++")
        set(_gcep_clang_headers_url "https://dylan-hollemaert.fr/downloads/clang++/linux/ARM64/clang-headers.tar.gz")
    else()
        set(_gcep_dl_url            "https://dylan-hollemaert.fr/downloads/clang++/linux/x64/clang++")
        set(_gcep_clang_headers_url "https://dylan-hollemaert.fr/downloads/clang++/linux/x64/clang-headers.tar.gz")
    endif()
    set(_gcep_bin_name "clang++")
endif()

# Paths
set(_gcep_bin_path          "${CMAKE_BINARY_DIR}/bin/${_gcep_bin_name}")
set(_gcep_clang_headers_dir "${CMAKE_BINARY_DIR}/bin/lib/clang/22/include")

# Download clang++ binary
if(NOT EXISTS "${_gcep_bin_path}")
    message(STATUS "[GCEP] clang++ not found, downloading to ${_gcep_bin_path}")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    _gcep_download("${_gcep_dl_url}" "${_gcep_bin_path}" "clang++")
    if(NOT WIN32)
        file(CHMOD "${_gcep_bin_path}"
            PERMISSIONS
                OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE)
    endif()
    message(STATUS "[GCEP] clang++ ready at ${_gcep_bin_path}")
else()
    message(STATUS "[GCEP] clang++ found at ${_gcep_bin_path}")
endif()

_gcep_download_and_extract("clang-resource-headers" "${_gcep_clang_headers_url}" "${_gcep_clang_headers_dir}")

# Expose paths for the rest of the build
set(GCEP_COMPILER     "${_gcep_bin_path}"                      CACHE FILEPATH "Bundled clang++ for script compilation" FORCE)
set(GCEP_RESOURCE_DIR "${CMAKE_BINARY_DIR}/bin/lib/clang/22"   CACHE PATH     "Clang resource dir"                     FORCE)