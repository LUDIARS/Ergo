# Host for tests/thirdparty/test_host_cache_isolation.cmake: a host with its
# own tests (include(CTest)) that turns on Ergo's managed curl and ergo_http.
# The script passes BUILD_SHARED_LIBS=ON / BUILD_TESTING=ON and points
# ERGO_SOURCE_DIR_CURL at fixtures/fake_curl. host_cache_facts.txt records
# what the host and the fake curl saw.
cmake_minimum_required(VERSION 3.20)
project(HostCacheIsolation LANGUAGES CXX)
include(CTest)

set(ERGO_WITH_CURL ON)
set(ERGO_BUILD_HTTP ON)
add_subdirectory("${ERGO_SOURCE_DIR}" ergo)

add_test(NAME host_smoke COMMAND "${CMAKE_COMMAND}" -E echo host_smoke)

get_property(_curl_seen GLOBAL PROPERTY FAKE_CURL_SEEN)
get_target_property(_http_type ergo_http TYPE)
set(_facts
    "host BUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}"
    "host BUILD_TESTING=${BUILD_TESTING}"
    "ergo_http ${_http_type}")
foreach(_seen IN LISTS _curl_seen)
    list(APPEND _facts "curl ${_seen}")
endforeach()
string(REPLACE ";" "\n" _facts "${_facts}")
file(WRITE "${CMAKE_BINARY_DIR}/host_cache_facts.txt" "${_facts}\n")
