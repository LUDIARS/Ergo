# ---------------------------------------------------------------------------
# ErgoPackagePrefetch.cmake — fetch link packages ahead of the configure
#
# Script mode only. `ergo_pkg fetch` (tools/package_cli/) starts it so that a
# later configure with ERGO_PACKAGES_FETCH=ON hits route 5 without network
# (spec/feature/package-manager.md §2.2). The source lands where the
# configure-time fetch route looks (ErgoPackageFetchCache.cmake:
# <FETCHCONTENT_BASE_DIR>/<dep>-src) and is fetched by FetchContent, the
# same engine as route 5 — this script only drives it.
#
#   cmake -DERGO_PKG_PREFETCH_NAMES=<a,b,...>
#         -DFETCHCONTENT_BASE_DIR=<dir>          (<build>/_deps or
#                                                  ERGO_DEPENDENCY_CACHE_DIR)
#         [-DERGO_PKG_PREFETCH_MANIFEST=<ergo-packages.json of Ergo>]
#         [-DERGO_PKG_PREFETCH_HOST=<host declaration>]   (private packages)
#         -P cmake/packages/ErgoPackagePrefetch.cmake
#
# Each name must be a link package with a repository and a 40-digit pin
# (PKG-02). A source already at its pin is left alone. After the fetch the
# checkout HEAD must equal the pin (PKG-04). Any failure stops with
# FATAL_ERROR (the CLI maps it to exit code 3); nothing is half-written into
# the source location of another package.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0168)
    cmake_policy(SET CMP0168 NEW)  # populate directly, no generator needed for a sub-build
endif()

include("${CMAKE_CURRENT_LIST_DIR}/ErgoPackageManifest.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ErgoPackageFetchCache.cmake")
include(FetchContent)

if(NOT DEFINED ERGO_PKG_PREFETCH_NAMES OR "${ERGO_PKG_PREFETCH_NAMES}" STREQUAL "")
    message(FATAL_ERROR "ergo prefetch: -DERGO_PKG_PREFETCH_NAMES=<a,b,...> is required")
endif()
if("${FETCHCONTENT_BASE_DIR}" STREQUAL "")
    message(FATAL_ERROR "ergo prefetch: -DFETCHCONTENT_BASE_DIR=<dir> is required")
endif()
if(NOT DEFINED ERGO_PKG_PREFETCH_MANIFEST)
    get_filename_component(ERGO_PKG_PREFETCH_MANIFEST
        "${CMAKE_CURRENT_LIST_DIR}/../../ergo-packages.json" ABSOLUTE)
endif()
get_filename_component(_base "${FETCHCONTENT_BASE_DIR}" ABSOLUTE)

ergo_pkg_manifest_load(_m "${ERGO_PKG_PREFETCH_MANIFEST}")
if(NOT "${ERGO_PKG_PREFETCH_HOST}" STREQUAL "")
    ergo_pkg_manifest_load(_m_host "${ERGO_PKG_PREFETCH_HOST}" HOST)
    ergo_pkg_manifest_merge(_m _m_host _clash)
    ergo_pkg_findings_with_severity(_clash error ${_clash})
    if(_clash)
        string(REPLACE ";" "\n  " _list "${_clash}")
        message(FATAL_ERROR "ergo prefetch: host declaration rejected:\n  ${_list}")
    endif()
endif()

string(REPLACE "," ";" _names "${ERGO_PKG_PREFETCH_NAMES}")
foreach(_n IN LISTS _names)
    if(NOT _n IN_LIST _m_LINK)
        message(FATAL_ERROR "ergo prefetch: '${_n}' is not a link package of the manifest")
    endif()
    set(_repo "${_m_PKG_${_n}_REPOSITORY}")
    set(_pin "${_m_PKG_${_n}_REV}")
    ergo_pkg_rule_rev_format(_f PACKAGE "${_n}" REV "${_pin}")
    if(_f OR _repo STREQUAL "")
        message(FATAL_ERROR "ergo prefetch: '${_n}' needs a repository and a 40-digit rev (${_f})")
    endif()
endforeach()

foreach(_n IN LISTS _names)
    set(_repo "${_m_PKG_${_n}_REPOSITORY}")
    set(_pin "${_m_PKG_${_n}_REV}")
    ergo_pkg_fetch_prefetched(_have "${_base}" "${_n}" "${_pin}")
    if(NOT _have STREQUAL "")
        message(STATUS "ergo prefetch: ${_n} already at ${_pin}")
        continue()
    endif()
    ergo_pkg_fetch_dep_name(_dep "${_n}")
    string(TOLOWER "${_dep}" _lc)
    ergo_pkg_fetch_source_dir(_src "${_base}" "${_n}")
    # The sub-build is scratch: the configure later uses the source directly
    # (route 5 with a source at the pin), so its generator does not matter.
    set(_scratch "${_base}/_ergo_prefetch/${_lc}")
    file(REMOVE_RECURSE "${_scratch}")
    message(STATUS "ergo prefetch: fetching ${_n} at ${_pin}")
    # Same content options as _ergo_fc_declare (cmake/ErgoDependencies.cmake).
    FetchContent_Populate(${_dep} QUIET
        SOURCE_DIR   "${_src}"
        BINARY_DIR   "${_scratch}/build"
        SUBBUILD_DIR "${_scratch}/subbuild"
        GIT_REPOSITORY "${_repo}"
        GIT_TAG        "${_pin}"
        GIT_SHALLOW    TRUE)
    file(REMOVE_RECURSE "${_scratch}")
    ergo_pkg_fetch_prefetched(_have "${_base}" "${_n}" "${_pin}")
    if(_have STREQUAL "")
        message(FATAL_ERROR "ergo prefetch: PKG-04 ${_n}: fetched HEAD is not the pin ${_pin}")
    endif()
    message(STATUS "ergo prefetch: ${_n} -> ${_src}")
endforeach()
