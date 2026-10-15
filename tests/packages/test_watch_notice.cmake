# The configure-time line of `ergo_pkg watch` (spec/feature/package-manager.md
# §4 "構成時の 1 行", §7 W5):
#   * no state file / broken state file / another host's state -> no line
#   * a state of the host with N entries -> "ergo packages: N update(s) known
#     for host <code>"
#   * the default state directory follows `ergo_pkg watch`
#
#   cmake -DERGO_SOURCE_DIR=<ergo> -DERGO_TEST_WORK_DIR=<dir> -P tests/packages/test_watch_notice.cmake
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")
include("${ERGO_SOURCE_DIR}/cmake/packages/ErgoPackageWatchNotice.cmake")

file(TO_CMAKE_PATH "${ERGO_TEST_WORK_DIR}" W)
file(REMOVE_RECURSE "${W}")
file(MAKE_DIRECTORY "${W}")

# One state entry as `ergo_pkg watch` writes it (spec/data/ergo-watch-report.md).
function(_entry out dep subject)
    set(${out} "{ \"dependency\": \"${dep}\", \"kind\": \"tag\", \"subject\": \"${subject}\",
      \"from_rev\": \"1111111\", \"from_tag\": \"v2.2.0\", \"to_rev\": \"2222222\", \"to_tag\": \"${subject}\",
      \"behind\": 41, \"warning\": false, \"strict_ok\": true, \"next\": \"\",
      \"first_seen\": \"2026-10-15T00:00:00Z\", \"last_notified\": \"2026-10-15T00:00:00Z\", \"notified_behind\": null }"
        PARENT_SCOPE)
endfunction()

# --- no state file ----------------------------------------------------------------
ergo_pkg_watch_notice(line CODE Example STATE_DIR "${W}")
pkg_assert_eq("no state file" "${line}" "")
ergo_pkg_watch_notice(line CODE Example STATE_DIR "")
pkg_assert_eq("no state directory" "${line}" "")
ergo_pkg_watch_notice(line CODE Example STATE_DIR "${W}/missing")
pkg_assert_eq("missing state directory" "${line}" "")
pkg_pass("no state file gives no line")

# --- broken state file ------------------------------------------------------------
file(WRITE "${W}/Example.watch-state.json" "{ \"schema\": 1, \"host\": \"Example\", \"entries\": [")
ergo_pkg_watch_notice(line CODE Example STATE_DIR "${W}")
pkg_assert_eq("truncated JSON" "${line}" "")
file(WRITE "${W}/Example.watch-state.json" "")
ergo_pkg_watch_notice(line CODE Example STATE_DIR "${W}")
pkg_assert_eq("empty file" "${line}" "")
file(WRITE "${W}/Example.watch-state.json" "[]\n")
ergo_pkg_watch_notice(line CODE Example STATE_DIR "${W}")
pkg_assert_eq("not an object" "${line}" "")
file(WRITE "${W}/Example.watch-state.json" "{ \"schema\": 2, \"host\": \"Example\", \"entries\": [] }\n")
ergo_pkg_watch_notice(line CODE Example STATE_DIR "${W}")
pkg_assert_eq("other schema" "${line}" "")
file(WRITE "${W}/Example.watch-state.json" "{ \"schema\": 1, \"host\": \"Example\", \"entries\": {} }\n")
ergo_pkg_watch_notice(line CODE Example STATE_DIR "${W}")
pkg_assert_eq("entries is not an array" "${line}" "")
file(REMOVE "${W}/Example.watch-state.json")
file(MAKE_DIRECTORY "${W}/Example.watch-state.json")
ergo_pkg_watch_notice(line CODE Example STATE_DIR "${W}")
pkg_assert_eq("a directory in its place" "${line}" "")
file(REMOVE_RECURSE "${W}/Example.watch-state.json")
pkg_pass("a broken state file gives no line")

# --- another host -----------------------------------------------------------------
_entry(e1 pictor v2.3.0)
file(WRITE "${W}/Example.watch-state.json"
    "{ \"schema\": 1, \"host\": \"Other\", \"fingerprints\": {}, \"entries\": [ ${e1} ] }\n")
ergo_pkg_watch_notice(line CODE Example STATE_DIR "${W}")
pkg_assert_eq("state of another host" "${line}" "")
file(WRITE "${W}/Example.watch-state.json"
    "{ \"schema\": 1, \"fingerprints\": {}, \"entries\": [ ${e1} ] }\n")
ergo_pkg_watch_notice(line CODE Example STATE_DIR "${W}")
pkg_assert_eq("state without a host" "${line}" "")
file(WRITE "${W}/Other.watch-state.json"
    "{ \"schema\": 1, \"host\": \"Other\", \"fingerprints\": {}, \"entries\": [ ${e1} ] }\n")
ergo_pkg_watch_notice(line CODE "../Other" STATE_DIR "${W}")
pkg_assert_eq("not a project code" "${line}" "")
ergo_pkg_watch_notice(line CODE "" STATE_DIR "${W}")
pkg_assert_eq("empty code" "${line}" "")
pkg_pass("another host's state gives no line")

# --- N updates --------------------------------------------------------------------
_entry(e2 tela v0.4.0)
file(WRITE "${W}/Example.watch-state.json"
    "{ \"schema\": 1, \"host\": \"Example\", \"fingerprints\": { \"pictor\": \"abc\" }, \"entries\": [ ${e1}, ${e2} ] }\n")
ergo_pkg_watch_notice(line CODE Example STATE_DIR "${W}")
pkg_assert_eq("two updates" "${line}" "ergo packages: 2 update(s) known for host Example")
file(WRITE "${W}/Example.watch-state.json"
    "{ \"schema\": 1, \"host\": \"Example\", \"fingerprints\": {}, \"entries\": [] }\n")
ergo_pkg_watch_notice(line CODE Example STATE_DIR "${W}")
pkg_assert_eq("no updates" "${line}" "ergo packages: 0 update(s) known for host Example")
file(WRITE "${W}/Example.watch-state.json" "{ \"schema\": 1, \"host\": \"Example\" }\n")
ergo_pkg_watch_notice(line CODE Example STATE_DIR "${W}")
pkg_assert_eq("no entries key" "${line}" "ergo packages: 0 update(s) known for host Example")
pkg_pass("a state of the host gives the count")

# --- default state directory ------------------------------------------------------
set(_saved_local "$ENV{LOCALAPPDATA}")
set(_saved_xdg "$ENV{XDG_STATE_HOME}")
set(_saved_home "$ENV{HOME}")
set(ENV{LOCALAPPDATA} "${W}/local")
set(ENV{XDG_STATE_HOME} "${W}/xdg")
set(ENV{HOME} "${W}/home")
ergo_pkg_watch_default_state_dir(dir)
if(CMAKE_HOST_WIN32)
    pkg_assert_eq("Windows default" "${dir}" "${W}/local/ergo/pkg-watch")
else()
    pkg_assert_eq("XDG default" "${dir}" "${W}/xdg/ergo/pkg-watch")
    set(ENV{XDG_STATE_HOME} "")
    ergo_pkg_watch_default_state_dir(dir)
    pkg_assert_eq("HOME default" "${dir}" "${W}/home/.local/state/ergo/pkg-watch")
endif()
set(ENV{LOCALAPPDATA} "${_saved_local}")
set(ENV{XDG_STATE_HOME} "${_saved_xdg}")
set(ENV{HOME} "${_saved_home}")
pkg_pass("default state directory follows ergo_pkg watch")
