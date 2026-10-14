# W3: the `track` key (packages.<name>.track.branch / host.ergo.track.branch) is read by the C++ watch
# only. Adding it must not change what the CMake reader (ergo_pkg_manifest_load) hands back
# (spec/feature/package-manager.md §7 W3, spec/data/ergo-packages.md).
#
#   cmake -DERGO_SOURCE_DIR=<ergo> -DERGO_TEST_WORK_DIR=<dir> -P tests/packages/test_track_key.cmake
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")

file(TO_CMAKE_PATH "${ERGO_TEST_WORK_DIR}" W)
file(REMOVE_RECURSE "${W}")

# Every variable of a load, with the manifest's own directory replaced by a marker.
function(load_values out file dir)
    ergo_pkg_manifest_load(m "${file}" ${ARGN})
    set(_values "")
    foreach(_v IN ITEMS SCHEMA ERRORS PACKAGES LINK TOOLS MODULES OBSERVE HOST_PRESENT HOST_CODE)
        list(APPEND _values "${_v}=${m_${_v}}")
    endforeach()
    foreach(_v IN LISTS m_VARS)
        string(REPLACE "${dir}" "<dir>" _value "${${_v}}")
        list(APPEND _values "${_v}=${_value}")
    endforeach()
    set(${out} "${_values}" PARENT_SCOPE)
endfunction()

# --- the real Ergo manifest, with and without track on every link package ----------------------
file(READ "${ERGO_SOURCE_DIR}/ergo-packages.json" _plain)
set(_tracked "${_plain}")
string(JSON _count LENGTH "${_plain}" packages)
math(EXPR _last "${_count} - 1")
foreach(_i RANGE ${_last})
    string(JSON _n MEMBER "${_plain}" packages ${_i})
    string(JSON _kind GET "${_plain}" packages "${_n}" kind)
    if(_kind STREQUAL "link")
        string(JSON _tracked SET "${_tracked}" packages "${_n}" track "{ \"branch\": \"main\" }")
    endif()
endforeach()
if(_tracked STREQUAL _plain)
    pkg_fail("the fixture did not add any track key")
endif()

file(WRITE "${W}/plain/ergo-packages.json" "${_plain}")
file(WRITE "${W}/tracked/ergo-packages.json" "${_tracked}")
load_values(plain "${W}/plain/ergo-packages.json" "${W}/plain")
load_values(tracked "${W}/tracked/ergo-packages.json" "${W}/tracked")
pkg_assert_eq("Ergo manifest reader output with track" "${tracked}" "${plain}")
pkg_pass("track on Ergo manifest packages leaves the CMake reader unchanged")

# --- a host declaration: track on host.ergo and on a private package ------------------------------
set(_decl [=[{
  "schema": 1,
  "host": {
    "code": "Example",
    "ergo": { "repository": "https://github.com/LUDIARS/Ergo.git", "rev": "b74f127aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" },
    "use": ["privlib"]
  },
  "packages": {
    "privlib": {
      "kind": "link",
      "code": "Example",
      "repository": "https://example.invalid/Privlib.git",
      "rev": "2222222222222222222222222222222222222222"
    }
  }
}]=])
set(_decl_tracked "${_decl}")
string(JSON _decl_tracked SET "${_decl_tracked}" host ergo track "{ \"branch\": \"develop\" }")
string(JSON _decl_tracked SET "${_decl_tracked}" packages privlib track "{ \"branch\": \"release\" }")
file(WRITE "${W}/host_plain/ergo-packages.json" "${_decl}")
file(WRITE "${W}/host_tracked/ergo-packages.json" "${_decl_tracked}")
load_values(hplain "${W}/host_plain/ergo-packages.json" "${W}/host_plain" HOST)
load_values(htracked "${W}/host_tracked/ergo-packages.json" "${W}/host_tracked" HOST)
pkg_assert_eq("host declaration reader output with track" "${htracked}" "${hplain}")
pkg_pass("track on a host declaration leaves the CMake reader unchanged")
