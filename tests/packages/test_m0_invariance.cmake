# Acceptance C-5 (configure-level): with ERGO_PACKAGES unset the hub leaves
# the host's target set and compile definitions untouched and only records.
# Also exercises the selected mode end to end on the host route (M1):
# PKG-01 / PKG-13 / PKG-14 stop the configure, a valid selection passes with
# the host-provided pictor checked against its pin.
#
# Configures tests/packages/fixtures/m0_host (project NONE: no compiler).
# For the whole-tree comparison against a baseline checkout see
# tests/packages/compare_m0_build.cmake.
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")

set(_fixture "${CMAKE_CURRENT_LIST_DIR}/fixtures/m0_host")
set(_gen_args "")
if(ERGO_TEST_GENERATOR)
    set(_gen_args -G "${ERGO_TEST_GENERATOR}")
endif()

# run_case(<name> <expect: pass|fail> <must-contain regex> <cache args...>)
function(run_case name expect pattern)
    set(_build "${ERGO_TEST_WORK_DIR}/${name}")
    file(REMOVE_RECURSE "${_build}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${_gen_args} -S "${_fixture}" -B "${_build}"
            "-DERGO_SOURCE_DIR=${ERGO_SOURCE_DIR}" ${ARGN}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    set(_all "${_out}\n${_err}")
    if(expect STREQUAL "pass")
        if(NOT _rc EQUAL 0 OR NOT EXISTS "${_build}/m0_fixture_ok.txt")
            pkg_fail("${name}: configure should pass\n${_all}")
        endif()
    elseif(_rc EQUAL 0)
        pkg_fail("${name}: configure should fail\n${_all}")
    endif()
    if(NOT _all MATCHES "${pattern}")
        pkg_fail("${name}: output lacks '${pattern}'\n${_all}")
    endif()
    pkg_pass("${name}")
endfunction()

# M0: unchanged targets and definitions, everything recorded, never fails —
# even though ergo_render has no real path (PKG-14 is only recorded).
run_case(m0 pass "ergo packages: mode=M0 \\(pictor=host-unmanaged, tela=absent\\)"
    -DFIXTURE_EXPECT_MODE=M0)

# Selected mode, unknown name: PKG-01 stops the configure.
run_case(selected_unknown fail "PKG-01\\|error\\|picter"
    -DERGO_PACKAGES=picter -DFIXTURE_EXPECT_MODE=selected)

# Selected mode, valid selection with a real path: the host-created pictor
# is taken through the host route (M1) and passes.
run_case(selected_ok pass "ergo packages: mode=M1 [(]pictor=host-unmanaged/host"
    -DERGO_PACKAGES=pictor -DFIXTURE_REAL_PATH=ON -DFIXTURE_EXPECT_MODE=M1)

# Selected mode, real path missing: PKG-14 is an error outside M0.
run_case(selected_no_real_path fail "PKG-14\\|error\\|ergo_render"
    -DERGO_PACKAGES=pictor -DFIXTURE_REAL_PATH=OFF -DFIXTURE_EXPECT_MODE=selected)

# Selected mode, reserved feature requested: PKG-13.
run_case(selected_reserved fail "PKG-13\\|error\\|pictor/webgpu"
    -DERGO_PACKAGES=pictor -DFIXTURE_REAL_PATH=ON "-DERGO_PACKAGE_FEATURES_PICTOR=+webgpu"
    -DFIXTURE_EXPECT_MODE=selected)

# Selected mode, pin override: warning (PKG-06), STRICT turns it into an error.
run_case(selected_override pass "PKG-06\\|warning\\|pictor"
    -DERGO_PACKAGES=pictor -DFIXTURE_REAL_PATH=ON
    -DERGO_PACKAGE_REV_PICTOR=4142f77c0775e1c3ad93733526085c56523ea9dc
    -DFIXTURE_EXPECT_MODE=M1)
run_case(selected_override_strict fail "PKG-06\\|error\\|pictor"
    -DERGO_PACKAGES=pictor -DFIXTURE_REAL_PATH=ON -DERGO_PACKAGES_STRICT=ON
    -DERGO_PACKAGE_REV_PICTOR=4142f77c0775e1c3ad93733526085c56523ea9dc
    -DFIXTURE_EXPECT_MODE=selected)
