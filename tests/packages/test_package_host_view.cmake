# W6: ergo_pkg status → ergo_package_viz --view host end to end
# (spec/feature/package-manager.md §5.1 / §7 W6).
#   * status: --junit is preferred, then <build>/Testing/Temporary/LastTest.log, then "unknown";
#     the host code comes from resolved.json's host.id when the host has no declaration;
#     a --junit that cannot be read is exit 2; no --build is exit 2; no path of the work tree is written
#   * viz --view host: the resolved + report + status fixtures give a TELA_GRAPH 1 file; the same
#     inputs give the same bytes; with every input missing the view is still written ("—")
# Every input is a fixture copied here; no network, no git repository is required.
#
#   cmake -DERGO_SOURCE_DIR=<ergo> -DERGO_TEST_WORK_DIR=<dir> -DERGO_PKG_CLI=<ergo_pkg>
#         -DERGO_PACKAGE_VIZ_CLI=<ergo_package_viz> -P tests/packages/test_package_host_view.cmake
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")

if(NOT ERGO_PKG_CLI OR NOT ERGO_PACKAGE_VIZ_CLI)
    pkg_fail("-DERGO_PKG_CLI=<ergo_pkg> and -DERGO_PACKAGE_VIZ_CLI=<ergo_package_viz> are required")
endif()
file(TO_CMAKE_PATH "${ERGO_TEST_WORK_DIR}" W)
file(REMOVE_RECURSE "${W}")
file(MAKE_DIRECTORY "${W}")

set(_model_fx "${ERGO_SOURCE_DIR}/tests/package_model/fixtures/host_state")
set(_status_fx "${ERGO_SOURCE_DIR}/tests/package_manager/fixtures/status")

# Fake build tree and host checkout (no ergo-packages.json in the host: the code comes from resolved.json).
file(MAKE_DIRECTORY "${W}/build" "${W}/host/.ergo" "${W}/empty")
file(WRITE "${W}/build/CMakeCache.txt" "# fake cache\n")
file(COPY "${_model_fx}/ergo-packages.resolved.json" DESTINATION "${W}/build")
file(COPY "${_model_fx}/watch-report.json" DESTINATION "${W}/host/.ergo")

# run(<expected exit> <exe> <args...>) — both streams in RUN_OUT.
function(run expected exe)
    execute_process(COMMAND "${exe}" ${ARGN}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL expected)
        pkg_fail("${exe} ${ARGN}: exit ${_rc}, expected ${expected}\n${_out}\n${_err}")
    endif()
    set(RUN_OUT "${_out}${_err}" PARENT_SCOPE)
endfunction()

function(expect_in label file needle)
    file(READ "${file}" _text)
    string(FIND "${_text}" "${needle}" _at)
    if(_at EQUAL -1)
        pkg_fail("${label}: '${needle}' not in ${file}:\n${_text}")
    endif()
endfunction()

function(expect_not_in label file needle)
    file(READ "${file}" _text)
    string(FIND "${_text}" "${needle}" _at)
    if(NOT _at EQUAL -1)
        pkg_fail("${label}: '${needle}' must not be in ${file}:\n${_text}")
    endif()
endfunction()

set(_status "${W}/build/ergo-packages.status.json")

# --- status: JUnit -----------------------------------------------------------
run(0 "${ERGO_PKG_CLI}" status --build "${W}/build" --junit "${_status_fx}/ctest-junit.xml"
    --host "${W}/host" --ergo "${ERGO_SOURCE_DIR}")
expect_in("junit source" "${_status}" "\"source\":\"junit\"")
expect_in("junit counts" "${_status}" "\"failed\":1,\"passed\":3,\"skipped\":1,\"source\":\"junit\",\"total\":5")
expect_in("host code from resolved.json" "${_status}" "\"host\":\"Example\"")
expect_in("schema" "${_status}" "\"schema\":1")
file(READ "${_status}" _status_text)
if(NOT _status_text MATCHES "\"configured_at\":\"[0-9]+-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]:[0-9][0-9]:[0-9][0-9]Z\"")
    pkg_fail("configured_at is not the CMakeCache.txt time in UTC: ${_status_text}")
endif()
expect_not_in("no work tree path" "${_status}" "${W}")

# --- status: LastTest.log, then unknown --------------------------------------
file(COPY "${_status_fx}/LastTest.log" DESTINATION "${W}/build/Testing/Temporary")
run(0 "${ERGO_PKG_CLI}" status --build "${W}/build" --host "${W}/host" --ergo "${ERGO_SOURCE_DIR}")
expect_in("last-test-log" "${_status}" "\"failed\":1,\"passed\":2,\"skipped\":0,\"source\":\"last-test-log\",\"total\":3")
file(REMOVE_RECURSE "${W}/build/Testing")
run(0 "${ERGO_PKG_CLI}" status --build "${W}/build" --host "${W}/host" --ergo "${ERGO_SOURCE_DIR}"
    --out "${W}/unknown.status.json")
expect_in("unknown" "${W}/unknown.status.json" "\"tests\":{\"source\":\"unknown\"}")

# --- status: argument errors -------------------------------------------------
run(2 "${ERGO_PKG_CLI}" status --build "${W}/build" --junit "${W}/no-such.xml" --host "${W}/host"
    --ergo "${ERGO_SOURCE_DIR}")
run(2 "${ERGO_PKG_CLI}" status --host "${W}/host" --ergo "${ERGO_SOURCE_DIR}")

# --- viz --view host ---------------------------------------------------------
run(0 "${ERGO_PKG_CLI}" status --build "${W}/build" --junit "${_status_fx}/ctest-junit.xml"
    --host "${W}/host" --ergo "${ERGO_SOURCE_DIR}")
run(0 "${ERGO_PACKAGE_VIZ_CLI}" "${W}/build" --view host --host "${W}/host" --out "${W}/out/a.tela")
expect_in("tela header" "${W}/out/a.tela" "TELA_GRAPH 1")
expect_in("title" "${W}/out/a.tela" "ホスト状態図 Example")
expect_in("review node" "${W}/out/a.tela" "#128 Pf")
expect_in("tests on the host node" "${W}/out/a.tela" "テスト 1 失敗 (5 中)")
run(0 "${ERGO_PACKAGE_VIZ_CLI}" "${W}/build" --view host --host "${W}/host" --out "${W}/out/b.tela")
file(READ "${W}/out/a.tela" _a)
file(READ "${W}/out/b.tela" _b)
if(NOT _a STREQUAL _b)
    pkg_fail("the host view is not deterministic")
endif()

# Every input missing: still drawn, every value "—".
run(0 "${ERGO_PACKAGE_VIZ_CLI}" "${W}/empty" --view host --out "${W}/out/empty.tela")
expect_in("missing inputs" "${W}/out/empty.tela" "HEAD — | モード — | テスト —")

pkg_pass("test_package_host_view")
