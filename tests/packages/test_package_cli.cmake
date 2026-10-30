# W1: the ergo_pkg CLI end to end (spec/feature/package-manager.md §2.2 / §7).
#   * init twice: the second run changes nothing, .gitignore holds .ergo/ once
#   * init in a directory whose ergo-packages.json is an Ergo manifest: exit 2
#   * init --upm writes .ergo/host.json only
#   * doctor: clean -> 0; stale bootstrap -> 1; leak (§8) -> 1; back to 0
#   * list --json
#   * fetch of a private package into --cache (FetchContent's place)
#   * --help lists all ten commands; `<command> --help` prints that command's
#     synopsis and exits 0 (one command table drives parsing and help)
# Ergo is a fake git checkout made from this tree's manifest and
# cmake/packages/ (so init has a SHA); no network.
#
#   cmake -DERGO_SOURCE_DIR=<ergo> -DERGO_TEST_WORK_DIR=<dir> -DERGO_PKG_CLI=<ergo_pkg>
#         [-DGIT_EXECUTABLE=<git>] -P tests/packages/test_package_cli.cmake
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/pkg_git_support.cmake")

if(NOT ERGO_PKG_CLI)
    pkg_fail("-DERGO_PKG_CLI=<path of ergo_pkg> is required")
endif()
file(TO_CMAKE_PATH "${ERGO_TEST_WORK_DIR}" W)
file(REMOVE_RECURSE "${W}")
file(MAKE_DIRECTORY "${W}")

# Fake Ergo checkout.
file(COPY "${ERGO_SOURCE_DIR}/ergo-packages.json" DESTINATION "${W}/ergo")
file(COPY "${ERGO_SOURCE_DIR}/cmake/packages" DESTINATION "${W}/ergo/cmake")
pkg_git("${W}/ergo" init -q)
pkg_git("${W}/ergo" add -A)
pkg_git("${W}/ergo" commit -q -m "fake ergo")
pkg_git("${W}/ergo" rev-parse HEAD)
set(ERGO_SHA "${GIT_OUT}")

# pkg(<expected exit code> <args...>) — output in PKG_OUT.
function(pkg expected)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env --unset=ERGO_DEPENDENCY_CACHE_DIR --unset=ERGO_SOURCE_DIR
            "${ERGO_PKG_CLI}" ${ARGN} --ergo "${W}/ergo"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL expected)
        pkg_fail("ergo_pkg ${ARGN}: exit ${_rc}, expected ${expected}\n${_out}\n${_err}")
    endif()
    set(PKG_OUT "${_out}${_err}" PARENT_SCOPE)
endfunction()

function(hash_files out dir)
    set(_h "")
    foreach(_f ergo-packages.json cmake/ergo-bootstrap.cmake .gitignore)
        file(SHA256 "${dir}/${_f}" _one)
        string(APPEND _h "${_f}=${_one};")
    endforeach()
    set(${out} "${_h}" PARENT_SCOPE)
endfunction()

# --- help -----------------------------------------------------------------------
set(_commands init list doctor fetch add remove update outdated watch status)
pkg(0 --help)
foreach(_c IN LISTS _commands)
    if(NOT PKG_OUT MATCHES "ergo_pkg ${_c} ")
        pkg_fail("--help does not list `ergo_pkg ${_c}`\n${PKG_OUT}")
    endif()
endforeach()
foreach(_c add remove update)
    pkg(0 ${_c} --help)
    if(NOT PKG_OUT MATCHES "ergo_pkg ${_c} <name>")
        pkg_fail("`${_c} --help` lacks its synopsis\n${PKG_OUT}")
    endif()
    foreach(_other IN LISTS _commands)
        if(NOT _other STREQUAL _c AND PKG_OUT MATCHES "ergo_pkg ${_other} ")
            pkg_fail("`${_c} --help` should print only ${_c}\n${PKG_OUT}")
        endif()
    endforeach()
endforeach()
pkg_pass("help: all ten commands, per-command synopsis")

# --- init ---------------------------------------------------------------------
set(H "${W}/host")
file(WRITE "${H}/.gitignore" "build/\n")
pkg(0 init --code Example --host "${H}")
hash_files(first "${H}")
pkg(0 init --code Example --host "${H}")
if(NOT PKG_OUT MATCHES "nothing to change")
    pkg_fail("second init should change nothing\n${PKG_OUT}")
endif()
hash_files(second "${H}")
pkg_assert_eq("second init leaves the bytes" "${second}" "${first}")
file(STRINGS "${H}/.gitignore" _lines)
list(FILTER _lines INCLUDE REGEX "^\\.ergo/$")
list(LENGTH _lines _n)
pkg_assert_eq(".ergo/ once in .gitignore" "${_n}" "1")
file(READ "${H}/cmake/ergo-bootstrap.cmake" _boot)
if(NOT _boot MATCHES "# ergo-bootstrap-source: ${ERGO_SHA}")
    pkg_fail("bootstrap must name its source SHA")
endif()
file(READ "${H}/ergo-packages.json" _decl)
string(JSON _rev GET "${_decl}" host ergo rev)
pkg_assert_eq("declaration pins Ergo" "${_rev}" "${ERGO_SHA}")
pkg_pass("init twice: second run unchanged")

file(MAKE_DIRECTORY "${W}/not_host")
file(COPY "${ERGO_SOURCE_DIR}/ergo-packages.json" DESTINATION "${W}/not_host")
pkg(2 init --host "${W}/not_host")
pkg(0 init --upm --code Example --host "${W}/unity")
if(NOT EXISTS "${W}/unity/.ergo/host.json" OR EXISTS "${W}/unity/ergo-packages.json"
   OR EXISTS "${W}/unity/.gitignore")
    pkg_fail("init --upm must write .ergo/host.json only")
endif()
pkg(2 nosuch)
pkg_pass("init refuses an Ergo manifest, --upm writes host.json only")

# --- doctor -------------------------------------------------------------------
pkg(0 doctor --host "${H}")
pkg(0 doctor --strict --json --host "${H}")
string(JSON _state GET "${PKG_OUT}" bootstrap state)
pkg_assert_eq("bootstrap current" "${_state}" current)

file(APPEND "${H}/cmake/ergo-bootstrap.cmake" "# local edit\n")
pkg(1 doctor --json --host "${H}")
string(JSON _state GET "${PKG_OUT}" bootstrap state)
pkg_assert_eq("bootstrap stale" "${_state}" stale)
pkg(0 init --host "${H}")   # refreshes the stale copy
pkg(0 doctor --host "${H}")
pkg_pass("doctor: bootstrap freshness true and false")

string(REPLACE "\"legacy_vars\": {}" "\"legacy_vars\": {}, \"notes\": \"/home/someone/ergo\""
    _leaky "${_decl}")
file(WRITE "${H}/ergo-packages.json" "${_leaky}")
pkg(1 doctor --host "${H}")
if(NOT PKG_OUT MATCHES "LEAK\\|error\\|ergo-packages.json")
    pkg_fail("doctor should report the leak\n${PKG_OUT}")
endif()
file(WRITE "${H}/ergo-packages.json" "${_decl}")
pkg(0 doctor --host "${H}")
pkg_pass("doctor: leak check true and false")

# --- list ---------------------------------------------------------------------
pkg(0 list --json --host "${H}")
string(JSON _name GET "${PKG_OUT}" packages 0 name)
pkg_assert_eq("list starts with Ergo" "${_name}" ergo)
string(JSON _host GET "${PKG_OUT}" host)
pkg_assert_eq("list host" "${_host}" Example)
pkg(0 list --host "${H}")
pkg_pass("list")

# --- fetch (private package of the host declaration) --------------------------
pkg_make_repo(P1 "${CMAKE_CURRENT_LIST_DIR}/fixtures/alpha" "${W}/repos/Privlib")
pkg_file_url(P_URL "${W}/repos/Privlib")
set(H2 "${W}/host2")
pkg(0 init --code Example --host "${H2}")
file(READ "${H2}/ergo-packages.json" _d2)
string(REPLACE "\"packages\": {}"
    "\"packages\": { \"privlib\": { \"kind\": \"link\", \"repository\": \"${P_URL}\", \"rev\": \"${P1}\" } }"
    _d2 "${_d2}")
string(REPLACE "\"use\": []" "\"use\": [\"privlib\"]" _d2 "${_d2}")
file(WRITE "${H2}/ergo-packages.json" "${_d2}")
pkg(2 fetch --host "${H2}")                       # nowhere to fetch to
pkg(2 fetch nosuch --cache "${W}/f" --host "${H2}")
pkg(0 fetch --cache "${W}/f" --host "${H2}")      # names from host.use
pkg_git("${W}/f/ergo_pkg_privlib-src" rev-parse HEAD)
pkg_assert_eq("fetched at the pin" "${GIT_OUT}" "${P1}")
pkg_pass("fetch into FetchContent's place")
