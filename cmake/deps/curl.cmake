# ---------------------------------------------------------------------------
# cmake/deps/curl.cmake — libcurl integration
#
# curl (https://github.com/curl/curl) ships a modern, well-maintained CMake
# build, so we let it run via add_subdirectory and just slim it down to a static
# libcurl with no CLI, no docs, no tests. TLS backend is chosen per platform
# from what the OS provides so the build needs no external crypto SDK:
#   - Windows : Schannel (system)
#   - others  : no built-in TLS by default (callers can flip ERGO_CURL_USE_SSL)
#
# Consume with:  target_link_libraries(<tgt> PRIVATE CURL::libcurl)
#                #include <curl/curl.h>
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

option(ERGO_CURL_USE_SSL "Enable a TLS backend for the managed libcurl build" ON)

# Trim curl to a library-only static build. The knobs are normal variables in
# this function's scope: FetchContent's add_subdirectory inherits them and
# curl's option() calls honour them (CMP0077 NEW), while nothing is written to
# the cache. The host's own BUILD_SHARED_LIBS / BUILD_TESTING (and Ergo's
# scope) stay as they were.
function(_ergo_wire_curl)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_CURL_EXE      OFF)  # no curl.exe
    set(BUILD_SHARED_LIBS   OFF)  # static libcurl
    set(BUILD_STATIC_LIBS   ON)
    set(BUILD_TESTING       OFF)
    set(CURL_DISABLE_INSTALL ON)
    set(CURL_USE_LIBPSL     OFF)  # avoid optional libpsl probe
    set(CURL_ENABLE_EXPORT_TARGET OFF)

    if(ERGO_CURL_USE_SSL AND WIN32)
        set(CURL_USE_SCHANNEL ON)   # system TLS, no OpenSSL needed
    elseif(NOT ERGO_CURL_USE_SSL)
        set(CURL_ENABLE_SSL OFF)
    endif()

    ergo_require_dependency(curl VERIFY_TARGETS CURL::libcurl)
endfunction()
_ergo_wire_curl()
