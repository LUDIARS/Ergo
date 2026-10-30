# ---------------------------------------------------------------------------
# ErgoPackageResolve.cmake — the six resolution routes of a link package
#
# Design: spec/feature/package-hub.md §3. The first route that hits wins and
# every route goes through the same checks:
#   1. host       a declared target already exists (or ERGO_PACKAGE_PROVIDED_
#                 <NAME>=ON: route "provided", verified at the end of the
#                 directory when the host creates the target later)
#   2. source     ERGO_PACKAGE_SOURCE_DIR_<NAME> (a wrong path is PKG-03, no
#                 fall-through)
#   3. sibling    <ERGO_PACKAGES_SIBLING_ROOT>/<Repo> only (Repo = last part
#                 of `repository`; .worktrees and other places are never read)
#   4. installed  find_package(<cmake.find_package.name> CONFIG) for packages
#                 that declare it; never under ERGO_PACKAGES_STRICT (U3)
#   5. fetch      only with ERGO_PACKAGES_FETCH=ON, through _ergo_fc_declare
#                 of cmake/ErgoDependencies.cmake (a SHA pin gets full
#                 history, so the PKG-08 / ancestry probes run; shared
#                 ERGO_DEPENDENCY_CACHE_DIR), HEAD == pin verified (PKG-04).
#                 A source already at the pin in FetchContent's place (left
#                 by `ergo_pkg fetch` or an earlier configure) is used as is,
#                 without network (ErgoPackageFetchCache.cmake)
#   6. none       PKG-03 configure error; for an optional package (the caller
#                 set ERGO_PKG_RES_<name>_OPTIONAL) a record and the package
#                 is not used (ERGO_PKG_RES_<name>_UNUSED). The same holds for
#                 an optional "provided" package the host never created
# The engine names no package: wiring comes from the manifest's
# `cmake.wiring` (cmake/packages/<name>.cmake).
#
# Facts are left in global properties ERGO_PKG_RES_<name>_{ROUTE,SOURCE_DIR,
# PIN,HEAD,DRIFT,DIRTY,VERSION,PRESENT,EXPORTS,UNUSED}; findings are appended
# to ERGO_PACKAGES_ACQUIRE_FINDINGS.
# ---------------------------------------------------------------------------
include_guard(GLOBAL)
cmake_policy(SET CMP0057 NEW)  # if(IN_LIST) also under cmake -P, where no policy version is set
include("${CMAKE_CURRENT_LIST_DIR}/ErgoPackageGit.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ErgoPackageHostDefaults.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ErgoPackageWiring.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ErgoPackageFetchCache.cmake")

# Internal: declared targets (cmake.targets + every feature's provides) that
# exist now.
function(_ergo_pkg_present_targets out_var prefix name)
    set(_q "${prefix}_PKG_${name}")
    set(_declared ${${_q}_TARGETS})
    foreach(_f IN LISTS ${_q}_FEATURES)
        list(APPEND _declared ${${_q}_FEAT_${_f}_PROVIDES})
    endforeach()
    set(_present "")
    foreach(_t IN LISTS _declared)
        if(TARGET "${_t}")
            list(APPEND _present "${_t}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _present)
    set(${out_var} "${_present}" PARENT_SCOPE)
endfunction()

# Internal: SOURCE_DIR of a (non-imported) target, "" for imported ones.
function(_ergo_pkg_target_source out_var target)
    set(_real "${target}")
    get_target_property(_aliased "${target}" ALIASED_TARGET)
    if(_aliased)
        set(_real "${_aliased}")
    endif()
    get_target_property(_imported "${_real}" IMPORTED)
    set(_dir "")
    if(NOT _imported)
        get_target_property(_dir "${_real}" SOURCE_DIR)
        if(NOT _dir)
            set(_dir "")
        endif()
    endif()
    set(${out_var} "${_dir}" PARENT_SCOPE)
endfunction()

# Internal: fetch <name>@<pin> through the third-party engine; source dir out.
function(_ergo_pkg_fetch out_var name repository pin)
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../ErgoDependencies.cmake")
    ergo_pkg_fetch_dep_name(_dep "${name}")
    get_property(_known GLOBAL PROPERTY _ergo_declared_deps)
    if(NOT _dep IN_LIST _known)
        ergo_declare_dependency(NAME ${_dep}
            GIT_REPOSITORY "${repository}" GIT_TAG "${pin}"
            SUMMARY "LUDIARS package '${name}' (Ergo package hub)")
    endif()
    # Prefetched (or earlier fetched) source at the pin: hand it to the
    # engine as its local source so no download step runs. An explicit
    # FetchContent / Ergo override of the host always wins.
    string(TOUPPER "${_dep}" _upper)
    if("${ERGO_SOURCE_DIR_${_upper}}" STREQUAL "" AND "${FETCHCONTENT_SOURCE_DIR_${_upper}}" STREQUAL "")
        set(_base "${FETCHCONTENT_BASE_DIR}")
        if("${_base}" STREQUAL "")
            set(_base "${CMAKE_BINARY_DIR}/_deps")
        endif()
        ergo_pkg_fetch_prefetched(_cached "${_base}" "${name}" "${pin}")
        if(NOT _cached STREQUAL "")
            set(ERGO_SOURCE_DIR_${_upper} "${_cached}")
        endif()
    endif()
    # SOURCE_SUBDIR at a missing dir: populate only, the wiring file does
    # the add_subdirectory (with EXCLUDE_FROM_ALL and the feature scope).
    _ergo_fc_declare(${_dep} SOURCE_SUBDIR _ergo_no_build)
    FetchContent_MakeAvailable(${_dep})
    string(TOLOWER "${_dep}" _lc)
    set(${out_var} "${${_lc}_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

# Internal: include the wiring file with the documented variables in scope.
function(_ergo_pkg_run_wiring prefix name route source features)
    set(ERGO_PKG_NAME "${name}")
    set(ERGO_PKG_ROUTE "${route}")
    set(ERGO_PKG_SOURCE_DIR "${source}")
    set(ERGO_PKG_BINARY_DIR "${CMAKE_BINARY_DIR}/_ergo_packages/${name}")
    set(ERGO_PKG_FEATURES ${features})
    set(ERGO_PKG_MANIFEST "${prefix}")
    include("${${prefix}_PKG_${name}_WIRING}")
endfunction()

# Internal: probe git and turn drift / dirty / expect_tag into findings.
function(_ergo_pkg_check_checkout prefix name route source pin)
    set(_q "${prefix}_PKG_${name}")
    set(_head "")
    set(_drift unknown)
    set(_dirty FALSE)
    set(_findings "")
    if(NOT "${source}" STREQUAL "" AND NOT route STREQUAL "installed")
        if(route STREQUAL "host" OR route STREQUAL "provided")
            ergo_pkg_git_probe(_g "${source}" "${pin}" "${${_q}_EXPECT_TAG}")
        else()
            ergo_pkg_git_probe(_g "${source}" "${pin}" "${${_q}_EXPECT_TAG}" ROOT_ONLY)
        endif()
        set(_head "${_g_HEAD}")
        set(_dirty ${_g_DIRTY})
        ergo_pkg_drift_classify(_drift HEAD "${_g_HEAD}" PIN "${pin}"
            PIN_KNOWN ${_g_PIN_KNOWN} PIN_IS_ANCESTOR ${_g_PIN_IS_ANCESTOR}
            HEAD_IS_ANCESTOR ${_g_HEAD_IS_ANCESTOR})
        ergo_pkg_rule_dirty(_f PACKAGE "${name}" DIRTY ${_dirty} STRICT "${ERGO_PACKAGES_STRICT}")
        list(APPEND _findings ${_f})
        ergo_pkg_rule_expect_tag(_f PACKAGE "${name}" TAG "${${_q}_EXPECT_TAG}"
            TAG_FOUND ${_g_TAG_FOUND} PIN_DESCENDS ${_g_PIN_DESCENDS} PIN "${pin}"
            STRICT "${ERGO_PACKAGES_STRICT}")
        list(APPEND _findings ${_f})
    endif()
    if(route STREQUAL "fetch")
        ergo_pkg_rule_fetched_head(_f PACKAGE "${name}" PIN "${pin}" HEAD "${_head}")
        list(APPEND _findings ${_f})
    endif()
    get_property(_version GLOBAL PROPERTY ERGO_PKG_RES_${name}_VERSION)
    ergo_pkg_rule_drift(_f PACKAGE "${name}" ROUTE "${route}" DRIFT "${_drift}"
        HEAD "${_head}" PIN "${pin}" VERSION "${_version}" STRICT "${ERGO_PACKAGES_STRICT}")
    list(APPEND _findings ${_f})
    ergo_pkg_res_add_findings(${_findings})
    set_property(GLOBAL PROPERTY ERGO_PKG_RES_${name}_HEAD "${_head}")
    set_property(GLOBAL PROPERTY ERGO_PKG_RES_${name}_DRIFT "${_drift}")
    set_property(GLOBAL PROPERTY ERGO_PKG_RES_${name}_DIRTY "${_dirty}")
endfunction()

# Internal: package root of a host-provided package ("" for imported targets).
function(_ergo_pkg_host_root out_var present)
    set(_root "")
    foreach(_t IN LISTS present)
        _ergo_pkg_target_source(_dir "${_t}")
        if(NOT _dir STREQUAL "")
            ergo_pkg_git_probe(_g "${_dir}" "" "")
            if(NOT _g_TOPLEVEL STREQUAL "")
                set(_root "${_g_TOPLEVEL}")
            else()
                set(_root "${_dir}")
            endif()
            break()
        endif()
    endforeach()
    set(${out_var} "${_root}" PARENT_SCOPE)
endfunction()

# Internal: verify, probe and wire a package once its route and source are
# known (shared by the immediate and the late "provided" path).
function(_ergo_pkg_finish prefix name route source pin features)
    set_property(GLOBAL PROPERTY ERGO_PKG_RES_${name}_ROUTE "${route}")
    set_property(GLOBAL PROPERTY ERGO_PKG_RES_${name}_SOURCE_DIR "${source}")
    set(_wiring "${${prefix}_PKG_${name}_WIRING}")
    if(_wiring STREQUAL "" OR NOT EXISTS "${_wiring}")
        set(_f "")
        ergo_pkg_add_finding(_f PKG-10 error "${name}"
            "cmake.wiring '${_wiring}' does not exist (one wiring file per package is required)")
        ergo_pkg_res_add_findings(${_f})
        return()
    endif()
    _ergo_pkg_run_wiring("${prefix}" "${name}" "${route}" "${source}" "${features}")
    _ergo_pkg_check_checkout("${prefix}" "${name}" "${route}" "${source}" "${pin}")
endfunction()

# ergo_pkg_resolve(<prefix> <name> <pin> <features...>)
# Runs at include time, before any Ergo module section.
function(ergo_pkg_resolve prefix name pin)
    set(_features ${ARGN})
    set(_q "${prefix}_PKG_${name}")
    string(TOUPPER "${name}" _u)
    string(REPLACE "-" "_" _u "${_u}")
    set_property(GLOBAL PROPERTY ERGO_PKG_RES_${name}_PIN "${pin}")
    set_property(GLOBAL PROPERTY ERGO_PKG_RES_${name}_FEATURES "${_features}")

    set(_route "")
    set(_source "")
    set(_tried "")

    # 1. host-provided (targets exist) / explicitly provided by the host.
    list(APPEND _tried host)
    _ergo_pkg_present_targets(_present "${prefix}" "${name}")
    if(_present)
        if(ERGO_PACKAGE_PROVIDED_${_u})
            set(_route provided)
        else()
            set(_route host)
        endif()
        _ergo_pkg_host_root(_source "${_present}")
    elseif(ERGO_PACKAGE_PROVIDED_${_u})
        # The host creates the targets after Ergo: verify at the end.
        set_property(GLOBAL PROPERTY ERGO_PKG_RES_${name}_ROUTE provided)
        set_property(GLOBAL APPEND PROPERTY ERGO_PACKAGES_PENDING "${name}")
        return()
    endif()

    # 2. explicit local source.
    if(_route STREQUAL "" AND NOT "${ERGO_PACKAGE_SOURCE_DIR_${_u}}" STREQUAL "")
        list(APPEND _tried source)
        get_filename_component(_dir "${ERGO_PACKAGE_SOURCE_DIR_${_u}}" ABSOLUTE)
        if(NOT EXISTS "${_dir}/CMakeLists.txt")
            ergo_pkg_rule_unresolved(_f PACKAGE "${name}" TRIED ${_tried}
                HINT "ERGO_PACKAGE_SOURCE_DIR_${_u}='${ERGO_PACKAGE_SOURCE_DIR_${_u}}' has no CMakeLists.txt")
            ergo_pkg_res_add_findings(${_f})
            set_property(GLOBAL PROPERTY ERGO_PKG_RES_${name}_ROUTE none)
            return()
        endif()
        set(_route source)
        set(_source "${_dir}")
    endif()

    # 3. sibling checkout <root>/<Repo>, nothing else.
    if(_route STREQUAL "" AND NOT "${ERGO_PACKAGES_SIBLING_ROOT}" STREQUAL ""
       AND NOT "${${_q}_REPO_NAME}" STREQUAL "")
        list(APPEND _tried sibling)
        get_filename_component(_dir "${ERGO_PACKAGES_SIBLING_ROOT}/${${_q}_REPO_NAME}" ABSOLUTE)
        if(EXISTS "${_dir}/CMakeLists.txt")
            set(_route sibling)
            set(_source "${_dir}")
        endif()
    endif()

    # 4. installed package (version only, never under STRICT).
    if(_route STREQUAL "" AND NOT "${${_q}_FIND_PACKAGE}" STREQUAL "")
        if(ERGO_PACKAGES_STRICT)
            list(APPEND _tried "installed(skipped: STRICT)")
        else()
            list(APPEND _tried installed)
            set(_pkg "${${_q}_FIND_PACKAGE}")
            find_package(${_pkg} CONFIG QUIET)
            _ergo_pkg_present_targets(_present "${prefix}" "${name}")
            if(${_pkg}_FOUND AND _present)
                set(_route installed)
                set_property(GLOBAL PROPERTY ERGO_PKG_RES_${name}_VERSION "${${_pkg}_VERSION}")
                set_property(GLOBAL PROPERTY ERGO_PKG_RES_${name}_PACKAGE_DIR "${${_pkg}_DIR}")
            endif()
        endif()
    endif()

    # 5. fetch, only when allowed.
    if(_route STREQUAL "")
        if(ERGO_PACKAGES_FETCH AND NOT "${${_q}_REPOSITORY}" STREQUAL "")
            list(APPEND _tried fetch)
            _ergo_pkg_fetch(_dir "${name}" "${${_q}_REPOSITORY}" "${pin}")
            set(_route fetch)
            set(_source "${_dir}")
        else()
            list(APPEND _tried "fetch(disabled)")
        endif()
    endif()

    # 6. nothing hit (an optional package is then simply not used).
    if(_route STREQUAL "")
        get_property(_optional GLOBAL PROPERTY ERGO_PKG_RES_${name}_OPTIONAL)
        if(_optional)
            ergo_pkg_rule_optional_unused(_f PACKAGE "${name}" TRIED ${_tried})
            set_property(GLOBAL PROPERTY ERGO_PKG_RES_${name}_UNUSED TRUE)
        else()
            ergo_pkg_rule_unresolved(_f PACKAGE "${name}" TRIED ${_tried})
        endif()
        ergo_pkg_res_add_findings(${_f})
        set_property(GLOBAL PROPERTY ERGO_PKG_RES_${name}_ROUTE none)
        return()
    endif()

    _ergo_pkg_finish("${prefix}" "${name}" "${_route}" "${_source}" "${pin}" "${_features}")
endfunction()

# ergo_pkg_resolve_late(<prefix> <name>)
# End-of-directory check of a package the host declared as provided but had
# not created when Ergo was included.
function(ergo_pkg_resolve_late prefix name)
    get_property(_pin GLOBAL PROPERTY ERGO_PKG_RES_${name}_PIN)
    get_property(_features GLOBAL PROPERTY ERGO_PKG_RES_${name}_FEATURES)
    _ergo_pkg_present_targets(_present "${prefix}" "${name}")
    get_property(_optional GLOBAL PROPERTY ERGO_PKG_RES_${name}_OPTIONAL)
    if(_optional AND NOT _present)
        ergo_pkg_rule_optional_unused(_f PACKAGE "${name}" TRIED provided)
        ergo_pkg_res_add_findings(${_f})
        set_property(GLOBAL PROPERTY ERGO_PKG_RES_${name}_UNUSED TRUE)
        set_property(GLOBAL PROPERTY ERGO_PKG_RES_${name}_ROUTE none)
        return()
    endif()
    _ergo_pkg_host_root(_source "${_present}")
    _ergo_pkg_finish("${prefix}" "${name}" provided "${_source}" "${_pin}" "${_features}")
endfunction()
