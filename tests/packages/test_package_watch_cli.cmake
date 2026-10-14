# W3: ergo_pkg outdated / watch end to end (spec/feature/package-manager.md §2.2 / §2.3 / §3 / §7 W3).
#   * outdated: tag (the newest v* tag past expect_tag) and track, --fail-on
#   * watch: events.json + <host>/.ergo/watch-report.json + state; the second run has no event and
#     leaves the state bytes unchanged
#   * a repository that cannot be reached is observed:false and leaves the state unchanged
#   * a sibling checkout is read as it is (no fetch) with as_of sibling-last-fetch
#   * a report that would leak (§8) is not written: exit 3
# Every repository is a fake one made here and reached by file:// URL; no network.
#
#   cmake -DERGO_SOURCE_DIR=<ergo> -DERGO_TEST_WORK_DIR=<dir> -DERGO_PKG_CLI=<ergo_pkg>
#         [-DGIT_EXECUTABLE=<git>] -P tests/packages/test_package_watch_cli.cmake
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/pkg_git_support.cmake")

if(NOT ERGO_PKG_CLI)
    pkg_fail("-DERGO_PKG_CLI=<path of ergo_pkg> is required")
endif()
file(TO_CMAKE_PATH "${ERGO_TEST_WORK_DIR}" W)
file(REMOVE_RECURSE "${W}")
file(MAKE_DIRECTORY "${W}")

# Fake Ergo checkout (the CLI reads its manifest; nothing here points at a public remote).
file(COPY "${ERGO_SOURCE_DIR}/ergo-packages.json" DESTINATION "${W}/ergo")
file(COPY "${ERGO_SOURCE_DIR}/cmake/packages" DESTINATION "${W}/ergo/cmake")

# pkg(<expected exit code> <args...>) — stdout in PKG_STDOUT, both streams in PKG_OUT.
function(pkg expected)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env --unset=ERGO_DEPENDENCY_CACHE_DIR --unset=ERGO_PKG_CACHE_DIR
            --unset=ERGO_PACKAGES_SIBLING_ROOT --unset=ERGO_SOURCE_DIR
            "${ERGO_PKG_CLI}" ${ARGN} --ergo "${W}/ergo"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL expected)
        pkg_fail("ergo_pkg ${ARGN}: exit ${_rc}, expected ${expected}\n${_out}\n${_err}")
    endif()
    set(PKG_STDOUT "${_out}" PARENT_SCOPE)
    set(PKG_OUT "${_out}${_err}" PARENT_SCOPE)
endfunction()

function(commit_file dir name)
    file(WRITE "${dir}/${name}" "${name}\n")
    pkg_git("${dir}" add -A)
    pkg_git("${dir}" commit -q -m "${name}")
    pkg_git("${dir}" rev-parse HEAD)
    set(GIT_OUT "${GIT_OUT}" PARENT_SCOPE)
endfunction()

# --- upstream: c1 (v1.0.0, annotated) <- c2 (v1.1.0) <- c3 = main ------------------------------
set(UP "${W}/remote/Privlib")
file(MAKE_DIRECTORY "${UP}")
pkg_git("${UP}" init -q)
commit_file("${UP}" one)
set(C1 "${GIT_OUT}")
pkg_git("${UP}" tag -a v1.0.0 -m v1.0.0)
commit_file("${UP}" two)
set(C2 "${GIT_OUT}")
pkg_git("${UP}" tag v1.1.0)
commit_file("${UP}" three)
set(C3 "${GIT_OUT}")
pkg_file_url(UP_URL "${UP}")
pkg_file_url(MISSING_URL "${W}/remote/Missing")

# write_host(<dir> <package name> <repository url>) — a host declaration with one private package.
function(write_host dir name url)
    file(WRITE "${dir}/ergo-packages.json" "{
  \"schema\": 1,
  \"host\": { \"code\": \"Example\", \"use\": [\"${name}\"] },
  \"packages\": {
    \"${name}\": { \"kind\": \"link\", \"code\": \"Example\", \"repository\": \"${url}\",
                 \"rev\": \"${C1}\", \"expect_tag\": \"v1.0.0\" }
  }
}
")
endfunction()

set(H "${W}/host")
write_host("${H}" privlib "${UP_URL}")

# --- outdated -----------------------------------------------------------------------------------
pkg(0 outdated --json --host "${H}")
string(JSON _kind GET "${PKG_STDOUT}" dependencies 0 updates 0 kind)
pkg_assert_eq("outdated: first update" "${_kind}" tag)
string(JSON _tag GET "${PKG_STDOUT}" dependencies 0 updates 0 to_tag)
pkg_assert_eq("outdated: newest tag past expect_tag" "${_tag}" v1.1.0)
string(JSON _rev GET "${PKG_STDOUT}" dependencies 0 updates 0 to_rev)
pkg_assert_eq("outdated: annotated tags are peeled" "${_rev}" "${C2}")
string(JSON _kind GET "${PKG_STDOUT}" dependencies 0 updates 1 kind)
pkg_assert_eq("outdated: track" "${_kind}" track)
string(JSON _type TYPE "${PKG_STDOUT}" dependencies 0 updates 1 behind)
pkg_assert_eq("outdated: no mirror, no distance" "${_type}" NULL)
pkg(1 outdated --fail-on tag --host "${H}")
pkg(0 outdated --fail-on off-main --host "${H}")
pkg(2 outdated --fail-on weird --host "${H}")
pkg_pass("outdated: tag and track")

# --- watch: first run, second run ---------------------------------------------------------------
pkg_git("${W}" clone -q --bare "${UP_URL}" "${W}/cache/mirrors/Privlib.git")
set(WATCH watch --host "${H}" --state "${W}/state" --out "${W}/out" --cache "${W}/cache")
pkg(0 ${WATCH})
file(READ "${W}/out/events.json" _events)
string(JSON _n LENGTH "${_events}" events)
pkg_assert_eq("first watch: tag + track" "${_n}" 2)
string(JSON _id GET "${_events}" events 0 id)
pkg_assert_eq("event id" "${_id}" "Example/privlib/tag/v1.1.0")
if(_events MATCHES "file:")
    pkg_fail("events.json must not carry the repository URL\n${_events}")
endif()
file(READ "${H}/.ergo/watch-report.json" _report)
string(JSON _behind GET "${_report}" dependencies 0 track behind)
pkg_assert_eq("mirror gives the distance" "${_behind}" 2)
set(STATE "${W}/state/Example.watch-state.json")
file(SHA256 "${STATE}" _state1)

pkg(0 ${WATCH})
file(READ "${W}/out/events.json" _events)
string(JSON _n LENGTH "${_events}" events)
pkg_assert_eq("second watch: no event" "${_n}" 0)
file(SHA256 "${STATE}" _state2)
pkg_assert_eq("second watch: state bytes" "${_state2}" "${_state1}")
file(READ "${H}/.ergo/watch-report.json" _report)
string(JSON _n LENGTH "${_report}" pending)
pkg_assert_eq("known updates stay pending" "${_n}" 2)
pkg_pass("watch: second run has no event")

# --- an unreachable repository leaves the state as it was ---------------------------------------
write_host("${H}" privlib "${MISSING_URL}")
pkg(0 ${WATCH})
file(READ "${H}/.ergo/watch-report.json" _report)
string(JSON _observed GET "${_report}" dependencies 0 observed)
pkg_assert_eq("unreachable: observed false" "${_observed}" OFF)
file(SHA256 "${STATE}" _state3)
pkg_assert_eq("unreachable: state unchanged" "${_state3}" "${_state1}")
write_host("${H}" privlib "${UP_URL}")
pkg_pass("watch: a failed fetch is not 'no update'")

# --- sibling checkout: read as of its last fetch ------------------------------------------------
pkg_git("${W}" clone -q "${UP_URL}" "${W}/root/Privlib")
commit_file("${UP}" four)   # upstream moves on; the sibling does not fetch it
pkg(0 outdated --json --root "${W}/root" --host "${H}")
string(JSON _as_of GET "${PKG_STDOUT}" dependencies 0 as_of)
pkg_assert_eq("sibling: as_of" "${_as_of}" sibling-last-fetch)
string(JSON _head GET "${PKG_STDOUT}" dependencies 0 track head)
pkg_assert_eq("sibling: head as of its last fetch" "${_head}" "${C3}")
string(JSON _sb GET "${PKG_STDOUT}" dependencies 0 sibling_behind)
pkg_assert_eq("sibling: its own branch is current" "${_sb}" 0)
pkg_git("${W}/root/Privlib" rev-parse refs/remotes/origin/main)
pkg_assert_eq("sibling: not fetched" "${GIT_OUT}" "${C3}")
pkg_pass("outdated: sibling checkout without fetch")

# --- a report that would leak is not written ----------------------------------------------------
set(H3 "${W}/leaky")
write_host("${H3}" "lib/home/someone" "${UP_URL}")
pkg(3 watch --host "${H3}" --state "${W}/state3")
if(EXISTS "${H3}/.ergo/watch-report.json" OR EXISTS "${W}/state3/events.json"
   OR EXISTS "${W}/state3/Example.watch-state.json")
    pkg_fail("nothing may be written when the report would leak")
endif()
pkg_pass("watch: leak check blocks the report (exit 3)")
