# ---------------------------------------------------------------------------
# ErgoPackageFetchCache.cmake — where route 5 (fetch) keeps a package source
#
# One responsibility: name the FetchContent location of a link package so the
# configure-time fetch route (ErgoPackageResolve.cmake) and the ahead-of-time
# prefetch (ErgoPackagePrefetch.cmake, `ergo_pkg fetch`) agree on it. Neither
# side has its own download engine: both go through FetchContent.
#
#   ergo_pkg_fetch_dep_name(<out_dep> <name>)
#       FetchContent name of package <name> ("ergo_pkg_<name>", '-' -> '_').
#   ergo_pkg_fetch_source_dir(<out_dir> <base_dir> <name>)
#       <base_dir>/<lowercase dep>-src, FetchContent's SOURCE_DIR default.
#   ergo_pkg_fetch_prefetched(<out_dir> <base_dir> <name> <pin>)
#       That directory when it is a git checkout whose HEAD is <pin>, else "".
#
# Works in script mode (cmake -P) as well as during a configure.
# ---------------------------------------------------------------------------
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/ErgoPackageGit.cmake")

function(ergo_pkg_fetch_dep_name out_dep name)
    set(_dep "ergo_pkg_${name}")
    string(REPLACE "-" "_" _dep "${_dep}")
    set(${out_dep} "${_dep}" PARENT_SCOPE)
endfunction()

function(ergo_pkg_fetch_source_dir out_dir base_dir name)
    ergo_pkg_fetch_dep_name(_dep "${name}")
    string(TOLOWER "${_dep}" _lc)
    set(${out_dir} "${base_dir}/${_lc}-src" PARENT_SCOPE)
endfunction()

function(ergo_pkg_fetch_prefetched out_dir base_dir name pin)
    set(_hit "")
    ergo_pkg_fetch_source_dir(_dir "${base_dir}" "${name}")
    if(NOT "${pin}" STREQUAL "" AND EXISTS "${_dir}/.git")
        _ergo_pkg_git(_rc _head "${_dir}" rev-parse HEAD)
        if(_rc EQUAL 0 AND _head STREQUAL pin)
            set(_hit "${_dir}")
        endif()
    endif()
    set(${out_dir} "${_hit}" PARENT_SCOPE)
endfunction()
