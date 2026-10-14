# Stage S2 configure tests of the package hub (spec/feature/package-hub.md
# §3 / §4 / §6): every resolution route, drift none / ahead / behind /
# diverged / unknown, STRICT on and off, the host extra manifest (name
# clash), and Emscripten simulated through the EMSCRIPTEN variable (vulkan-
# like gpu rejected, web on by default).
#
# The fake packages (fixtures/alpha, beta, gamma) are copied into the work
# dir and turned into git repositories here; the manifest is generated from
# fixtures/hub/ergo-packages.json.in with the SHAs this script created. Each
# case configures fixtures/hub_host (project NONE: no compiler) and checks
# the exit code, the log, resolved.json and the host-visible facts.
#
#   cmake -DERGO_SOURCE_DIR=<ergo> -DERGO_TEST_WORK_DIR=<dir>
#         [-DGIT_EXECUTABLE=<git>] [-DERGO_TEST_GENERATOR=<gen>]
#         -P tests/packages/test_hub_routes.cmake
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")

if(NOT GIT_EXECUTABLE)
    find_program(GIT_EXECUTABLE git)
endif()
if(NOT GIT_EXECUTABLE)
    pkg_fail("git is required for the hub route tests")
endif()

file(TO_CMAKE_PATH "${ERGO_TEST_WORK_DIR}" W)  # JSON-safe (no backslashes)
set(FIX "${CMAKE_CURRENT_LIST_DIR}/fixtures")
file(REMOVE_RECURSE "${W}")
file(MAKE_DIRECTORY "${W}")
set(_gen_args "")
if(ERGO_TEST_GENERATOR)
    set(_gen_args -G "${ERGO_TEST_GENERATOR}")
endif()

# --- git fixtures -----------------------------------------------------------
function(git dir)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -c user.name=ergo-test
            -c user.email=ergo-test@example.invalid -c commit.gpgsign=false
            -c tag.gpgsign=false -c core.autocrlf=false -c init.defaultBranch=main
            -C "${dir}" ${ARGN}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _rc EQUAL 0)
        pkg_fail("git ${ARGN} in ${dir}: ${_err}")
    endif()
    set(GIT_OUT "${_out}" PARENT_SCOPE)
endfunction()

# Copy fixtures/<src> to <dest> and commit it; HEAD SHA in <out>.
function(make_repo out src dest)
    file(COPY "${FIX}/${src}/" DESTINATION "${dest}")
    git("${dest}" init -q)
    git("${dest}" add -A)
    git("${dest}" commit -q -m "first")
    git("${dest}" rev-parse HEAD)
    set(${out} "${GIT_OUT}" PARENT_SCOPE)
endfunction()

function(file_url out path)
    if(path MATCHES "^/")
        set(${out} "file://${path}" PARENT_SCOPE)
    else()
        set(${out} "file:///${path}" PARENT_SCOPE)
    endif()
endfunction()

set(ALPHA "${W}/repos/Alpha")
make_repo(C1 alpha "${ALPHA}")
git("${ALPHA}" tag v1.0.0)
file(WRITE "${ALPHA}/assets/version.txt" "2\n")
git("${ALPHA}" commit -q -am "second")
git("${ALPHA}" rev-parse HEAD)
set(C2 "${GIT_OUT}")
git("${ALPHA}" tag v9.0.0)   # a tag the pin C1 does not descend from

file(MAKE_DIRECTORY "${W}/clones")
git("${W}/clones" clone -q "${ALPHA}" behind)
git("${W}/clones/behind" checkout -q "${C1}")
git("${W}/clones" clone -q "${ALPHA}" diverged)
git("${W}/clones/diverged" checkout -q -b side "${C1}")
file(WRITE "${W}/clones/diverged/assets/version.txt" "side\n")
git("${W}/clones/diverged" commit -q -am "side")
git("${W}/clones" clone -q "${ALPHA}" dirty)
file(WRITE "${W}/clones/dirty/assets/version.txt" "edited\n")

file(COPY "${FIX}/alpha/" DESTINATION "${W}/plain/Alpha")             # no git
file(COPY "${FIX}/alpha/" DESTINATION "${W}/sib2/.worktrees/Alpha")   # never read

make_repo(B1 beta "${W}/repos/Beta")
make_repo(G1 gamma "${W}/repos/Gamma")

file_url(ALPHA_URL "${ALPHA}")
file_url(BETA_URL "${W}/repos/Beta")
file_url(GAMMA_URL "${W}/repos/Gamma")
set(WIRING_DIR "${FIX}/hub/wiring")
set(BETA_REV "${B1}")
set(GAMMA_REV "${G1}")

# make_manifest(<out file> <alpha rev> [EXPECT_TAG <tag>] [TOOL])
function(make_manifest out alpha_rev)
    cmake_parse_arguments(A "TOOL" "EXPECT_TAG" "" ${ARGN})
    set(ALPHA_REV "${alpha_rev}")
    set(ALPHA_EXPECT_TAG v1.0.0)
    if(A_EXPECT_TAG)
        set(ALPHA_EXPECT_TAG "${A_EXPECT_TAG}")
    endif()
    set(TOOL_ENTRY "")
    if(A_TOOL)
        set(TOOL_ENTRY ",\n    \"delta\": { \"kind\": \"tool\", \"code\": \"Fd\", \"acquire\": \"submodule\", \"path\": \"tools/delta\" }")
    endif()
    configure_file("${FIX}/hub/ergo-packages.json.in" "${out}" @ONLY)
endfunction()
make_manifest("${W}/m/pin-c2.json" "${C2}")
make_manifest("${W}/m/pin-c1.json" "${C1}")
make_manifest("${W}/m/pin-c1-tag9.json" "${C1}" EXPECT_TAG v9.0.0)
make_manifest("${W}/m/pin-c2-tool.json" "${C2}" TOOL)
configure_file("${FIX}/hub/extra-ok.json.in" "${W}/m/extra-ok.json" @ONLY)
configure_file("${FIX}/hub/extra-clash.json.in" "${W}/m/extra-clash.json" @ONLY)

# --- case runner --------------------------------------------------------------
# run_case(<name> <pass|fail> <log regex> <manifest> <cache args...>)
# A list value is written with "^^" for ";" (execute_process would split it)
# and goes through an initial-cache script.
function(run_case name expect pattern manifest)
    set(_build "${W}/cases/${name}")
    set(_args "")
    set(_preload "")
    foreach(_a IN LISTS ARGN)
        if(_a MATCHES "^-D([^=]+)=(.*\\^\\^.*)$")
            string(REPLACE "^^" ";" _v "${CMAKE_MATCH_2}")
            string(APPEND _preload "set(${CMAKE_MATCH_1} \"${_v}\" CACHE STRING \"\")\n")
        else()
            list(APPEND _args "${_a}")
        endif()
    endforeach()
    file(WRITE "${W}/preload/${name}.cmake" "${_preload}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${_gen_args} -S "${FIX}/hub_host" -B "${_build}"
            "-DERGO_SOURCE_DIR=${ERGO_SOURCE_DIR}"
            "-DERGO_PACKAGES_MANIFEST_FILE=${manifest}"
            "-DFETCHCONTENT_BASE_DIR=${W}/f"   # short: Windows path length
            -C "${W}/preload/${name}.cmake" ${_args}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    set(_all "${_out}\n${_err}")
    if(expect STREQUAL "pass")
        if(NOT _rc EQUAL 0 OR NOT EXISTS "${_build}/hub_fixture_ok.txt")
            pkg_fail("${name}: configure should pass\n${_all}")
        endif()
    elseif(_rc EQUAL 0)
        pkg_fail("${name}: configure should fail\n${_all}")
    endif()
    if(NOT _all MATCHES "${pattern}")
        pkg_fail("${name}: output lacks '${pattern}'\n${_all}")
    endif()
    set(CASE_LOG "${_all}" PARENT_SCOPE)
    set(CASE_BUILD "${_build}" PARENT_SCOPE)
endfunction()

# expect_json(<label> <expected> <json path...>) on the last case's report.
function(expect_json label expected)
    file(READ "${CASE_BUILD}/ergo-packages.resolved.json" _json)
    string(JSON _v ERROR_VARIABLE _e GET "${_json}" ${ARGN})
    if(_e)
        pkg_fail("${label}: ${ARGN} missing in resolved.json (${_e})")
    endif()
    pkg_assert_eq("${label}" "${_v}" "${expected}")
endfunction()

# expect_fact(<label> <key> <expected>) on the last case's hub_facts.txt.
function(expect_fact label key expected)
    file(STRINGS "${CASE_BUILD}/hub_facts.txt" _lines)
    foreach(_l IN LISTS _lines)
        if(_l MATCHES "^${key}=(.*)$")
            pkg_assert_eq("${label}" "${CMAKE_MATCH_1}" "${expected}")
            return()
        endif()
    endforeach()
    pkg_fail("${label}: no '${key}' fact")
endfunction()

set(M2 "${W}/m/pin-c2.json")
set(M1 "${W}/m/pin-c1.json")

# --- 1. host route (M1) -------------------------------------------------------
run_case(host_none pass "mode=M1" "${M2}" -DERGO_PACKAGES=alpha
    -DFIXTURE_HOST_ALPHA=before "-DFIXTURE_ALPHA_SRC=${ALPHA}")
expect_json("host route" host packages alpha route)
expect_json("host mode" M1 packages alpha mode)
expect_json("host drift" none packages alpha drift)
expect_json("host state" host-unmanaged packages alpha state)
expect_json("host head" "${C2}" packages alpha head)
expect_fact("host exports" asset_dir "${ALPHA}/assets")
pkg_pass("host route, drift none")

run_case(host_ahead pass "PKG-05\\|warning\\|alpha\\|checkout HEAD [0-9a-f]+ is ahead"
    "${M1}" -DERGO_PACKAGES=alpha -DFIXTURE_HOST_ALPHA=before "-DFIXTURE_ALPHA_SRC=${ALPHA}")
expect_json("ahead drift" ahead packages alpha drift)
run_case(host_ahead_strict fail "PKG-05\\|error\\|alpha" "${M1}" -DERGO_PACKAGES=alpha
    -DERGO_PACKAGES_STRICT=ON -DFIXTURE_HOST_ALPHA=before "-DFIXTURE_ALPHA_SRC=${ALPHA}")
pkg_pass("drift ahead (warning, STRICT error)")

run_case(host_contract_missing fail "PKG-10\\|error\\|alpha/gpu\\|host-provided package violates"
    "${M2}" -DERGO_PACKAGES=alpha -DFIXTURE_HOST_ALPHA=bad)
pkg_pass("PKG-10 host-provided contract missing")

run_case(provided_late pass "mode=M1" "${M2}" -DERGO_PACKAGES=alpha
    -DERGO_PACKAGE_PROVIDED_ALPHA=ON -DFIXTURE_HOST_ALPHA=late "-DFIXTURE_ALPHA_SRC=${ALPHA}")
expect_json("provided route" provided packages alpha route)
expect_json("provided drift" none packages alpha drift)
run_case(provided_missing fail "PKG-10\\|error\\|alpha\\|.*target 'alpha' is missing \\(route provided\\)"
    "${M2}" -DERGO_PACKAGES=alpha -DERGO_PACKAGE_PROVIDED_ALPHA=ON)
pkg_pass("ERGO_PACKAGE_PROVIDED_<NAME> (late check)")

# --- 2. local source (M2) -----------------------------------------------------
run_case(source_none pass "mode=M2" "${M2}" -DERGO_PACKAGES=alpha
    "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${ALPHA}")
expect_json("source route" source packages alpha route)
expect_json("source mode" M2 packages alpha mode)
expect_json("source state" hub-managed packages alpha state)
expect_json("source drift" none packages alpha drift)
expect_json("source features" gpu packages alpha features 0)
expect_fact("source exports" asset_dir "${ALPHA}/assets")
expect_fact("cache default applied" seen "demo=OFF,web=OFF,nogpu=")
expect_fact("targets" targets alpha)
pkg_pass("source route, cache default, exports")

run_case(source_host_cache pass "PKG-15\\|warning\\|ALPHA_BUILD_DEMO" "${M2}"
    -DERGO_PACKAGES=alpha "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${ALPHA}" -DALPHA_BUILD_DEMO=ON)
expect_fact("host value kept (no FORCE)" seen "demo=ON,web=OFF,nogpu=")
pkg_pass("cache default never FORCEd (PKG-15)")

run_case(source_minus_gpu fail "PKG-13\\|error\\|alpha" "${M2}" -DERGO_PACKAGES=alpha
    "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${ALPHA}" "-DERGO_PACKAGE_FEATURES_ALPHA=-gpu^^+metal")
pkg_pass("reserved feature stops before acquisition")

run_case(source_missing fail "PKG-03\\|error\\|alpha\\|.*has no CMakeLists.txt" "${M2}"
    -DERGO_PACKAGES=alpha "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${W}/nowhere"
    "-DERGO_PACKAGES_SIBLING_ROOT=${W}/repos")
pkg_pass("wrong source dir does not fall through")

run_case(source_behind pass "PKG-05\\|warning\\|alpha\\|.* is behind" "${M2}"
    -DERGO_PACKAGES=alpha "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${W}/clones/behind")
expect_json("behind drift" behind packages alpha drift)
run_case(source_diverged pass "PKG-05\\|warning\\|alpha\\|.* is diverged" "${M2}"
    -DERGO_PACKAGES=alpha "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${W}/clones/diverged")
expect_json("diverged drift" diverged packages alpha drift)
run_case(source_unknown pass "PKG-09\\|warning\\|alpha" "${M2}"
    -DERGO_PACKAGES=alpha "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${W}/plain/Alpha")
expect_json("unknown drift" unknown packages alpha drift)
run_case(source_unknown_strict fail "PKG-09\\|error\\|alpha" "${M2}"
    -DERGO_PACKAGES=alpha -DERGO_PACKAGES_STRICT=ON
    "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${W}/plain/Alpha")
pkg_pass("drift behind / diverged / unknown")

run_case(source_dirty pass "mode=M2" "${M2}" -DERGO_PACKAGES=alpha
    "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${W}/clones/dirty")
expect_json("dirty flag" ON packages alpha dirty)
file(READ "${CASE_BUILD}/ergo-packages.resolved.json" _json)
if(NOT _json MATCHES "\"PKG-07\"")
    pkg_fail("dirty: PKG-07 not recorded")
endif()
run_case(source_dirty_strict fail "PKG-07\\|error\\|alpha" "${M2}" -DERGO_PACKAGES=alpha
    -DERGO_PACKAGES_STRICT=ON "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${W}/clones/dirty")
pkg_pass("PKG-07 dirty (record, STRICT error)")

run_case(expect_tag pass "PKG-08\\|warning\\|alpha\\|pin [0-9a-f]+ does not descend from expect_tag v9.0.0"
    "${W}/m/pin-c1-tag9.json" -DERGO_PACKAGES=alpha "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${ALPHA}")
pkg_pass("PKG-08 expect_tag")

# --- 3. sibling checkout ------------------------------------------------------
run_case(sibling pass "mode=M2" "${M2}" -DERGO_PACKAGES=alpha
    "-DERGO_PACKAGES_SIBLING_ROOT=${W}/repos")
expect_json("sibling route" sibling packages alpha route)
run_case(sibling_worktrees fail "PKG-03\\|error\\|alpha\\|.*tried: host, sibling" "${M2}"
    -DERGO_PACKAGES=alpha "-DERGO_PACKAGES_SIBLING_ROOT=${W}/sib2")
pkg_pass("sibling route reads <root>/<Repo> only (.worktrees ignored)")

# --- 4. installed (find_package) ------------------------------------------------
run_case(installed pass "mode=M2" "${M2}" -DERGO_PACKAGES=beta
    "-DBeta_DIR=${FIX}/beta_install")
expect_json("installed route" installed packages beta route)
expect_json("installed version" 0.3.0 packages beta version)
run_case(installed_strict fail "PKG-03\\|error\\|beta\\|.*installed\\(skipped: STRICT\\)" "${M2}"
    -DERGO_PACKAGES=beta -DERGO_PACKAGES_STRICT=ON "-DBeta_DIR=${FIX}/beta_install")
pkg_pass("installed route (version only, unused under STRICT)")

# --- 5. fetch -------------------------------------------------------------------
run_case(fetch_off fail "PKG-03\\|error\\|alpha\\|.*fetch\\(disabled\\)" "${M2}"
    -DERGO_PACKAGES=alpha)
run_case(fetch_ok pass "mode=M2" "${M2}" -DERGO_PACKAGES=alpha
    -DERGO_PACKAGES_FETCH=ON -DERGO_PACKAGES_STRICT=ON)
expect_json("fetch route" fetch packages alpha route)
expect_json("fetch head" "${C2}" packages alpha head)
run_case(fetch_mismatch fail "PKG-04\\|error\\|alpha" "${M2}" -DERGO_PACKAGES=alpha
    -DERGO_PACKAGES_FETCH=ON "-DFETCHCONTENT_SOURCE_DIR_ERGO_PKG_ALPHA=${W}/clones/behind")
pkg_pass("fetch route (only when allowed, HEAD == pin verified)")

# --- extra manifest -------------------------------------------------------------
run_case(extra_ok pass "mode=M2" "${M2}" "-DERGO_PACKAGES=alpha^^gamma"
    "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${ALPHA}"
    "-DERGO_PACKAGE_SOURCE_DIR_GAMMA=${W}/repos/Gamma"
    "-DERGO_PACKAGE_MANIFEST_EXTRA=${W}/m/extra-ok.json")
expect_json("extra package resolved" source packages gamma route)
expect_fact("extra targets" targets "alpha,gamma")
run_case(extra_clash fail "PKG-15\\|error\\|alpha\\|extra manifest reuses" "${M2}"
    -DERGO_PACKAGES=alpha "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${ALPHA}"
    "-DERGO_PACKAGE_MANIFEST_EXTRA=${W}/m/extra-clash.json")
run_case(extra_clash_m0 fail "PKG-15\\|error\\|alpha\\|extra manifest reuses" "${M2}"
    "-DERGO_PACKAGE_MANIFEST_EXTRA=${W}/m/extra-clash.json")
pkg_pass("ERGO_PACKAGE_MANIFEST_EXTRA (same name is a configure error)")

# --- Emscripten (simulated) -------------------------------------------------------
run_case(emscripten_web pass "mode=M2" "${M2}" -DERGO_PACKAGES=alpha -DEMSCRIPTEN=ON
    "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${ALPHA}")
expect_json("emscripten platform" emscripten platform)
expect_json("web auto on" web packages alpha features 0)
expect_fact("web mapped, gpu hidden" seen "demo=OFF,web=ON,nogpu=ON")
expect_fact("only the web target" targets alpha_web)
expect_fact("wiring variables stay in scope" leak "")
run_case(emscripten_gpu fail "PKG-13\\|error\\|alpha/gpu\\|not available on platform 'emscripten'"
    "${M2}" -DERGO_PACKAGES=alpha -DEMSCRIPTEN=ON
    "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${ALPHA}" -DERGO_PACKAGE_FEATURES_ALPHA=+gpu)
pkg_pass("Emscripten: gpu rejected, web on by default")

# --- tool submodule (PKG-16) ------------------------------------------------------
run_case(tool_uninit pass "PKG-16\\|warning\\|delta" "${W}/m/pin-c2-tool.json"
    -DERGO_PACKAGES=alpha "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${ALPHA}")
run_case(tool_uninit_strict fail "PKG-16\\|error\\|delta" "${W}/m/pin-c2-tool.json"
    -DERGO_PACKAGES=alpha -DERGO_PACKAGES_STRICT=ON "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${ALPHA}")
pkg_pass("PKG-16 tool submodule")

# --- all errors listed before stopping -------------------------------------------
run_case(all_listed fail "PKG-03\\|error\\|alpha.*PKG-03\\|error\\|beta|PKG-03\\|error\\|beta.*PKG-03\\|error\\|alpha"
    "${M2}" "-DERGO_PACKAGES=alpha^^beta" -DERGO_PACKAGES_STRICT=ON)
pkg_pass("FATAL lists every error")
