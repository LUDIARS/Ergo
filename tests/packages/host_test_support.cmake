# ---------------------------------------------------------------------------
# host_test_support.cmake — helpers for the host consumption tests
# (tests/packages/test_host_consume.cmake, tests/thirdparty/
# test_host_cache_isolation.cmake, tests/audio/test_audio_stage_runtime.cmake).
#
# Each test is `cmake -P <script>` and configures / builds a small host project
# that add_subdirectory()s this Ergo checkout. The nested build uses the outer
# build's generator, platform (-A), C++ compiler and compiler launcher, so it
# matches the toolchain under test and reuses a compiler cache. No network.
#
#   -DERGO_SOURCE_DIR=<ergo> -DERGO_TEST_WORK_DIR=<dir>
#   [-DERGO_TEST_GENERATOR=<gen>] [-DERGO_TEST_PLATFORM=<platform>]
#   [-DERGO_TEST_CXX_COMPILER=<path>] [-DERGO_TEST_CXX_LAUNCHER=<launcher>]
# ---------------------------------------------------------------------------
if(NOT DEFINED ERGO_SOURCE_DIR)
    get_filename_component(ERGO_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()
if(NOT ERGO_TEST_WORK_DIR)
    message(FATAL_ERROR "FAIL: ERGO_TEST_WORK_DIR is required")
endif()

# Multi-config generators (Visual Studio / Xcode / Ninja Multi-Config) build
# this configuration; single-config builds keep the generator's default.
set(HOST_TEST_CONFIG Debug)

function(host_fail what)
    message(FATAL_ERROR "FAIL: ${what}")
endfunction()

function(host_pass name)
    message(STATUS "PASS: ${name}")
endfunction()

# host_configure(<build dir> <source dir> <rc var> <log var> [cache args...])
function(host_configure build source rc_var log_var)
    set(_args -S "${source}" -B "${build}" "-DERGO_SOURCE_DIR=${ERGO_SOURCE_DIR}")
    if(ERGO_TEST_GENERATOR)
        list(APPEND _args -G "${ERGO_TEST_GENERATOR}")
        if(ERGO_TEST_PLATFORM)
            list(APPEND _args -A "${ERGO_TEST_PLATFORM}")
        endif()
    endif()
    # IDE generators pick their own compiler.
    if(ERGO_TEST_CXX_COMPILER AND NOT ERGO_TEST_GENERATOR MATCHES "^(Visual Studio|Xcode)")
        list(APPEND _args "-DCMAKE_CXX_COMPILER=${ERGO_TEST_CXX_COMPILER}")
    endif()
    if(ERGO_TEST_CXX_LAUNCHER)
        list(APPEND _args "-DCMAKE_CXX_COMPILER_LAUNCHER=${ERGO_TEST_CXX_LAUNCHER}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" ${_args} ${ARGN}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    set(${rc_var} "${_rc}" PARENT_SCOPE)
    set(${log_var} "${_out}\n${_err}" PARENT_SCOPE)
endfunction()

# host_build(<build dir> [targets...]) — fails the test on a build error.
function(host_build build)
    set(_targets "")
    if(ARGN)
        set(_targets --target ${ARGN})
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${build}" --config ${HOST_TEST_CONFIG}
            --parallel 2 ${_targets}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        host_fail("build of ${build} ${_targets} failed\n${_out}\n${_err}")
    endif()
endfunction()

# host_cache_value(<build dir> <name> <out var>) — the value of a cache entry
# (whatever its type; -D<name>=<v> without a type is UNINITIALIZED), or
# "<absent>" when the cache has no such entry.
function(host_cache_value build name out)
    file(STRINGS "${build}/CMakeCache.txt" _lines REGEX "^${name}:[A-Z]+=")
    set(_value "<absent>")
    if(_lines)
        list(GET _lines 0 _line)
        string(REGEX REPLACE "^${name}:[A-Z]+=" "" _value "${_line}")
    endif()
    set(${out} "${_value}" PARENT_SCOPE)
endfunction()

function(host_assert_cache build name expected)
    host_cache_value("${build}" ${name} _value)
    if(NOT _value STREQUAL expected)
        host_fail("${build}: cache ${name} is '${_value}', expected '${expected}'")
    endif()
endfunction()

# host_generated_file(<build dir> <prefix> <out var>) — a file written by
# file(GENERATE OUTPUT "<prefix>$<CONFIG>.txt"), for the configuration built.
function(host_generated_file build prefix out)
    if(EXISTS "${build}/${prefix}${HOST_TEST_CONFIG}.txt")
        set(${out} "${build}/${prefix}${HOST_TEST_CONFIG}.txt" PARENT_SCOPE)
    elseif(EXISTS "${build}/${prefix}.txt")
        set(${out} "${build}/${prefix}.txt" PARENT_SCOPE)
    else()
        file(GLOB _found "${build}/${prefix}*.txt")
        host_fail("${build}: no ${prefix}<config>.txt for ${HOST_TEST_CONFIG} (found: ${_found})")
    endif()
endfunction()

# ctest next to the running cmake.
get_filename_component(_host_cmake_dir "${CMAKE_COMMAND}" DIRECTORY)
find_program(HOST_CTEST_COMMAND NAMES ctest HINTS "${_host_cmake_dir}" NO_DEFAULT_PATH)
if(NOT HOST_CTEST_COMMAND)
    find_program(HOST_CTEST_COMMAND NAMES ctest)
endif()
