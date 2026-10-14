# ---------------------------------------------------------------------------
# pkg_git_support.cmake — fake git repositories for the tests/packages/ scripts
# (W1 tests; test_hub_routes.cmake keeps its own copy). No network: every
# repository is created inside ERGO_TEST_WORK_DIR and reached by file:// URL.
#
#   pkg_git(<dir> <args...>)              runs git, FATAL on failure, GIT_OUT
#   pkg_make_repo(<out_sha> <src> <dest>) copies <src>/ to <dest>, commits it
#   pkg_file_url(<out> <path>)            file:// URL of a local path
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

if(NOT GIT_EXECUTABLE)
    find_program(GIT_EXECUTABLE git)
endif()
if(NOT GIT_EXECUTABLE)
    message(FATAL_ERROR "FAIL: git is required for this test")
endif()

function(pkg_git dir)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -c user.name=ergo-test
            -c user.email=ergo-test@example.invalid -c commit.gpgsign=false
            -c tag.gpgsign=false -c core.autocrlf=false -c init.defaultBranch=main
            -C "${dir}" ${ARGN}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "FAIL: git ${ARGN} in ${dir}: ${_err}")
    endif()
    set(GIT_OUT "${_out}" PARENT_SCOPE)
endfunction()

function(pkg_make_repo out src dest)
    file(COPY "${src}/" DESTINATION "${dest}")
    pkg_git("${dest}" init -q)
    pkg_git("${dest}" add -A)
    pkg_git("${dest}" commit -q -m "first")
    pkg_git("${dest}" rev-parse HEAD)
    set(${out} "${GIT_OUT}" PARENT_SCOPE)
endfunction()

function(pkg_file_url out path)
    if(path MATCHES "^/")
        set(${out} "file://${path}" PARENT_SCOPE)
    else()
        set(${out} "file:///${path}" PARENT_SCOPE)
    endif()
endfunction()
