cmake_minimum_required(VERSION 3.20)
# Managed curl must not touch the host's cache (spec/setup/third-party-deps.md
# 注意点). A host with include(CTest) and BUILD_SHARED_LIBS=ON turns on
# ERGO_WITH_CURL / ERGO_BUILD_HTTP; curl is the fake source tree in
# fixtures/fake_curl (no network). Configured twice (a reconfigure reads the
# cache the first run wrote):
#   - the host's BUILD_SHARED_LIBS / BUILD_TESTING stay ON in its cache and in
#     its scope after add_subdirectory(Ergo), and its own test is registered
#   - curl still sees its library-only knobs (normal variables, CMP0077) and
#     none of them lands in the cache
#   - ergo_http stays STATIC
#
#   cmake -DERGO_SOURCE_DIR=<ergo> -DERGO_TEST_WORK_DIR=<dir>
#         [-DERGO_TEST_GENERATOR=<gen>] [-DERGO_TEST_PLATFORM=<platform>]
#         [-DERGO_TEST_CXX_COMPILER=<cxx>] [-DERGO_TEST_CXX_LAUNCHER=<launcher>]
#         -P tests/thirdparty/test_host_cache_isolation.cmake
include("${CMAKE_CURRENT_LIST_DIR}/../packages/host_test_support.cmake")

set(_fixtures "${CMAKE_CURRENT_LIST_DIR}/fixtures")
set(B "${ERGO_TEST_WORK_DIR}/host")
file(REMOVE_RECURSE "${ERGO_TEST_WORK_DIR}")
file(MAKE_DIRECTORY "${ERGO_TEST_WORK_DIR}")

foreach(_run first reconfigure)
    host_configure("${B}" "${_fixtures}/host" _rc _log
        -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=ON
        "-DERGO_SOURCE_DIR_CURL=${_fixtures}/fake_curl")
    if(NOT _rc EQUAL 0)
        host_fail("${_run}: host configure failed\n${_log}")
    endif()

    host_assert_cache("${B}" BUILD_SHARED_LIBS ON)
    host_assert_cache("${B}" BUILD_TESTING ON)
    foreach(_knob BUILD_CURL_EXE BUILD_STATIC_LIBS CURL_DISABLE_INSTALL CURL_USE_LIBPSL
                  CURL_ENABLE_EXPORT_TARGET)
        host_cache_value("${B}" ${_knob} _value)
        if(NOT _value STREQUAL "<absent>")
            host_fail("${_run}: curl knob ${_knob} was written to the host cache (${_value})")
        endif()
    endforeach()

    file(STRINGS "${B}/host_cache_facts.txt" _facts)
    foreach(_expect
            "host BUILD_SHARED_LIBS=ON" "host BUILD_TESTING=ON" "ergo_http STATIC_LIBRARY"
            "curl BUILD_CURL_EXE=OFF" "curl BUILD_SHARED_LIBS=OFF" "curl BUILD_STATIC_LIBS=ON"
            "curl BUILD_TESTING=OFF" "curl CURL_DISABLE_INSTALL=ON" "curl CURL_USE_LIBPSL=OFF"
            "curl CURL_ENABLE_EXPORT_TARGET=OFF")
        if(NOT _expect IN_LIST _facts)
            host_fail("${_run}: expected '${_expect}' in host_cache_facts.txt: ${_facts}")
        endif()
    endforeach()

    file(READ "${B}/CTestTestfile.cmake" _ctest)
    if(NOT _ctest MATCHES "host_smoke")
        host_fail("${_run}: the host's own test host_smoke is not registered\n${_ctest}")
    endif()
    host_pass("${_run}: host cache and scope kept, curl trimmed without cache writes")
endforeach()
