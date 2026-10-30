cmake_minimum_required(VERSION 3.20)
# Host consumption (spec/setup/build-cpp.md「ホストアプリからの取り込み」): a
# compiled host project add_subdirectory()s this checkout the way a game does
# and links Ergo:: targets (tests/packages/fixtures/host_consume).
#   1. C++14 host with BUILD_SHARED_LIBS=ON / BUILD_TESTING=ON on the command
#      line: configures with the subproject defaults (tests OFF, EXCLUDE_FROM_ALL
#      ON), keeps the host's cache values, every ergo_* library is STATIC and
#      has its Ergo:: alias, the host's own test_math target does not collide,
#      and `all` builds host_app / test_math / host_plugin (SHARED, not on
#      Windows) without building the Ergo libraries the host does not link.
#   2. ctest sees only the host's own test, and it passes.
#   3. Toggling ERGO_BUILD_TESTS in that build directory takes effect both
#      ways: ON puts Ergo back in `all` (so its tests get built), OFF takes it
#      out again, and `all` still skips the Ergo libraries the host does not
#      link (ERGO_EXCLUDE_FROM_ALL is not frozen by the first configure).
#   4. A module whose required module is OFF stops the configure, naming both.
#
#   cmake -DERGO_SOURCE_DIR=<ergo> -DERGO_TEST_WORK_DIR=<dir>
#         [-DERGO_TEST_GENERATOR=<gen>] [-DERGO_TEST_PLATFORM=<platform>]
#         [-DERGO_TEST_CXX_COMPILER=<cxx>] [-DERGO_TEST_CXX_LAUNCHER=<launcher>]
#         -P tests/packages/test_host_consume.cmake
include("${CMAKE_CURRENT_LIST_DIR}/host_test_support.cmake")

set(_fixture "${CMAKE_CURRENT_LIST_DIR}/fixtures/host_consume")
file(REMOVE_RECURSE "${ERGO_TEST_WORK_DIR}")
file(MAKE_DIRECTORY "${ERGO_TEST_WORK_DIR}")

# --- 1. configure + build -------------------------------------------------
set(B "${ERGO_TEST_WORK_DIR}/host")
host_configure("${B}" "${_fixture}" _rc _log
    -DCMAKE_CXX_STANDARD=14 -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=ON)
if(NOT _rc EQUAL 0)
    host_fail("host configure failed\n${_log}")
endif()
if(NOT _log MATCHES "module libraries are always STATIC; BUILD_SHARED_LIBS ignored")
    host_fail("configure did not say that BUILD_SHARED_LIBS is ignored\n${_log}")
endif()

# The host's cache is the host's; Ergo picked its subproject defaults.
host_assert_cache("${B}" BUILD_SHARED_LIBS ON)
host_assert_cache("${B}" BUILD_TESTING ON)
host_assert_cache("${B}" CMAKE_CXX_STANDARD 14)
host_assert_cache("${B}" ERGO_BUILD_TESTS OFF)
host_assert_cache("${B}" ERGO_EXCLUDE_FROM_ALL ON)
host_assert_cache("${B}" ERGO_POSITION_INDEPENDENT_CODE ON)

file(STRINGS "${B}/host_facts.txt" _facts)
foreach(_expect "BUILD_SHARED_LIBS=ON" "BUILD_TESTING=ON" "CMAKE_CXX_STANDARD=14"
        "ergo_exclude_from_all=ON")
    if(NOT _expect IN_LIST _facts)
        host_fail("host scope after add_subdirectory lacks '${_expect}': ${_facts}")
    endif()
endforeach()
set(_libraries 0)
foreach(_fact IN LISTS _facts)
    if(NOT _fact MATCHES "^library ([^ ]+) ([^ ]+) ([^ ]+)$")
        continue()
    endif()
    set(_t "${CMAKE_MATCH_1}")
    set(_type "${CMAKE_MATCH_2}")
    set(_alias "${CMAKE_MATCH_3}")
    math(EXPR _libraries "${_libraries} + 1")
    # INTERFACE libraries (ergo_thirdparty) have no binary; the rest are STATIC.
    if(NOT _type STREQUAL "STATIC_LIBRARY" AND NOT _type STREQUAL "INTERFACE_LIBRARY")
        host_fail("${_t} is ${_type}; Ergo's libraries must be STATIC")
    endif()
    if(NOT _alias STREQUAL "alias")
        host_fail("${_t} has no Ergo:: alias")
    endif()
endforeach()
if(_libraries LESS 20)
    host_fail("only ${_libraries} ergo_* libraries seen: ${_facts}")
endif()
host_pass("configure: host cache kept, subproject defaults, ${_libraries} STATIC libraries with Ergo:: aliases")

host_build("${B}")
host_generated_file("${B}" host_files_ _files_txt)
file(STRINGS "${_files_txt}" _files)
set(_checked 0)
foreach(_line IN LISTS _files)
    if(_line MATCHES "^(linked|unused)=(.+)$")
        set(_kind "${CMAKE_MATCH_1}")
        set(_file "${CMAKE_MATCH_2}")
        math(EXPR _checked "${_checked} + 1")
        if(_kind STREQUAL "linked" AND NOT EXISTS "${_file}")
            host_fail("ergo_log was not built for host_app: ${_file}")
        elseif(_kind STREQUAL "unused" AND EXISTS "${_file}")
            host_fail("`all` built an Ergo library the host does not link: ${_file}")
        endif()
    endif()
endforeach()
if(NOT _checked EQUAL 2)
    host_fail("${_files_txt} should name the linked and the unused library: ${_files}")
endif()
host_pass("build: C++14 host + SHARED plugin link Ergo, unused Ergo targets stay out of all")

# --- 2. the host's tests only ---------------------------------------------
if(NOT HOST_CTEST_COMMAND)
    host_fail("ctest not found next to ${CMAKE_COMMAND}")
endif()
execute_process(COMMAND "${HOST_CTEST_COMMAND}" --test-dir "${B}" -C ${HOST_TEST_CONFIG} -N
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0 OR NOT _out MATCHES "Total Tests: 1\n" OR NOT _out MATCHES "host_smoke")
    host_fail("ctest -N should list only host_smoke\n${_out}\n${_err}")
endif()
execute_process(COMMAND "${HOST_CTEST_COMMAND}" --test-dir "${B}" -C ${HOST_TEST_CONFIG}
        --output-on-failure
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    host_fail("host_smoke failed\n${_out}\n${_err}")
endif()
host_pass("ctest: Ergo adds no tests to the host, host_smoke runs")

# --- 3. toggling ERGO_BUILD_TESTS in an existing build directory ----------
host_configure("${B}" "${_fixture}" _rc _log -DERGO_BUILD_TESTS=ON)
if(NOT _rc EQUAL 0)
    host_fail("reconfigure with ERGO_BUILD_TESTS=ON failed\n${_log}")
endif()
file(STRINGS "${B}/host_facts.txt" _facts)
if("ergo_exclude_from_all=ON" IN_LIST _facts)
    host_fail("ERGO_BUILD_TESTS=ON left Ergo out of `all`, so its tests would not be built: ${_facts}")
endif()
host_configure("${B}" "${_fixture}" _rc _log -DERGO_BUILD_TESTS=OFF)
if(NOT _rc EQUAL 0)
    host_fail("reconfigure with ERGO_BUILD_TESTS=OFF failed\n${_log}")
endif()
file(STRINGS "${B}/host_facts.txt" _facts)
if(NOT "ergo_exclude_from_all=ON" IN_LIST _facts)
    host_fail("ERGO_BUILD_TESTS=OFF did not take Ergo out of `all` again: ${_facts}")
endif()
host_build("${B}")
file(STRINGS "${_files_txt}" _files)
foreach(_line IN LISTS _files)
    if(_line MATCHES "^unused=(.+)$" AND EXISTS "${CMAKE_MATCH_1}")
        host_fail("`all` built an Ergo library the host does not link after the toggle: ${CMAKE_MATCH_1}")
    endif()
endforeach()
host_pass("ERGO_BUILD_TESTS toggles ON and OFF in an existing build directory")

# --- 4. configure-time module requirements --------------------------------
set(B3 "${ERGO_TEST_WORK_DIR}/missing_requirements")
host_configure("${B3}" "${_fixture}" _rc _log -DERGO_BUILD_IO=OFF -DERGO_BUILD_MATH=OFF)
if(_rc EQUAL 0)
    host_fail("configure with ERGO_BUILD_IO=OFF / ERGO_BUILD_MATH=OFF should fail\n${_log}")
endif()
string(REGEX REPLACE "[ \t\r\n]+" " " _log "${_log}")  # CMake wraps message text
foreach(_expect
        "ERGO_BUILD_UI_KIT=ON but ERGO_BUILD_IO=OFF"
        "ERGO_BUILD_UI_LAYOUT=ON but ERGO_BUILD_IO=OFF"
        "ERGO_BUILD_RESOURCE=ON but ERGO_BUILD_IO=OFF"
        "ERGO_BUILD_PHYSICS2D=ON but ERGO_BUILD_MATH=OFF"
        "No silent fallback")
    string(FIND "${_log}" "${_expect}" _at)
    if(_at EQUAL -1)
        host_fail("configure error lacks '${_expect}'\n${_log}")
    endif()
endforeach()
host_pass("configure error names every missing module requirement")
