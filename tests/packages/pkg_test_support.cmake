# ---------------------------------------------------------------------------
# pkg_test_support.cmake — assertions shared by the tests/packages/ scripts.
#
# Each test script runs as `cmake -DERGO_SOURCE_DIR=<ergo> -P <script>` and
# fails by FATAL_ERROR. Findings follow the ErgoPackageRules.cmake format
# "<id>|<severity>|<subject>|<message>".
# ---------------------------------------------------------------------------
if(NOT DEFINED ERGO_SOURCE_DIR)
    get_filename_component(ERGO_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()
if(NOT DEFINED ERGO_TEST_WORK_DIR)
    set(ERGO_TEST_WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/ergo-packages-test")
endif()

include("${ERGO_SOURCE_DIR}/cmake/packages/ErgoPackageManifest.cmake")
include("${ERGO_SOURCE_DIR}/cmake/packages/ErgoPackageRules.cmake")
include("${ERGO_SOURCE_DIR}/cmake/packages/ErgoPackageReport.cmake")

set(_PKG_TEST_FAILURES 0)

function(pkg_fail what)
    message(FATAL_ERROR "FAIL: ${what}")
endfunction()

function(pkg_assert_eq label actual expected)
    if(NOT "${actual}" STREQUAL "${expected}")
        pkg_fail("${label}: expected '${expected}' got '${actual}'")
    endif()
endfunction()

# Assert some finding matches <id> and <severity> (and optional <subject>).
function(pkg_assert_finding label findings id severity)
    set(_subject "${ARGV4}")
    foreach(_f IN LISTS findings)
        string(REPLACE "|" ";" _parts "${_f}")
        list(GET _parts 0 _id)
        list(GET _parts 1 _sev)
        list(GET _parts 2 _subj)
        if(_id STREQUAL id AND _sev STREQUAL severity
           AND (_subject STREQUAL "" OR _subj STREQUAL _subject))
            return()
        endif()
    endforeach()
    pkg_fail("${label}: no ${id}/${severity} ${_subject} finding in [${findings}]")
endfunction()

# Assert no finding carries <id>.
function(pkg_assert_no_finding label findings id)
    foreach(_f IN LISTS findings)
        if(_f MATCHES "^${id}\\|")
            pkg_fail("${label}: unexpected finding '${_f}'")
        endif()
    endforeach()
endfunction()

function(pkg_assert_empty label findings)
    if(NOT "${findings}" STREQUAL "")
        pkg_fail("${label}: expected no findings, got [${findings}]")
    endif()
endfunction()

function(pkg_pass name)
    message(STATUS "PASS: ${name}")
endfunction()
