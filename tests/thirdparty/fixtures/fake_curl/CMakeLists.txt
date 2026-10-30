# Stand-in for curl 8.19's top-level CMakeLists.txt, used by
# tests/thirdparty/test_host_cache_isolation.cmake through
# -DERGO_SOURCE_DIR_CURL (nothing is fetched). Same policy range as upstream
# (so CMP0077 is NEW) and the same option() names cmake/deps/curl.cmake sets.
# Reports what it saw to the host through a global property.
cmake_minimum_required(VERSION 3.7...3.16)
project(CURL LANGUAGES NONE)

option(BUILD_CURL_EXE "Build curl executable" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_STATIC_LIBS "Build static libraries" OFF)
option(BUILD_TESTING "Build tests" ON)
option(CURL_DISABLE_INSTALL "Disable installation targets" OFF)
option(CURL_USE_LIBPSL "Use libpsl" ON)
option(CURL_ENABLE_EXPORT_TARGET "Enable CMake export target" ON)
option(CURL_ENABLE_SSL "Enable SSL support" ON)
option(CURL_USE_SCHANNEL "Enable Windows native SSL/TLS (Schannel)" OFF)

set_property(GLOBAL PROPERTY FAKE_CURL_SEEN
    "BUILD_CURL_EXE=${BUILD_CURL_EXE}"
    "BUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}"
    "BUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}"
    "BUILD_TESTING=${BUILD_TESTING}"
    "CURL_DISABLE_INSTALL=${CURL_DISABLE_INSTALL}"
    "CURL_USE_LIBPSL=${CURL_USE_LIBPSL}"
    "CURL_ENABLE_EXPORT_TARGET=${CURL_ENABLE_EXPORT_TARGET}")

add_library(libcurl INTERFACE)
add_library(CURL::libcurl ALIAS libcurl)
