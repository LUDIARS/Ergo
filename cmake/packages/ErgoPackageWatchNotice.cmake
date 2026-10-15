# ---------------------------------------------------------------------------
# ErgoPackageWatchNotice.cmake — the configure-time line of `ergo_pkg watch`
#
# One responsibility: turn the host's watch state file into the one line
# spec/feature/package-manager.md §4 ("構成時の 1 行") asks for:
#
#   -- ergo packages: 2 update(s) known for host Ln
#
# The state file is the one `ergo_pkg watch` writes by default
# (tools/package_cli/watch_paths.cpp): <state>/<code>.watch-state.json with
# <state> = %LOCALAPPDATA%/ergo/pkg-watch on Windows, else
# $XDG_STATE_HOME/ergo/pkg-watch, else ~/.local/state/ergo/pkg-watch. Format:
# spec/data/ergo-watch-report.md (schema 1, `host`, `entries`).
#
# Reads one local file and never touches the network. A missing, broken or
# other host's state file gives no line and never stops the configure. Pure
# functions, so tests/packages/test_watch_notice.cmake runs them under
# `cmake -P`.
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

# ergo_pkg_watch_default_state_dir(<out>)
# The default state directory of `ergo_pkg watch` ("" when the environment
# gives none).
function(ergo_pkg_watch_default_state_dir out_var)
    set(_dir "")
    if(CMAKE_HOST_WIN32)
        if(NOT "$ENV{LOCALAPPDATA}" STREQUAL "")
            set(_dir "$ENV{LOCALAPPDATA}/ergo/pkg-watch")
        endif()
    elseif(NOT "$ENV{XDG_STATE_HOME}" STREQUAL "")
        set(_dir "$ENV{XDG_STATE_HOME}/ergo/pkg-watch")
    elseif(NOT "$ENV{HOME}" STREQUAL "")
        set(_dir "$ENV{HOME}/.local/state/ergo/pkg-watch")
    endif()
    file(TO_CMAKE_PATH "${_dir}" _dir)
    set(${out_var} "${_dir}" PARENT_SCOPE)
endfunction()

# ergo_pkg_watch_notice(<out_line> CODE <code> STATE_DIR <dir>)
# "ergo packages: N update(s) known for host <code>" when
# <dir>/<code>.watch-state.json is a schema-1 state of host <code>; "" when
# the code is not a project code, the file is missing or broken, or it
# belongs to another host.
function(ergo_pkg_watch_notice out_var)
    cmake_parse_arguments(A "" "CODE;STATE_DIR" "" ${ARGN})
    set(${out_var} "" PARENT_SCOPE)
    # Same rule as ergo_package_manager's is_valid_code (also keeps the file
    # name inside the state directory).
    if("${A_STATE_DIR}" STREQUAL "" OR NOT "${A_CODE}" MATCHES "^[A-Za-z][A-Za-z0-9_-]*$")
        return()
    endif()
    set(_file "${A_STATE_DIR}/${A_CODE}.watch-state.json")
    if(NOT EXISTS "${_file}" OR IS_DIRECTORY "${_file}")
        return()
    endif()
    file(READ "${_file}" _json)
    string(JSON _schema ERROR_VARIABLE _err GET "${_json}" schema)
    if(_err OR NOT _schema STREQUAL "1")
        return()
    endif()
    string(JSON _host ERROR_VARIABLE _err GET "${_json}" host)
    if(_err OR NOT _host STREQUAL A_CODE)
        return()
    endif()
    set(_count 0)
    string(JSON _type ERROR_VARIABLE _err TYPE "${_json}" entries)
    if(NOT _err)
        if(NOT _type STREQUAL "ARRAY")
            return()
        endif()
        string(JSON _count ERROR_VARIABLE _err LENGTH "${_json}" entries)
        if(_err)
            return()
        endif()
    endif()
    set(${out_var} "ergo packages: ${_count} update(s) known for host ${A_CODE}" PARENT_SCOPE)
endfunction()
