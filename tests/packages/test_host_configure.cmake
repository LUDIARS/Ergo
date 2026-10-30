# W1 configure tests of the host declaration (spec/feature/package-manager.md
# §2.1 / §7). Configures a copy of fixtures/decl_host (project NONE) in M0
# against Ergo's own manifest and checks:
#   * <host>/ergo-packages.json with a `host` section is the host declaration:
#     resolved.json host.id = host.code, manifest_extra names it
#   * host.id falls back to ERGO_PACKAGES_HOST_ID, then CMAKE_PROJECT_NAME
#   * a private package named like an Ergo manifest entry stops the
#     configure with PKG-15 (M0 included)
#   * a host declaration given as the Ergo manifest is a configure error
#   * ERGO_PACKAGE_MANIFEST_EXTRA may point at a host declaration elsewhere
#   * a host-root ergo-packages.json without `host` is not read
#
#   cmake -DERGO_SOURCE_DIR=<ergo> -DERGO_TEST_WORK_DIR=<dir>
#         [-DERGO_TEST_GENERATOR=<gen>] -P tests/packages/test_host_configure.cmake
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")

file(TO_CMAKE_PATH "${ERGO_TEST_WORK_DIR}" W)
file(REMOVE_RECURSE "${W}")
file(MAKE_DIRECTORY "${W}")
set(_gen_args "")
if(ERGO_TEST_GENERATOR)
    set(_gen_args -G "${ERGO_TEST_GENERATOR}")
endif()
set(SHA 1111111111111111111111111111111111111111)

# write_decl(<file> <code> [<private package name>])
function(write_decl file code)
    set(_pkgs "")
    if(ARGC GREATER 2)
        set(_pkgs "\"${ARGV2}\": { \"kind\": \"link\", \"repository\": \"https://example.invalid/P.git\", \"rev\": \"${SHA}\" }")
    endif()
    file(WRITE "${file}" "{
  \"schema\": 1,
  \"host\": {
    \"code\": \"${code}\",
    \"ergo\": { \"repository\": \"https://github.com/LUDIARS/Ergo.git\", \"rev\": \"${SHA}\" },
    \"use\": [], \"optional\": [], \"features\": {}, \"legacy_vars\": {}
  },
  \"packages\": { ${_pkgs} }
}
")
endfunction()

# run_case(<name> <pass|fail> <log regex> <declaration file or ""> <cache args...>)
function(run_case name expect pattern decl)
    set(_host "${W}/${name}/host")
    set(_build "${W}/${name}/build")
    file(COPY "${CMAKE_CURRENT_LIST_DIR}/fixtures/decl_host/" DESTINATION "${_host}")
    if(decl)
        configure_file("${decl}" "${_host}/ergo-packages.json" COPYONLY)  # file(COPY_FILE) needs CMake 3.21
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${_gen_args} -S "${_host}" -B "${_build}"
            "-DERGO_SOURCE_DIR=${ERGO_SOURCE_DIR}" ${ARGN}
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
    set(CASE_HOST "${_host}" PARENT_SCOPE)
endfunction()

function(expect_json label expected)
    file(READ "${CASE_BUILD}/ergo-packages.resolved.json" _json)
    string(JSON _v ERROR_VARIABLE _e GET "${_json}" ${ARGN})
    if(_e)
        pkg_fail("${label}: ${ARGN} missing in resolved.json (${_e})")
    endif()
    pkg_assert_eq("${label}" "${_v}" "${expected}")
endfunction()

write_decl("${W}/decl-example.json" Example)
write_decl("${W}/decl-other.json" Other)
write_decl("${W}/decl-clash.json" Example pictor)
write_decl("${W}/decl-private.json" Example privlib)
file(WRITE "${W}/not-host.json" "{ \"schema\": 1, \"packages\": {} }\n")

# host.code -> host.id; the declaration is the host extra manifest.
run_case(code pass "mode=M0" "${W}/decl-example.json")
expect_json("host id from host.code" Example host id)
file(READ "${CASE_BUILD}/ergo-packages.resolved.json" _json)
string(JSON _extra GET "${_json}" manifest_extra)
if(NOT _extra MATCHES "/code/host/ergo-packages[.]json$")
    pkg_fail("declaration recorded: manifest_extra is '${_extra}'")
endif()
pkg_pass("host.id from host.code")

# Private packages of the declaration join the manifest (M0 records them).
run_case(private pass "mode=M0" "${W}/decl-private.json")
expect_json("private package kind" link packages privlib kind)
pkg_pass("private packages from the host declaration")

# Fallbacks.
run_case(host_id_var pass "mode=M0" "" -DERGO_PACKAGES_HOST_ID=Custom)
expect_json("host id from ERGO_PACKAGES_HOST_ID" Custom host id)
run_case(project_name pass "mode=M0" "")
expect_json("host id from the project name" ergo_pkg_decl_host host id)
run_case(not_host pass "mode=M0" "${W}/not-host.json")
expect_json("a manifest without host is not read" ergo_pkg_decl_host host id)
pkg_pass("host.id fallbacks (ERGO_PACKAGES_HOST_ID, CMAKE_PROJECT_NAME)")

# PKG-15: a private package reuses a public name -> configure error, even M0.
run_case(clash fail "PKG-15\\|error\\|pictor" "${W}/decl-clash.json")
pkg_pass("PKG-15 for a private name of the Ergo manifest")

# A host declaration read as the Ergo manifest is a configure error.
run_case(as_manifest fail "host declaration, not an Ergo manifest" ""
    "-DERGO_PACKAGES_MANIFEST_FILE=${W}/decl-example.json")
pkg_pass("host declaration as the Ergo manifest is rejected")

# ERGO_PACKAGE_MANIFEST_EXTRA pointing at a host declaration elsewhere wins.
run_case(extra pass "mode=M0" "${W}/decl-example.json"
    "-DERGO_PACKAGE_MANIFEST_EXTRA=${W}/decl-other.json")
expect_json("host id from the extra declaration" Other host id)
pkg_pass("ERGO_PACKAGE_MANIFEST_EXTRA reads a host declaration")
