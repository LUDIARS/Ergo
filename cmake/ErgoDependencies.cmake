# ---------------------------------------------------------------------------
# ErgoDependencies.cmake — third-party library acquisition engine
#
# Generic, library-agnostic plumbing for fetching and wiring external C/C++
# libraries (kazmath / curl / ...) from pinned upstream sources. This file owns
# *only* the mechanism (fetch + verify + bookkeeping); per-library integration
# lives in cmake/deps/<name>.cmake, and the registry that decides which deps to
# build lives in third_party/dependencies.cmake.
#
# Design rules (Ergo coding conventions):
#   - No silent fallback. If a dependency is requested but cannot be provided
#     (fetch disabled, no source override, missing expected target), this fails
#     loudly with FATAL_ERROR rather than degrading to a partial build.
#   - Reproducible. Every dependency is pinned to an exact upstream commit/tag.
#   - SRP. This engine never names a concrete library; it only fetches/verifies.
# ---------------------------------------------------------------------------
include_guard(GLOBAL)
include(FetchContent)

option(ERGO_FETCH_DEPENDENCIES
    "Allow Ergo to fetch managed third-party libraries from their pinned upstream sources" ON)

# Shared download/build cache for fetched sources. Empty => CMake default
# (<build>/_deps). Point several build trees at one dir to avoid re-downloading.
set(ERGO_DEPENDENCY_CACHE_DIR "" CACHE PATH
    "Shared base dir for fetched third-party sources (empty = <build>/_deps)")
if(ERGO_DEPENDENCY_CACHE_DIR)
    set(FETCHCONTENT_BASE_DIR "${ERGO_DEPENDENCY_CACHE_DIR}" CACHE PATH "" FORCE)
endif()

# ---------------------------------------------------------------------------
# ergo_declare_dependency(NAME <name>
#                         GIT_REPOSITORY <url>
#                         GIT_TAG <commit-or-tag>
#                         [LICENSE <spdx>] [HOMEPAGE <url>] [SUMMARY <text>])
#
# Records dependency metadata in global properties. Declaration is cheap and
# side-effect-free: nothing is downloaded until ergo_require_dependency() (for
# add_subdirectory deps) or ergo_populate_dependency() (for custom-built deps)
# is called. Pin GIT_TAG to a full commit SHA or an immutable release tag.
# ---------------------------------------------------------------------------
function(ergo_declare_dependency)
    cmake_parse_arguments(D "" "NAME;GIT_REPOSITORY;GIT_TAG;LICENSE;HOMEPAGE;SUMMARY" "" ${ARGN})
    if(NOT D_NAME)
        message(FATAL_ERROR "ergo_declare_dependency: NAME is required")
    endif()
    if(NOT D_GIT_REPOSITORY OR NOT D_GIT_TAG)
        message(FATAL_ERROR
            "ergo_declare_dependency(${D_NAME}): GIT_REPOSITORY and a pinned GIT_TAG are both required")
    endif()

    set_property(GLOBAL PROPERTY _ergo_dep_${D_NAME}_repo     "${D_GIT_REPOSITORY}")
    set_property(GLOBAL PROPERTY _ergo_dep_${D_NAME}_tag      "${D_GIT_TAG}")
    set_property(GLOBAL PROPERTY _ergo_dep_${D_NAME}_license  "${D_LICENSE}")
    set_property(GLOBAL PROPERTY _ergo_dep_${D_NAME}_homepage "${D_HOMEPAGE}")
    set_property(GLOBAL PROPERTY _ergo_dep_${D_NAME}_summary  "${D_SUMMARY}")
    set_property(GLOBAL APPEND PROPERTY _ergo_declared_deps "${D_NAME}")

    # Per-dependency source override for offline / vendored builds:
    #   -DERGO_SOURCE_DIR_<UPPER>=<path>  bypasses fetching entirely.
    string(TOUPPER "${D_NAME}" _upper)
    set(ERGO_SOURCE_DIR_${_upper} "" CACHE PATH
        "Use a local source tree for '${D_NAME}' instead of fetching (offline override)")
endfunction()

# Internal: assert a dependency was declared; emit FetchContent_Declare for it,
# honouring an offline source override. EXTRA_ARGS are passed through verbatim
# (e.g. SOURCE_SUBDIR for source-only deps).
function(_ergo_fc_declare NAME)
    get_property(_known GLOBAL PROPERTY _ergo_declared_deps)
    if(NOT NAME IN_LIST _known)
        message(FATAL_ERROR
            "ergo dependency '${NAME}' was never declared. "
            "Add an ergo_declare_dependency(NAME ${NAME} ...) in third_party/dependencies.cmake.")
    endif()

    string(TOUPPER "${NAME}" _upper)
    if(ERGO_SOURCE_DIR_${_upper})
        if(NOT EXISTS "${ERGO_SOURCE_DIR_${_upper}}")
            message(FATAL_ERROR
                "ERGO_SOURCE_DIR_${_upper}='${ERGO_SOURCE_DIR_${_upper}}' does not exist")
        endif()
        message(STATUS "ergo dep '${NAME}': using local source ${ERGO_SOURCE_DIR_${_upper}}")
        FetchContent_Declare(${NAME}
            SOURCE_DIR "${ERGO_SOURCE_DIR_${_upper}}" ${ARGN})
        return()
    endif()

    if(NOT ERGO_FETCH_DEPENDENCIES)
        message(FATAL_ERROR
            "ergo dependency '${NAME}' is required but ERGO_FETCH_DEPENDENCIES=OFF and no "
            "ERGO_SOURCE_DIR_${_upper} override was given. Either enable fetching or point "
            "ERGO_SOURCE_DIR_${_upper} at a local checkout.")
    endif()

    get_property(_repo GLOBAL PROPERTY _ergo_dep_${NAME}_repo)
    get_property(_tag  GLOBAL PROPERTY _ergo_dep_${NAME}_tag)
    message(STATUS "ergo dep '${NAME}': fetching ${_repo}@${_tag}")
    FetchContent_Declare(${NAME}
        GIT_REPOSITORY "${_repo}"
        GIT_TAG        "${_tag}"
        GIT_SHALLOW    TRUE
        ${ARGN})
endfunction()

# ---------------------------------------------------------------------------
# ergo_require_dependency(NAME [VERIFY_TARGETS <t>...])
#
# For dependencies that ship a usable modern CMake build: fetch and
# add_subdirectory() it via FetchContent, then assert the expected imported
# targets actually exist. Set any upstream cache options *before* calling this.
# ---------------------------------------------------------------------------
function(ergo_require_dependency NAME)
    cmake_parse_arguments(R "" "" "VERIFY_TARGETS" ${ARGN})
    _ergo_fc_declare(${NAME})
    FetchContent_MakeAvailable(${NAME})
    if(R_VERIFY_TARGETS)
        ergo_verify_targets(${NAME} ${R_VERIFY_TARGETS})
    endif()
endfunction()

# ---------------------------------------------------------------------------
# ergo_populate_dependency(NAME OUT_SOURCE_DIR <var>)
#
# For dependencies whose own CMake we do NOT want to run (ancient minimums,
# extra GL/test targets, etc.): download the source only and hand back its path
# so the caller can define its own target over the upstream sources.
# ---------------------------------------------------------------------------
function(ergo_populate_dependency NAME)
    cmake_parse_arguments(P "" "OUT_SOURCE_DIR" "" ${ARGN})
    if(NOT P_OUT_SOURCE_DIR)
        message(FATAL_ERROR "ergo_populate_dependency(${NAME}): OUT_SOURCE_DIR is required")
    endif()
    # SOURCE_SUBDIR pointed at a non-existent dir keeps FetchContent from trying
    # to add_subdirectory the upstream tree on populate.
    _ergo_fc_declare(${NAME} SOURCE_SUBDIR _ergo_no_build)
    FetchContent_GetProperties(${NAME})
    if(NOT ${NAME}_POPULATED)
        FetchContent_Populate(${NAME})
    endif()
    set(${P_OUT_SOURCE_DIR} "${${NAME}_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# ergo_verify_targets(CONTEXT <target>...)
#
# Fail loudly if any expected target is missing after a dependency was made
# available (catches upstream renames / option changes early — no silent skip).
# ---------------------------------------------------------------------------
function(ergo_verify_targets CONTEXT)
    foreach(_t IN LISTS ARGN)
        if(NOT TARGET ${_t})
            message(FATAL_ERROR
                "ergo dependency '${CONTEXT}' did not provide expected target '${_t}'. "
                "Upstream layout may have changed; update cmake/deps/${CONTEXT}.cmake.")
        endif()
    endforeach()
endfunction()
