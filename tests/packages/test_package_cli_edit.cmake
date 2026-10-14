# W2: ergo_pkg add / remove / update end to end (spec/feature/package-manager.md §2.2 / §7 W2).
#   * update --to <tag> writes rev and expect_tag only; every other byte stays
#   * a shared package is written in the --ergo manifest, ergo and private ones
#     in the host; the mix-up (shared without --ergo, host-owned without a host
#     declaration) exits 2
#   * a feature branch / a SHA off main is refused, --allow-off-main lets it in
#   * --to main warns that STRICT will not pass
#   * remove refuses to break modules.*.needs
#   * add --kind link without the wiring file exits 2 and leaves the file alone
# Repositories are fake git repos reached by file:// URL; no network.
#
#   cmake -DERGO_SOURCE_DIR=<ergo> -DERGO_TEST_WORK_DIR=<dir> -DERGO_PKG_CLI=<ergo_pkg>
#         [-DGIT_EXECUTABLE=<git>] -P tests/packages/test_package_cli_edit.cmake
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/pkg_git_support.cmake")

if(NOT ERGO_PKG_CLI)
    pkg_fail("-DERGO_PKG_CLI=<path of ergo_pkg> is required")
endif()
file(TO_CMAKE_PATH "${ERGO_TEST_WORK_DIR}" W)
file(REMOVE_RECURSE "${W}")
file(MAKE_DIRECTORY "${W}")
set(CACHE_DIR "${W}/cache")

# commit_file(<repo> <file> <text>) — one more commit, SHA in GIT_OUT.
function(commit_file repo file text)
    file(WRITE "${repo}/${file}" "${text}")
    pkg_git("${repo}" add -A)
    pkg_git("${repo}" commit -q -m "${file}")
    pkg_git("${repo}" rev-parse HEAD)
    set(GIT_OUT "${GIT_OUT}" PARENT_SCOPE)
endfunction()

# Fake Pictor: v1.0.0 on main, then main moves on; feat/x leaves from v1.0.0.
pkg_make_repo(P_ROOT "${CMAKE_CURRENT_LIST_DIR}/fixtures/alpha" "${W}/repos/Pictor")
commit_file("${W}/repos/Pictor" tagged.txt "tagged\n")
set(P_TAG "${GIT_OUT}")
pkg_git("${W}/repos/Pictor" tag -a v1.0.0 -m v1.0.0)
pkg_git("${W}/repos/Pictor" checkout -q -b feat/x)
commit_file("${W}/repos/Pictor" feature.txt "feature\n")
set(P_FEATURE "${GIT_OUT}")
pkg_git("${W}/repos/Pictor" checkout -q main)
commit_file("${W}/repos/Pictor" main.txt "main\n")
set(P_MAIN "${GIT_OUT}")
pkg_file_url(P_URL "${W}/repos/Pictor")

# Fake Ergo checkout whose manifest takes Pictor from the fake repository.
file(READ "${ERGO_SOURCE_DIR}/ergo-packages.json" _manifest)
string(REPLACE "https://github.com/LUDIARS/Pictor.git" "${P_URL}" _manifest "${_manifest}")
file(WRITE "${W}/ergo/ergo-packages.json" "${_manifest}")
file(COPY "${ERGO_SOURCE_DIR}/cmake/packages" DESTINATION "${W}/ergo/cmake")
pkg_git("${W}/ergo" init -q)
pkg_git("${W}/ergo" add -A)
pkg_git("${W}/ergo" commit -q -m "fake ergo")
commit_file("${W}/ergo" next.txt "next\n")
set(ERGO_TAGGED "${GIT_OUT}")
pkg_git("${W}/ergo" tag -a v0.1.0 -m v0.1.0)
pkg_file_url(ERGO_URL "${W}/ergo")

# pkg(<expected exit code> <args...>) — with --ergo; pkg_env(...) finds Ergo by ERGO_SOURCE_DIR only.
function(pkg expected)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env --unset=ERGO_DEPENDENCY_CACHE_DIR --unset=ERGO_PKG_CACHE_DIR
            --unset=ERGO_SOURCE_DIR "${ERGO_PKG_CLI}" ${ARGN} --ergo "${W}/ergo"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL expected)
        pkg_fail("ergo_pkg ${ARGN}: exit ${_rc}, expected ${expected}\n${_out}\n${_err}")
    endif()
    set(PKG_OUT "${_out}${_err}" PARENT_SCOPE)
endfunction()
function(pkg_env expected)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env --unset=ERGO_DEPENDENCY_CACHE_DIR --unset=ERGO_PKG_CACHE_DIR
            "ERGO_SOURCE_DIR=${W}/ergo" "${ERGO_PKG_CLI}" ${ARGN}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL expected)
        pkg_fail("ergo_pkg ${ARGN}: exit ${_rc}, expected ${expected}\n${_out}\n${_err}")
    endif()
    set(PKG_OUT "${_out}${_err}" PARENT_SCOPE)
endfunction()

# Host: init, then point host.ergo at the fake Ergo.
set(H "${W}/host")
pkg(0 init --code Example --host "${H}")
file(READ "${H}/ergo-packages.json" _decl)
string(REPLACE "https://github.com/LUDIARS/Ergo.git" "${ERGO_URL}" _decl "${_decl}")
file(WRITE "${H}/ergo-packages.json" "${_decl}")

# --- add (shared) ---------------------------------------------------------------
pkg(0 add pictor --optional --features +vulkan --host "${H}")
file(READ "${H}/ergo-packages.json" _decl)
string(JSON _use GET "${_decl}" host use 0)
pkg_assert_eq("add puts pictor in host.use" "${_use}" pictor)
string(JSON _feat GET "${_decl}" host features pictor)
pkg_assert_eq("add writes the feature request" "${_feat}" "+vulkan")
pkg(2 add pictor --repository "${P_URL}" --host "${H}")   # the Ergo manifest owns pictor's pin
pkg_pass("add: shared package goes to host.use")

# --- update: owner ---------------------------------------------------------------
file(SHA256 "${H}/ergo-packages.json" _host_before)
file(READ "${W}/ergo/ergo-packages.json" _ergo_before)
pkg_env(2 update pictor --to v1.0.0 --cache "${CACHE_DIR}" --host "${H}")   # shared needs --ergo
pkg(0 update pictor --to v1.0.0 --cache "${CACHE_DIR}" --host "${H}")
file(READ "${W}/ergo/ergo-packages.json" _ergo_after)
string(JSON _old_rev GET "${_ergo_before}" packages pictor rev)
string(JSON _old_tag GET "${_ergo_before}" packages pictor expect_tag)
string(REPLACE "\"rev\": \"${_old_rev}\"" "\"rev\": \"${P_TAG}\"" _want "${_ergo_before}")
string(REPLACE "\"expect_tag\": \"${_old_tag}\"" "\"expect_tag\": \"v1.0.0\"" _want "${_want}")
pkg_assert_eq("update --to <tag> writes rev and expect_tag only" "${_ergo_after}" "${_want}")
file(SHA256 "${H}/ergo-packages.json" _host_after)
pkg_assert_eq("a shared update leaves the host alone" "${_host_after}" "${_host_before}")
pkg(2 update ergo --to v0.1.0 --cache "${CACHE_DIR}" --host "${W}/ergo")    # not a host declaration
pkg(2 update nosuch --to v1.0.0 --cache "${CACHE_DIR}" --host "${H}")
pkg_pass("update: shared in the --ergo manifest, mix-ups exit 2")

# --- update: off-main and STRICT ---------------------------------------------------
pkg(2 update pictor --to feat/x --cache "${CACHE_DIR}" --host "${H}")
pkg(2 update pictor --to ${P_FEATURE} --cache "${CACHE_DIR}" --host "${H}")
file(READ "${W}/ergo/ergo-packages.json" _still)
pkg_assert_eq("refused off-main leaves the manifest" "${_still}" "${_ergo_after}")
pkg(0 update pictor --to feat/x --allow-off-main --cache "${CACHE_DIR}" --host "${H}")
file(READ "${W}/ergo/ergo-packages.json" _off)
string(JSON _rev GET "${_off}" packages pictor rev)
pkg_assert_eq("--allow-off-main pins the feature head" "${_rev}" "${P_FEATURE}")
pkg(0 update pictor --to main --cache "${CACHE_DIR}" --host "${H}")
if(NOT PKG_OUT MATCHES "STRICT")
    pkg_fail("--to main should warn about STRICT\n${PKG_OUT}")
endif()
file(READ "${W}/ergo/ergo-packages.json" _main)
string(JSON _rev GET "${_main}" packages pictor rev)
pkg_assert_eq("--to main pins the main head" "${_rev}" "${P_MAIN}")
string(JSON _tag ERROR_VARIABLE _no_tag GET "${_main}" packages pictor expect_tag)
if(NOT _no_tag)
    pkg_fail("--to main must drop expect_tag (got ${_tag})")
endif()
pkg_pass("update: off-main refused / allowed, --to main warns about STRICT")

# --- update ergo (host) --------------------------------------------------------------
file(READ "${H}/ergo-packages.json" _decl_before)
pkg(0 update ergo --to v0.1.0 --cache "${CACHE_DIR}" --host "${H}")
file(READ "${H}/ergo-packages.json" _decl_after)
string(JSON _old_rev GET "${_decl_before}" host ergo rev)
string(REPLACE "\"rev\": \"${_old_rev}\"" "\"rev\": \"${ERGO_TAGGED}\", \"expect_tag\": \"v0.1.0\""
    _want "${_decl_before}")
pkg_assert_eq("update ergo writes host.ergo only" "${_decl_after}" "${_want}")
pkg_pass("update: ergo is pinned in the host")

# --- remove ---------------------------------------------------------------------------
file(SHA256 "${H}/ergo-packages.json" _before)
pkg(2 remove pictor --host "${H}")    # ergo_render needs pictor
file(SHA256 "${H}/ergo-packages.json" _after)
pkg_assert_eq("refused remove leaves the file" "${_after}" "${_before}")
pkg(0 remove tela --host "${H}")      # not declared: nothing to change
pkg_pass("remove: refuses to break modules.*.needs")

# --- add (private link) ------------------------------------------------------------------
pkg_make_repo(N_SHA "${CMAKE_CURRENT_LIST_DIR}/fixtures/alpha" "${W}/repos/Newlib")
pkg_git("${W}/repos/Newlib" tag -a v2.0.0 -m v2.0.0)
pkg_file_url(N_URL "${W}/repos/Newlib")
file(SHA256 "${H}/ergo-packages.json" _before)
pkg(2 add newlib --kind link --repository "${N_URL}" --rev ${N_SHA} --host "${H}")
file(SHA256 "${H}/ergo-packages.json" _after)
pkg_assert_eq("add without the wiring file leaves the file" "${_after}" "${_before}")
file(WRITE "${H}/cmake/packages/newlib.cmake" "# wiring of newlib\n")
pkg(0 add newlib --kind link --repository "${N_URL}" --rev v2.0.0 --cache "${CACHE_DIR}" --host "${H}")
file(READ "${H}/ergo-packages.json" _decl)
string(JSON _rev GET "${_decl}" packages newlib rev)
pkg_assert_eq("private add resolves the tag" "${_rev}" "${N_SHA}")
string(JSON _wiring GET "${_decl}" packages newlib cmake wiring)
pkg_assert_eq("private add names the wiring" "${_wiring}" "cmake/packages/newlib.cmake")
pkg(0 update newlib --to v2.0.0 --cache "${CACHE_DIR}" --host "${H}")   # host-owned: nothing to change
if(NOT PKG_OUT MATCHES "nothing to change")
    pkg_fail("updating to the same pin should change nothing\n${PKG_OUT}")
endif()
pkg(0 remove newlib --host "${H}")
file(READ "${H}/ergo-packages.json" _decl)
string(JSON _pkgs LENGTH "${_decl}" packages)
pkg_assert_eq("remove drops the private package" "${_pkgs}" "0")
pkg_pass("add: private link needs its wiring file, remove drops it")
