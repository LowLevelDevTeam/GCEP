cmake_minimum_required(VERSION 3.30)

if(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(_gcep_dl_url "https://dylan-hollemaert.fr/downloads/clang++/windows/ARM64/clang++.exe")
    else()
        set(_gcep_dl_url "https://dylan-hollemaert.fr/downloads/clang++/windows/x64/clang++.exe")
    endif()
    set(_gcep_bin_name "clang++.exe")
elseif(APPLE)
    set(_gcep_dl_url  "https://dylan-hollemaert.fr/downloads/clang++/macos/clang++")
    set(_gcep_bin_name "clang++")
else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        set(_gcep_dl_url "https://dylan-hollemaert.fr/downloads/clang++/linux/ARM64/clang++")
    else()
        set(_gcep_dl_url "https://dylan-hollemaert.fr/downloads/clang++/linux/x64/clang++")
    endif()
    set(_gcep_bin_name "clang++")
endif()

set(_gcep_bin_path "${CMAKE_BINARY_DIR}/bin/${_gcep_bin_name}")

if(NOT EXISTS "${_gcep_bin_path}")
    message(STATUS "[GCEP] clang++ not found, downloading to ${_gcep_bin_path}")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    file(DOWNLOAD
        "${_gcep_dl_url}"
        "${_gcep_bin_path}"
        SHOW_PROGRESS
        STATUS _dl_status
        TLS_VERIFY ON
    )
    list(GET _dl_status 0 _dl_code)
    list(GET _dl_status 1 _dl_msg)
    if(NOT _dl_code EQUAL 0)
        message(FATAL_ERROR "[GCEP] Download failed (${_dl_code}): ${_dl_msg}")
    endif()
    if(NOT WIN32)
        file(CHMOD "${_gcep_bin_path}"
            PERMISSIONS
                OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
        )
    endif()
    message(STATUS "[GCEP] clang++ ready at ${_gcep_bin_path}")
else()
    message(STATUS "[GCEP] clang++ found at ${_gcep_bin_path}")
endif()