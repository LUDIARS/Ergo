# W1: the host bootstrap (cmake/packages/ergo-bootstrap.cmake.in, written by
# `ergo_pkg init`) finds Ergo in the documented order — ERGO_SOURCE_DIR (or a
# legacy alias) -> the sibling checkout -> fetch only when
# ERGO_BOOTSTRAP_FETCH=ON — and without that switch never goes to the
# network (spec/feature/package-manager.md §2.1 / §7).
#
# Fake Ergo checkouts (fixtures/boot_ergo) and fake hosts (fixtures/boot_host)
# are laid out in the work dir; project NONE, no compiler. Only the fetch_ok
# case uses git: a fake Ergo repository of two commits, reached by file://
# URL and pinned at the first (untagged, behind the tip) commit.
#
#   cmake -DERGO_SOURCE_DIR=<ergo> -DERGO_TEST_WORK_DIR=<dir>
#         [-DGIT_EXECUTABLE=<git>] [-DERGO_TEST_GENERATOR=<gen>]
#         -P tests/packages/test_bootstrap.cmake
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/pkg_git_support.cmake")

file(TO_CMAKE_PATH "${ERGO_TEST_WORK_DIR}" W)
file(REMOVE_RECURSE "${W}")
file(MAKE_DIRECTORY "${W}")
set(FIX "${CMAKE_CURRENT_LIST_DIR}/fixtures")
set(_gen_args "")
if(ERGO_TEST_GENERATOR)
    set(_gen_args -G "${ERGO_TEST_GENERATOR}")
endif()
set(SHA 1111111111111111111111111111111111111111)

function(make_ergo dir name)
    file(COPY "${FIX}/boot_ergo/" DESTINATION "${dir}")
    file(WRITE "${dir}/name.txt" "${name}\n")
endfunction()

# make_host(<dir> <repository> <rev>) — host declaration + generated bootstrap.
function(make_host dir repository rev)
    file(COPY "${FIX}/boot_host/" DESTINATION "${dir}")
    file(WRITE "${dir}/ergo-packages.json" "{
  \"schema\": 1,
  \"host\": {
    \"code\": \"Example\",
    \"ergo\": { \"repository\": \"${repository}\", \"rev\": \"${rev}\" },
    \"use\": [], \"optional\": [], \"features\": {},
    \"legacy_vars\": { \"EXAMPLE_ERGO_DIR\": \"ergo.source_dir\" }
  },
  \"packages\": {}
}
")
    set(ERGO_BOOTSTRAP_SOURCE "${SHA}")
    configure_file("${ERGO_SOURCE_DIR}/cmake/packages/ergo-bootstrap.cmake.in"
        "${dir}/cmake/ergo-bootstrap.cmake" @ONLY)
endfunction()

# run_case(<name> <host dir> <pass|fail> <log regex> <cache args...>)
function(run_case name host expect pattern)
    set(_build "${W}/b/${name}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${_gen_args} -S "${host}" -B "${_build}" ${ARGN}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    set(_all "${_out}\n${_err}")
    # CMake wraps long messages at spaces: let a space match any wrap.
    string(REPLACE " " "[ \n]+" pattern "${pattern}")
    if(expect STREQUAL "pass" AND NOT _rc EQUAL 0)
        pkg_fail("${name}: configure should pass\n${_all}")
    elseif(expect STREQUAL "fail" AND _rc EQUAL 0)
        pkg_fail("${name}: configure should fail\n${_all}")
    endif()
    if(NOT _all MATCHES "${pattern}")
        pkg_fail("${name}: output lacks '${pattern}'\n${_all}")
    endif()
    set(CASE_BUILD "${_build}" PARENT_SCOPE)
endfunction()

function(expect_facts label route ergo)
    file(READ "${CASE_BUILD}/boot_facts.txt" _facts)
    pkg_assert_eq("${label}" "${_facts}" "route=${route}\nergo=${ergo}\n")
endfunction()

# Workspace with a sibling checkout named after the repository (Ergo).
make_ergo("${W}/ws/Ergo" sibling)
make_ergo("${W}/other/ErgoSource" source)
make_host("${W}/ws/Host" "https://github.com/LUDIARS/Ergo.git" "${SHA}")

# 1. ERGO_SOURCE_DIR wins over the sibling.
run_case(source "${W}/ws/Host" pass "" "-DERGO_SOURCE_DIR=${W}/other/ErgoSource")
expect_facts("ERGO_SOURCE_DIR first" source source)
# A legacy alias of ergo.source_dir is honoured with a deprecation message.
run_case(legacy "${W}/ws/Host" pass "EXAMPLE_ERGO_DIR is a legacy alias"
    "-DEXAMPLE_ERGO_DIR=${W}/other/ErgoSource")
expect_facts("legacy alias" source source)
# 2. The sibling checkout next to the host.
run_case(sibling "${W}/ws/Host" pass "")
expect_facts("sibling second" sibling sibling)
# A wrong ERGO_SOURCE_DIR does not fall through to the sibling.
run_case(source_wrong "${W}/ws/Host" fail "is not an Ergo checkout"
    "-DERGO_SOURCE_DIR=${W}/nowhere")
pkg_pass("bootstrap order: ERGO_SOURCE_DIR -> sibling")

# 3. No checkout and fetching off: stop without any network access. The
#    repository does not exist; FetchContent would leave _deps behind.
if(W MATCHES "^/")
    set(_missing_url "file://${W}/nowhere/Ergo.git")
else()
    set(_missing_url "file:///${W}/nowhere/Ergo.git")
endif()
make_host("${W}/lonely/Host" "${_missing_url}" "${SHA}")
run_case(fetch_off "${W}/lonely/Host" fail "Ergo not found.*ERGO_BOOTSTRAP_FETCH=ON")
if(EXISTS "${CASE_BUILD}/_deps")
    pkg_fail("fetch_off: the bootstrap must not start FetchContent")
endif()
# Fetching on but the pin is not a SHA: PKG-02 before any download.
make_host("${W}/lonely2/Host" "${_missing_url}" main)
run_case(fetch_bad_rev "${W}/lonely2/Host" fail "PKG-02 ergo" -DERGO_BOOTSTRAP_FETCH=ON)
if(EXISTS "${CASE_BUILD}/_deps")
    pkg_fail("fetch_bad_rev: the bootstrap must not start FetchContent")
endif()
pkg_pass("bootstrap fetches only when ERGO_BOOTSTRAP_FETCH=ON (no network otherwise)")

# 4. Fetching on: host.ergo.rev is the first of two commits, untagged and
#    behind the tip, as a real pin is once Ergo's main moves on. A shallow
#    clone reaches only branch and tag tips and could not check it out.
make_ergo("${W}/repos/Ergo" fetched)
pkg_git("${W}/repos/Ergo" init -q)
pkg_git("${W}/repos/Ergo" add -A)
pkg_git("${W}/repos/Ergo" commit -q -m "pin")
pkg_git("${W}/repos/Ergo" rev-parse HEAD)
set(_pin "${GIT_OUT}")
file(WRITE "${W}/repos/Ergo/name.txt" "tip\n")
pkg_git("${W}/repos/Ergo" commit -q -am "tip")
pkg_file_url(_ergo_url "${W}/repos/Ergo")
make_host("${W}/fetch/Host" "${_ergo_url}" "${_pin}")
run_case(fetch_ok "${W}/fetch/Host" pass "" -DERGO_BOOTSTRAP_FETCH=ON "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}")
expect_facts("fetch at a pin behind the tip" fetch fetched)
pkg_pass("bootstrap fetches Ergo at a SHA pin behind the tip (ERGO_BOOTSTRAP_FETCH=ON, file://)")
