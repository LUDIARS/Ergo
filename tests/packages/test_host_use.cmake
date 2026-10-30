cmake_minimum_required(VERSION 3.20)
# Host declaration defaults on a configure (spec/feature/package-manager.md
# §2.1, the Ludellus-Native steps of §5.2): a fake host takes a fake Ergo
# through the generated bootstrap, and its declaration's use / optional /
# features / legacy_vars become the hub's defaults.
#   L1  use empty                 -> mode=M0, Ergo's drift against host.ergo
#   L2  use+optional alpha, the host creates alpha after Ergo (provided)
#                                 -> mode=M1 (alpha=.../provided), PKG-05 and
#                                    PKG-15 (host FORCE) recorded; without a
#                                    source the optional alpha is not used
#   L3  features "-gpu" + legacy alias of the source dir
#                                 -> mode=M2 (alpha=.../source), features from
#                                    the declaration, no PKG-15
#   cache values override every default and resolved.json says so; Ergo's
#   own checkout is recorded in host.ergo (pin / head / route / drift none,
#   ahead, behind, diverged, unknown) with PKG-05 / PKG-09 / PKG-07.
#
# Fake Ergo: fixtures/use_ergo + a copy of Ergo's cmake/ + a manifest made
# from fixtures/hub/ergo-packages.json.in, committed as a git repository.
# Fake package alpha: fixtures/alpha (as in test_hub_routes). No network.
#
#   cmake -DERGO_SOURCE_DIR=<ergo> -DERGO_TEST_WORK_DIR=<dir>
#         [-DGIT_EXECUTABLE=<git>] [-DERGO_TEST_GENERATOR=<gen>]
#         -P tests/packages/test_host_use.cmake
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")

if(NOT GIT_EXECUTABLE)
    find_program(GIT_EXECUTABLE git)
endif()
if(NOT GIT_EXECUTABLE)
    pkg_fail("git is required for the host use tests")
endif()

file(TO_CMAKE_PATH "${ERGO_TEST_WORK_DIR}" W)  # JSON-safe (no backslashes)
set(FIX "${CMAKE_CURRENT_LIST_DIR}/fixtures")
file(REMOVE_RECURSE "${W}")
file(MAKE_DIRECTORY "${W}")
set(_gen_args "")
if(ERGO_TEST_GENERATOR)
    set(_gen_args -G "${ERGO_TEST_GENERATOR}")
endif()

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

function(commit_all out dir message)
    git("${dir}" add -A)
    git("${dir}" commit -q -m "${message}")
    git("${dir}" rev-parse HEAD)
    set(${out} "${GIT_OUT}" PARENT_SCOPE)
endfunction()

function(file_url out path)
    if(path MATCHES "^/")
        set(${out} "file://${path}" PARENT_SCOPE)
    else()
        set(${out} "file:///${path}" PARENT_SCOPE)
    endif()
endfunction()

# --- fake alpha: C1 (tag v1.0.0) <- C2; the host's copy stays at C1 -----------
set(ALPHA "${W}/repos/Alpha")
file(COPY "${FIX}/alpha/" DESTINATION "${ALPHA}")
git("${ALPHA}" init -q)
commit_all(C1 "${ALPHA}" first)
git("${ALPHA}" tag v1.0.0)
file(WRITE "${ALPHA}/assets/version.txt" "2\n")
commit_all(C2 "${ALPHA}" second)
file(MAKE_DIRECTORY "${W}/clones")
git("${W}/clones" clone -q "${ALPHA}" behind)
git("${W}/clones/behind" checkout -q "${C1}")

# --- fake Ergo: E1 <- E2, manifest pins alpha at C2 ---------------------------
set(ERGO "${W}/ws/Ergo")
file(COPY "${FIX}/use_ergo/" DESTINATION "${ERGO}")
file(COPY "${ERGO_SOURCE_DIR}/cmake/" DESTINATION "${ERGO}/cmake")
file_url(ALPHA_URL "${ALPHA}")
set(ALPHA_REV "${C2}")
set(ALPHA_EXPECT_TAG v1.0.0)
set(WIRING_DIR "${FIX}/hub/wiring")
set(BETA_URL "file:///nowhere/Beta")
set(BETA_REV "${C2}")
set(TOOL_ENTRY "")
configure_file("${FIX}/hub/ergo-packages.json.in" "${ERGO}/ergo-packages.json" @ONLY)
git("${ERGO}" init -q)
commit_all(E1 "${ERGO}" first)
file(WRITE "${ERGO}/notes.txt" "second\n")
commit_all(E2 "${ERGO}" second)
# Ergo checkouts for the drift cases: behind (at E1), diverged (a side
# commit on E1) and a plain copy without git.
git("${W}/clones" clone -q "${ERGO}" ergo_behind)
git("${W}/clones/ergo_behind" checkout -q "${E1}")
git("${W}/clones" clone -q "${ERGO}" ergo_diverged)
git("${W}/clones/ergo_diverged" checkout -q -b side "${E1}")
file(WRITE "${W}/clones/ergo_diverged/notes.txt" "side\n")
commit_all(ES "${W}/clones/ergo_diverged" side)
file(COPY "${FIX}/use_ergo/" DESTINATION "${W}/plain/Ergo")
file(COPY "${ERGO_SOURCE_DIR}/cmake/" DESTINATION "${W}/plain/Ergo/cmake")
file(COPY "${ERGO}/ergo-packages.json" DESTINATION "${W}/plain/Ergo")

# --- fake host ----------------------------------------------------------------
set(HOST "${W}/ws/Host")
file(COPY "${FIX}/use_host/" DESTINATION "${HOST}")
set(ERGO_BOOTSTRAP_SOURCE "${E2}")
configure_file("${ERGO_SOURCE_DIR}/cmake/packages/ergo-bootstrap.cmake.in"
    "${HOST}/cmake/ergo-bootstrap.cmake" @ONLY)

# write_decl(<ergo rev> <use json> <optional json> <features json>)
function(write_decl ergo_rev use optional features)
    file(WRITE "${HOST}/ergo-packages.json" "{
  \"schema\": 1,
  \"host\": {
    \"code\": \"Example\",
    \"ergo\": { \"repository\": \"https://github.com/LUDIARS/Ergo.git\", \"rev\": \"${ergo_rev}\" },
    \"use\": ${use},
    \"optional\": ${optional},
    \"features\": ${features},
    \"legacy_vars\": { \"EXAMPLE_ALPHA_DIR\": \"alpha.source_dir\" }
  },
  \"packages\": {}
}
")
endfunction()

# run_case(<name> <pass|fail> <log regex> <cache args...>)
# The Ergo checkout is ${ERGO} unless CASE_ERGO names another one.
set(CASE_ERGO "")
function(run_case name expect pattern)
    set(_build "${W}/b/${name}")
    set(_ergo "${ERGO}")
    if(CASE_ERGO)
        set(_ergo "${CASE_ERGO}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${_gen_args} -S "${HOST}" -B "${_build}"
            "-DERGO_SOURCE_DIR=${_ergo}" "-DFETCHCONTENT_BASE_DIR=${W}/f" ${ARGN}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    set(_all "${_out}\n${_err}")
    if(expect STREQUAL "pass")
        if(NOT _rc EQUAL 0 OR NOT EXISTS "${_build}/use_fixture_ok.txt")
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

function(expect_json label expected)
    file(READ "${CASE_BUILD}/ergo-packages.resolved.json" _json)
    string(JSON _v ERROR_VARIABLE _e GET "${_json}" ${ARGN})
    if(_e)
        pkg_fail("${label}: ${ARGN} missing in resolved.json (${_e})")
    endif()
    pkg_assert_eq("${label}" "${_v}" "${expected}")
endfunction()

# expect_finding(<label> <id> <severity> <subject> [ABSENT])
function(expect_finding label id severity subject)
    file(READ "${CASE_BUILD}/ergo-packages.resolved.json" _json)
    string(JSON _n LENGTH "${_json}" findings)
    set(_hit FALSE)
    if(_n GREATER 0)
        math(EXPR _last "${_n} - 1")
        foreach(_i RANGE ${_last})
            string(JSON _fid GET "${_json}" findings ${_i} id)
            string(JSON _fsev GET "${_json}" findings ${_i} severity)
            string(JSON _fsub GET "${_json}" findings ${_i} subject)
            if(_fid STREQUAL id AND (severity STREQUAL "" OR _fsev STREQUAL severity)
               AND (subject STREQUAL "" OR _fsub STREQUAL subject))
                set(_hit TRUE)
            endif()
        endforeach()
    endif()
    if("ABSENT" IN_LIST ARGN)
        if(_hit)
            pkg_fail("${label}: unexpected ${id} ${subject} finding")
        endif()
    elseif(NOT _hit)
        pkg_fail("${label}: no ${id}/${severity} ${subject} finding\n${_json}")
    endif()
endfunction()

function(expect_fact label key expected)
    file(STRINGS "${CASE_BUILD}/use_facts.txt" _lines)
    foreach(_l IN LISTS _lines)
        if(_l MATCHES "^${key}=(.*)$")
            pkg_assert_eq("${label}" "${CMAKE_MATCH_1}" "${expected}")
            return()
        endif()
    endforeach()
    pkg_fail("${label}: no '${key}' fact")
endfunction()

# --- L1: Ergo pinned, use empty -> M0; Ergo's drift recorded --------------------
write_decl("${E2}" "[]" "[]" "{}")
run_case(l1 pass "mode=M0")
expect_json("L1 selected from the declaration" declaration host selected_from)
expect_json("L1 Ergo route" source host ergo route)
expect_json("L1 Ergo drift" none host ergo drift)
expect_json("L1 Ergo head" "${E2}" host ergo head)
expect_json("L1 Ergo pin" "${E2}" host ergo pin)
expect_finding("L1 no Ergo drift finding" PKG-05 "" ergo ABSENT)
write_decl("${E1}" "[]" "[]" "{}")
run_case(l1_ahead pass "PKG-05\\|warning\\|ergo\\|checkout HEAD [0-9a-f]+ is ahead")
expect_json("Ergo ahead of its pin" ahead host ergo drift)
expect_finding("ahead: PKG-05" PKG-05 warning ergo)
write_decl("${E2}" "[]" "[]" "{}")
set(CASE_ERGO "${W}/clones/ergo_behind")
run_case(l1_behind pass "PKG-05\\|warning\\|ergo\\|checkout HEAD [0-9a-f]+ is behind")
expect_json("Ergo behind its pin" behind host ergo drift)
expect_json("behind: head is the checkout's" "${E1}" host ergo head)
set(CASE_ERGO "${W}/clones/ergo_diverged")
run_case(l1_diverged pass "PKG-05\\|warning\\|ergo\\|checkout HEAD [0-9a-f]+ is diverged")
expect_json("Ergo diverged from its pin" diverged host ergo drift)
expect_json("diverged: head" "${ES}" host ergo head)
set(CASE_ERGO "${W}/plain/Ergo")
run_case(l1_no_git pass "PKG-09\\|warning\\|ergo")
expect_json("Ergo without git" unknown host ergo drift)
expect_finding("no git: PKG-09, not PKG-05" PKG-05 "" ergo ABSENT)
set(CASE_ERGO "")
write_decl("1111111111111111111111111111111111111111" "[]" "[]" "{}")
run_case(l1_unknown pass "PKG-05\\|warning\\|ergo\\|.*pin commit not in the checkout")
expect_json("Ergo pin not in the checkout" unknown host ergo drift)
expect_json("unknown: head still read" "${E2}" host ergo head)
pkg_pass("L1: host.ergo drift none / ahead / behind / diverged / unknown (PKG-05 / PKG-09)")

# --- L2: use/optional alpha, provided by the host after Ergo -> M1 -----------------
write_decl("${E2}" "[\"alpha\"]" "[\"alpha\"]" "{}")
run_case(l2 pass "mode=M1 \\(alpha=[a-z-]+/provided" -DFIXTURE_PROVIDED=ON
    "-DFIXTURE_ALPHA_SRC=${W}/clones/behind")
expect_json("L2 selected from the declaration" declaration host selected_from)
expect_json("L2 selected" alpha selected 0)
expect_json("L2 route" provided packages alpha route)
expect_json("L2 mode" M1 packages alpha mode)
expect_json("L2 drift" behind packages alpha drift)
expect_finding("L2 PKG-05" PKG-05 warning alpha)
expect_finding("L2 PKG-15 (host FORCE)" PKG-15 warning "")
expect_fact("L2 the host's own alpha" targets alpha)
pkg_pass("L2: mode=M1 (alpha=provided), PKG-05 and PKG-15 recorded")

# Provided but never created / no route at all: the optional alpha is not used.
run_case(l2_absent_provided pass "mode=M0" -DFIXTURE_PROVIDED=ON)
expect_json("unused (late)" alpha host unused 0)
expect_finding("unused record (late)" PKG-03 record alpha)
expect_finding("no PKG-03 error (late)" PKG-03 error alpha ABSENT)
run_case(l2_absent pass "mode=M0")
expect_json("unused (no route)" alpha host unused 0)
expect_finding("unused record" PKG-03 record alpha)
expect_finding("no PKG-03 error" PKG-03 error alpha ABSENT)
write_decl("${E2}" "[\"alpha\"]" "[]" "{}")
run_case(l2_required fail "PKG-03\\|error\\|alpha")
run_case(l2_optional_cache pass "mode=M0" -DERGO_PACKAGES_OPTIONAL=alpha)
expect_json("optional from the cache" cache host optional_from)
pkg_pass("optional: not found is not used, required is PKG-03")

# --- L3: delegated to the hub, features from the declaration -> M2 ----------------
write_decl("${E2}" "[\"alpha\"]" "[\"alpha\"]" "{ \"alpha\": \"-gpu\" }")
run_case(l3 pass "mode=M2 \\(alpha=[a-z-]+/source" "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${ALPHA}")
expect_json("L3 route" source packages alpha route)
expect_json("L3 mode" M2 packages alpha mode)
expect_json("L3 features from the declaration" declaration host features_from alpha)
file(READ "${CASE_BUILD}/ergo-packages.resolved.json" _json)
string(JSON _nf LENGTH "${_json}" packages alpha features)
pkg_assert_eq("L3 -gpu applied" "${_nf}" "0")
expect_fact("L3 gpu off in the package" seen "demo=OFF,web=OFF,nogpu=ON")
expect_finding("L3 no PKG-15" PKG-15 "" "" ABSENT)
pkg_pass("L3: mode=M2 (alpha=source), features from host.features, PKG-15 gone")

# The legacy alias stands in for ERGO_PACKAGE_SOURCE_DIR_ALPHA.
run_case(l3_legacy pass "EXAMPLE_ALPHA_DIR is a legacy alias" "-DEXAMPLE_ALPHA_DIR=${ALPHA}")
expect_json("legacy alias route" source packages alpha route)
pkg_pass("host.legacy_vars <name>.source_dir")

# --- cache values override the declaration ---------------------------------------
run_case(override_features pass "mode=M2" "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${ALPHA}"
    -DERGO_PACKAGE_FEATURES_ALPHA=+gpu)
expect_json("features from the cache" cache host features_from alpha)
expect_json("cache feature request" gpu packages alpha features 0)
write_decl("${E2}" "[]" "[]" "{}")
run_case(override_use pass "mode=M2" -DERGO_PACKAGES=alpha "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${ALPHA}")
expect_json("selection from the cache" cache host selected_from)
pkg_pass("cache values override the declaration and are recorded")

# --- Ergo's own checkout: dirty (PKG-07), STRICT stops on drift ---------------------
write_decl("${E1}" "[]" "[]" "{}")
run_case(ergo_strict fail "PKG-05\\|error\\|ergo" -DERGO_PACKAGES=alpha
    -DERGO_PACKAGES_STRICT=ON "-DERGO_PACKAGE_SOURCE_DIR_ALPHA=${ALPHA}")
write_decl("${E2}" "[]" "[]" "{}")
file(WRITE "${ERGO}/notes.txt" "edited\n")
run_case(ergo_dirty pass "mode=M0")
expect_json("Ergo dirty" ON host ergo dirty)
expect_finding("Ergo PKG-07" PKG-07 record ergo)
pkg_pass("Ergo itself: PKG-07 dirty, PKG-05 under STRICT")
