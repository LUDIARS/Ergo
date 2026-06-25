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

# Trim curl to a library-only static build. These must be set before the
# upstream CMakeLists runs (FetchContent_MakeAvailable below).
set(BUILD_CURL_EXE      OFF CACHE BOOL "" FORCE)  # no curl.exe
set(BUILD_SHARED_LIBS   OFF CACHE BOOL "" FORCE)  # static libcurl
set(BUILD_STATIC_LIBS   ON  CACHE BOOL "" FORCE)
set(BUILD_TESTING       OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL     OFF CACHE BOOL "" FORCE)  # avoid optional libpsl probe
set(CURL_ENABLE_EXPORT_TARGET OFF CACHE BOOL "" FORCE)

if(ERGO_CURL_USE_SSL AND WIN32)
    set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)   # system TLS, no OpenSSL needed
elseif(NOT ERGO_CURL_USE_SSL)
    set(CURL_ENABLE_SSL OFF CACHE BOOL "" FORCE)
endif()

ergo_require_dependency(curl VERIFY_TARGETS CURL::libcurl)
