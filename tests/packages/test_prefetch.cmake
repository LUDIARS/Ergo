# W1: `ergo_pkg fetch` runs cmake/packages/ErgoPackagePrefetch.cmake, which
# fetches link packages at their pin into FetchContent's place
# (<FETCHCONTENT_BASE_DIR>/ergo_pkg_<name>-src). Afterwards a configure with
# ERGO_PACKAGES_FETCH=ON hits route 5 without network
# (spec/feature/package-manager.md §2.2 / §7).
#
# The fake package (fixtures/alpha) becomes a git repository here, reached by
# a file:// URL; "no network" is proven by moving that repository away before
# the configure. project NONE, no compiler. The pin is an untagged commit
# behind the tip (C1 v1.0.0 <- C2 pin <- C3 tip), as real pins are: a shallow
# clone reaches only branch and tag tips, so a GIT_SHALLOW fetch fails there.
#
#   cmake -DERGO_SOURCE_DIR=<ergo> -DERGO_TEST_WORK_DIR=<dir>
#         [-DGIT_EXECUTABLE=<git>] [-DERGO_TEST_GENERATOR=<gen>]
#         -P tests/packages/test_prefetch.cmake
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/pkg_git_support.cmake")

file(TO_CMAKE_PATH "${ERGO_TEST_WORK_DIR}" W)
file(REMOVE_RECURSE "${W}")
file(MAKE_DIRECTORY "${W}")
set(FIX "${CMAKE_CURRENT_LIST_DIR}/fixtures")
set(PREFETCH "${ERGO_SOURCE_DIR}/cmake/packages/ErgoPackagePrefetch.cmake")
set(_gen_args "")
if(ERGO_TEST_GENERATOR)
    set(_gen_args -G "${ERGO_TEST_GENERATOR}")
endif()

# --- fake package and manifest ----------------------------------------------
pkg_make_repo(C1 "${FIX}/alpha" "${W}/repos/Alpha")
pkg_git("${W}/repos/Alpha" tag v1.0.0)
foreach(_c C2 C3)
    file(WRITE "${W}/repos/Alpha/${_c}.txt" "${_c}\n")
    pkg_git("${W}/repos/Alpha" add -A)
    pkg_git("${W}/repos/Alpha" commit -q -m "${_c}")
    pkg_git("${W}/repos/Alpha" rev-parse HEAD)
    set(${_c} "${GIT_OUT}")
endforeach()
pkg_file_url(ALPHA_URL "${W}/repos/Alpha")
set(ALPHA_REV "${C2}")                                # untagged, not the tip
set(ALPHA_EXPECT_TAG v1.0.0)
set(BETA_URL "file:///nowhere/Beta.git")              # never fetched
set(BETA_REV 0000000000000000000000000000000000000000)
set(WIRING_DIR "${FIX}/hub/wiring")
set(TOOL_ENTRY "")
configure_file("${FIX}/hub/ergo-packages.json.in" "${W}/m/ergo-packages.json" @ONLY)
set(BASE "${W}/f")

function(prefetch expect)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DERGO_PKG_PREFETCH_NAMES=${ARGV1}"
            "-DFETCHCONTENT_BASE_DIR=${BASE}"
            "-DERGO_PKG_PREFETCH_MANIFEST=${W}/m/ergo-packages.json"
            "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}" -P "${PREFETCH}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    set(PREFETCH_LOG "${_out}\n${_err}" PARENT_SCOPE)
    if(expect STREQUAL "pass" AND NOT _rc EQUAL 0)
        pkg_fail("prefetch ${ARGV1} should pass\n${_out}\n${_err}")
    elseif(expect STREQUAL "fail" AND _rc EQUAL 0)
        pkg_fail("prefetch ${ARGV1} should fail\n${_out}\n${_err}")
    endif()
endfunction()

# --- prefetch -----------------------------------------------------------------
prefetch(fail beta)       # beta's pin repository does not exist
prefetch(fail nosuch)     # not a link package of the manifest
if(EXISTS "${BASE}/ergo_pkg_nosuch-src")
    pkg_fail("an unknown name must not create a source directory")
endif()
prefetch(pass alpha)
set(SRC "${BASE}/ergo_pkg_alpha-src")
pkg_git("${SRC}" rev-parse HEAD)
pkg_assert_eq("prefetched HEAD is the untagged pin behind the tip" "${GIT_OUT}" "${C2}")
if(EXISTS "${BASE}/_ergo_prefetch/ergo_pkg_alpha")
    pkg_fail("the scratch sub-build must be removed")
endif()
prefetch(pass alpha)      # second run: already at the pin
if(NOT PREFETCH_LOG MATCHES "already at")
    pkg_fail("second prefetch should leave the source alone\n${PREFETCH_LOG}")
endif()
pkg_pass("prefetch to FetchContent's place, idempotent")

# --- offline configure: route 5 from the prefetched source ---------------------
file(RENAME "${W}/repos/Alpha" "${W}/repos/Alpha.away")
execute_process(
    COMMAND "${CMAKE_COMMAND}" ${_gen_args} -S "${FIX}/hub_host" -B "${W}/build"
        "-DERGO_SOURCE_DIR=${ERGO_SOURCE_DIR}"
        "-DERGO_PACKAGES_MANIFEST_FILE=${W}/m/ergo-packages.json"
        "-DFETCHCONTENT_BASE_DIR=${BASE}"
        -DERGO_PACKAGES=alpha -DERGO_PACKAGES_FETCH=ON
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0 OR NOT EXISTS "${W}/build/hub_fixture_ok.txt")
    pkg_fail("offline configure after prefetch should pass\n${_out}\n${_err}")
endif()
file(READ "${W}/build/ergo-packages.resolved.json" _json)
string(JSON _route GET "${_json}" packages alpha route)
string(JSON _head GET "${_json}" packages alpha head)
pkg_assert_eq("route 5" "${_route}" fetch)
pkg_assert_eq("fetched head" "${_head}" "${C2}")
if(_out MATCHES "PKG-08")
    pkg_fail("the pin descends from v1.0.0: no PKG-08 with full history\n${_out}")
endif()
pkg_pass("ERGO_PACKAGES_FETCH=ON configure hits route 5 without network after prefetch")
