# ---------------------------------------------------------------------------
# ErgoPackages.cmake — entry point of the LUDIARS package hub
#
# Included once from the top-level CMakeLists.txt, right after the managed
# third-party registry and before any module section. Design:
# spec/feature/package-hub.md (manifest §2, checks §4, modes §6).
#
# Stage S1 (this file): manifest + rules + record only.
#   * ERGO_PACKAGES unset  -> compatibility mode M0. The hub acquires nothing
#     and changes nothing: link-package targets the host already created are
#     recorded as host-unmanaged, every finding is recorded, configure never
#     fails because of the hub.
#   * ERGO_PACKAGES set    -> the selection is checked (PKG-01/02/06/11/12/
#     13/14/15) and errors stop the configure, all listed at once. Acquisition
#     routes (host-provided SHA check, local source, sibling, fetch) arrive in
#     stage S2; until then nothing is add_subdirectory'd here either.
#
# Results: <build>/ergo-packages.resolved.json (written once at the end of
# this directory, byte-identical for the same configuration) and the File API
# query <build>/.cmake/api/v1/query/client-ergo/query.json.
#
# This file names no concrete package: everything comes from the manifest.
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/ErgoPackageManifest.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ErgoPackageRules.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ErgoPackageReport.cmake")

# Do not create the cache entry when the host already set a normal variable
# (set(CACHE) would otherwise drop that binding under CMP0126 OLD).
if(NOT DEFINED ERGO_PACKAGES)
    set(ERGO_PACKAGES "" CACHE STRING
        "LUDIARS link packages managed by the Ergo package hub (e.g. pictor;tela). Empty = compatibility mode M0")
endif()
option(ERGO_PACKAGES_STRICT
    "Ergo package hub: treat warning-level findings as configure errors (CI / shipping)" OFF)

# Test hook: tests/packages/ point this at fixture manifests.
if(NOT DEFINED ERGO_PACKAGES_MANIFEST_FILE)
    get_filename_component(ERGO_PACKAGES_MANIFEST_FILE
        "${CMAKE_CURRENT_LIST_DIR}/../../ergo-packages.json" ABSOLUTE)
endif()
get_filename_component(_ergo_packages_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# Platform name used by manifest `platforms` (desktop / android / ios /
# emscripten).
function(_ergo_packages_platform out_var)
    if(EMSCRIPTEN OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        set(_p emscripten)
    elseif(ANDROID)
        set(_p android)
    elseif(IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(_p ios)
    else()
        set(_p desktop)
    endif()
    set(${out_var} ${_p} PARENT_SCOPE)
endfunction()

# Variable-name suffix for a package (ERGO_PACKAGE_*_<UPPER>).
function(_ergo_packages_upper out_var name)
    string(TOUPPER "${name}" _u)
    string(REPLACE "-" "_" _u "${_u}")
    set(${out_var} "${_u}" PARENT_SCOPE)
endfunction()

# Record where an existing target comes from: real target behind an alias,
# imported or not, its SOURCE_DIR and VERSION (empty when not set).
function(_ergo_packages_describe_target target out_source out_imported out_version)
    set(_real "${target}")
    get_target_property(_aliased "${target}" ALIASED_TARGET)
    if(_aliased)
        set(_real "${_aliased}")
    endif()
    get_target_property(_imported "${_real}" IMPORTED)
    get_target_property(_source "${_real}" SOURCE_DIR)
    get_target_property(_version "${_real}" VERSION)
    foreach(_v _source _version)
        if(NOT ${_v})
            set(${_v} "")
        endif()
    endforeach()
    set(${out_source} "${_source}" PARENT_SCOPE)
    set(${out_imported} "${_imported}" PARENT_SCOPE)
    set(${out_version} "${_version}" PARENT_SCOPE)
endfunction()

# gitlink SHA recorded in the Ergo tree for a submodule path ("" without git).
function(_ergo_packages_gitlink out_var root path)
    set(_sha "")
    find_package(Git QUIET)
    if(GIT_EXECUTABLE)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${root}" ls-tree HEAD -- "${path}"
            OUTPUT_VARIABLE _out ERROR_QUIET RESULT_VARIABLE _rc
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(_rc EQUAL 0 AND _out MATCHES "^160000 commit ([0-9a-f]+)")
            set(_sha "${CMAKE_MATCH_1}")
        endif()
    endif()
    set(${out_var} "${_sha}" PARENT_SCOPE)
endfunction()

# Snapshot which manifest targets exist before any Ergo module ran, so the
# report can tell host-created targets from ones a module found later.
function(_ergo_packages_begin)
    ergo_pkg_manifest_load(_m "${ERGO_PACKAGES_MANIFEST_FILE}")
    set(_present "")
    foreach(_n IN LISTS _m_LINK)
        foreach(_t IN LISTS _m_PKG_${_n}_TARGETS)
            if(TARGET "${_t}")
                list(APPEND _present "${_n}:${_t}")
            endif()
        endforeach()
    endforeach()
    set_property(GLOBAL PROPERTY ERGO_PACKAGES_TARGETS_AT_INCLUDE "${_present}")
    set_property(GLOBAL PROPERTY ERGO_PACKAGES_MANIFEST "${ERGO_PACKAGES_MANIFEST_FILE}")
    set_property(GLOBAL PROPERTY ERGO_PACKAGES_ROOT "${_ergo_packages_root}")
    ergo_pkg_report_install_query("${CMAKE_BINARY_DIR}")
endfunction()

# Runs deferred at the end of the including directory: evaluates the rules,
# writes resolved.json and (outside M0) stops on errors.
function(_ergo_packages_finalize)
    get_property(_manifest GLOBAL PROPERTY ERGO_PACKAGES_MANIFEST)
    get_property(_at_include GLOBAL PROPERTY ERGO_PACKAGES_TARGETS_AT_INCLUDE)
    get_property(_root GLOBAL PROPERTY ERGO_PACKAGES_ROOT)
    ergo_pkg_manifest_load(_m "${_manifest}")
    _ergo_packages_platform(_platform)

    set(_selected ${ERGO_PACKAGES})
    if(_selected)
        set(_mode selected)
    else()
        set(_mode M0)
    endif()

    set(_findings "")
    foreach(_e IN LISTS _m_ERRORS)
        ergo_pkg_add_finding(_findings MANIFEST error manifest "${_e}")
    endforeach()

    ergo_pkg_rule_unknown_names(_f SELECTED ${_selected} KNOWN ${_m_LINK})
    list(APPEND _findings ${_f})

    # Declared graph (package requires + feature requires) and provided
    # targets, over the whole manifest.
    set(_nodes "")
    set(_edges "")
    set(_provided "")
    foreach(_n IN LISTS _m_LINK)
        list(APPEND _nodes "${_n}")
        foreach(_r IN LISTS _m_PKG_${_n}_REQUIRES)
            list(APPEND _edges "${_n}>${_r}")
        endforeach()
        foreach(_t IN LISTS _m_PKG_${_n}_TARGETS)
            list(APPEND _provided "${_n}:${_t}")
        endforeach()
        foreach(_ft IN LISTS _m_PKG_${_n}_FEATURES)
            foreach(_r IN LISTS _m_PKG_${_n}_FEAT_${_ft}_REQUIRES)
                if(_r MATCHES "/")
                    list(APPEND _edges "${_n}/${_ft}>${_r}")
                elseif(_r IN_LIST _m_PKG_${_n}_FEATURES)
                    list(APPEND _edges "${_n}/${_ft}>${_n}/${_r}")
                else()
                    list(APPEND _edges "${_n}/${_ft}>${_r}")
                endif()
            endforeach()
            foreach(_t IN LISTS _m_PKG_${_n}_FEAT_${_ft}_PROVIDES)
                list(APPEND _provided "${_n}:${_t}")
            endforeach()
        endforeach()
    endforeach()
    ergo_pkg_rule_cycles(_f NODES ${_nodes} EDGES ${_edges})
    list(APPEND _findings ${_f})
    ergo_pkg_rule_collisions(_f STRICT "${ERGO_PACKAGES_STRICT}" PROVIDED ${_provided})
    list(APPEND _findings ${_f})

    # Per link package: pin, override, requires, features, observed state.
    set(_available "")
    foreach(_n IN LISTS _m_LINK)
        set(_q "_m_PKG_${_n}")
        set(_r "_r_PKG_${_n}")
        _ergo_packages_upper(_u "${_n}")

        ergo_pkg_rule_rev_format(_f PACKAGE "${_n}" REV "${${_q}_REV}")
        list(APPEND _findings ${_f})
        set(_override "${ERGO_PACKAGE_REV_${_u}}")
        ergo_pkg_rule_rev_override(_f PACKAGE "${_n}" PIN "${${_q}_REV}"
            OVERRIDE "${_override}" STRICT "${ERGO_PACKAGES_STRICT}")
        list(APPEND _findings ${_f})

        if(_n IN_LIST _selected)
            ergo_pkg_rule_requires(_f PACKAGE "${_n}"
                REQUIRES ${${_q}_REQUIRES} AVAILABLE ${_selected})
            list(APPEND _findings ${_f})
            ergo_pkg_rule_cmake_minimum(_f PACKAGE "${_n}"
                MINIMUM "${${_q}_CMAKE_MINIMUM}" CURRENT "${CMAKE_VERSION}")
            list(APPEND _findings ${_f})
        endif()

        # Feature tables as the rule inputs expect them.
        set(_defaults "")
        set(_fplatforms "")
        set(_reserved "")
        set(_conflicts "")
        set(_freqs "")
        set(_render_features "")
        foreach(_ft IN LISTS ${_q}_FEATURES)
            set(_fq "${_q}_FEAT_${_ft}")
            list(APPEND _defaults "${_ft}=${${_fq}_DEFAULT}")
            string(REPLACE ";" "," _pl "${${_fq}_PLATFORMS}")
            list(APPEND _fplatforms "${_ft}=${_pl}")
            if(${_fq}_RESERVED)
                list(APPEND _reserved "${_ft}")
            endif()
            foreach(_c IN LISTS ${_fq}_CONFLICTS)
                list(APPEND _conflicts "${_ft}!${_c}")
            endforeach()
            set(_same_pkg_reqs "")
            foreach(_fr IN LISTS ${_fq}_REQUIRES)
                if(_fr IN_LIST ${_q}_FEATURES)
                    list(APPEND _same_pkg_reqs "${_fr}")
                endif()
            endforeach()
            string(REPLACE ";" "," _rl "${_same_pkg_reqs}")
            list(APPEND _freqs "${_ft}=${_rl}")
            if(${_fq}_PROVIDES AND NOT ${_fq}_RESERVED)
                list(APPEND _render_features "${_ft}")
            endif()
        endforeach()

        # A real-path module that needs this package requires one render
        # feature (only once the package is selected).
        set(_require_any "")
        if(_n IN_LIST _selected)
            foreach(_mod IN LISTS _m_MODULES)
                if(_m_MOD_${_mod}_NEED_KIND STREQUAL "real-path"
                   AND _n IN_LIST _m_MOD_${_mod}_NEEDS AND TARGET ${_mod})
                    set(_require_any ${_render_features})
                endif()
            endforeach()
        endif()

        ergo_pkg_features_select(_features _f PACKAGE "${_n}" PLATFORM "${_platform}"
            FEATURES ${${_q}_FEATURES} DEFAULTS ${_defaults} PLATFORMS ${_fplatforms}
            RESERVED ${_reserved} REQUEST ${ERGO_PACKAGE_FEATURES_${_u}})
        list(APPEND _findings ${_f})
        ergo_pkg_rule_feature_combination(_f PACKAGE "${_n}" PLATFORM "${_platform}"
            SELECTED ${_features} PLATFORMS ${_fplatforms} RESERVED ${_reserved}
            CONFLICTS ${_conflicts} REQUIRES ${_freqs} REQUIRE_ANY ${_require_any})
        list(APPEND _findings ${_f})

        # Observed state of the declared targets.
        set(_state absent)
        set(_present "")
        set(_source "")
        set(_imported FALSE)
        set(_version "")
        foreach(_t IN LISTS ${_q}_TARGETS)
            if(TARGET "${_t}")
                list(APPEND _present "${_t}")
                if(_source STREQUAL "" AND _version STREQUAL "")
                    _ergo_packages_describe_target("${_t}" _source _imported _version)
                endif()
                if("${_n}:${_t}" IN_LIST _at_include)
                    set(_state host-unmanaged)
                elseif(NOT _state STREQUAL "host-unmanaged")
                    set(_state module-found)
                endif()
            endif()
        endforeach()
        if(_present)
            list(APPEND _available "${_n}")
        endif()

        set(${_r}_KIND link)
        set(${_r}_CODE "${${_q}_CODE}")
        set(${_r}_PIN "${${_q}_REV}")
        set(${_r}_EXPECT_TAG "${${_q}_EXPECT_TAG}")
        set(${_r}_OVERRIDE "${_override}")
        set(${_r}_STATE "${_state}")
        set(${_r}_SOURCE_DIR "${_source}")
        set(${_r}_IMPORTED "${_imported}")
        set(${_r}_TARGETS_PRESENT ${_present})
        set(${_r}_FEATURES ${_features})
        set(${_r}_VERSION "${_version}")
    endforeach()

    foreach(_n IN LISTS _m_TOOLS)
        set(_r "_r_PKG_${_n}")
        set(_path "${_m_PKG_${_n}_PATH}")
        _ergo_packages_gitlink(_sha "${_root}" "${_path}")
        set(${_r}_KIND tool)
        set(${_r}_CODE "${_m_PKG_${_n}_CODE}")
        set(${_r}_STATE "${_m_PKG_${_n}_ACQUIRE}")
        set(${_r}_PATH "${_path}")
        set(${_r}_GITLINK "${_sha}")
        if(EXISTS "${_root}/${_path}/.git")
            set(${_r}_INITIALIZED TRUE)
        else()
            set(${_r}_INITIALIZED FALSE)
        endif()
    endforeach()

    foreach(_mod IN LISTS _m_MODULES)
        set(_q "_m_MOD_${_mod}")
        set(_r "_r_MOD_${_mod}")
        if(TARGET ${_mod})
            set(_enabled TRUE)
        else()
            set(_enabled FALSE)
        endif()
        set(_reason "")
        if(${_q}_REASON_VAR)
            set(_reason "${${${_q}_REASON_VAR}}")
        elseif(${_q}_WHEN)
            set(_when_value "${${${_q}_WHEN}}")
            if(_when_value)
                set(_reason "${${_q}_WHEN}=ON")
            else()
                set(_reason "${${_q}_WHEN}=OFF")
            endif()
        endif()
        set(_real_path FALSE)
        if(${_q}_REAL_PATH_VAR)
            set(_real_path_value "${${${_q}_REAL_PATH_VAR}}")
            if(_real_path_value)
                set(_real_path TRUE)
            endif()
        endif()
        ergo_pkg_rule_module_needs(_f MODULE "${_mod}" ENABLED ${_enabled}
            NEEDS ${${_q}_NEEDS} AVAILABLE ${_available}
            NEED_KIND "${${_q}_NEED_KIND}" REAL_PATH ${_real_path}
            REASON "${_reason}" MODE ${_mode})
        list(APPEND _findings ${_f})

        set(${_r}_ENABLED ${_enabled})
        set(${_r}_NEEDS ${${_q}_NEEDS})
        set(${_r}_OPTIONAL ${${_q}_OPTIONAL})
        set(${_r}_PORTED_FROM ${${_q}_PORTED_FROM})
        set(${_r}_REASON "${_reason}")
    endforeach()

    foreach(_o IN LISTS _m_OBSERVE)
        foreach(_field CODE GROUP PATH_HINT RELATIONS)
            set(_r_OBS_${_o}_${_field} "${_m_OBS_${_o}_${_field}}")
        endforeach()
    endforeach()

    file(RELATIVE_PATH _manifest_rel "${_root}" "${_manifest}")
    set(_r_MODE ${_mode})
    set(_r_STRICT ${ERGO_PACKAGES_STRICT})
    set(_r_PLATFORM ${_platform})
    set(_r_CMAKE_VERSION ${CMAKE_VERSION})
    set(_r_MANIFEST "${_manifest_rel}")
    set(_r_SELECTED ${_selected})
    set(_r_PACKAGES ${_m_PACKAGES})
    set(_r_MODULES ${_m_MODULES})
    set(_r_OBSERVE ${_m_OBSERVE})
    set(_r_FINDINGS ${_findings})
    ergo_pkg_report_render(_json _r)
    set(_resolved "${CMAKE_BINARY_DIR}/ergo-packages.resolved.json")
    ergo_pkg_report_write("${_resolved}" "${_json}")

    # Summary. M0 only informs; outside M0 every error is listed, then stop.
    set(_states "")
    foreach(_n IN LISTS _m_LINK)
        list(APPEND _states "${_n}=${_r_PKG_${_n}_STATE}")
    endforeach()
    string(REPLACE ";" ", " _states "${_states}")
    list(LENGTH _findings _count)
    message(STATUS "ergo packages: mode=${_mode} (${_states}), ${_count} finding(s) -> ${_resolved}")
    ergo_pkg_findings_with_severity(_errors error ${_findings})
    ergo_pkg_findings_with_severity(_warnings warning ${_findings})
    foreach(_w IN LISTS _warnings)
        message(STATUS "ergo packages: ${_w}")
    endforeach()
    if(_mode STREQUAL "M0")
        foreach(_e IN LISTS _errors)
            message(STATUS "ergo packages: ${_e}")
        endforeach()
        return()
    endif()
    if(_errors)
        string(REPLACE ";" "\n  " _list "${_errors}")
        message(FATAL_ERROR "ergo packages: configuration rejected:\n  ${_list}\n(see ${_resolved})")
    endif()
    message(WARNING
        "ergo packages: ERGO_PACKAGES is set but stage S1 only checks and records. "
        "Acquisition (host-provided SHA check / local source / fetch) is not implemented yet, "
        "so the host must still create the package targets itself. See spec/setup/packages.md.")
endfunction()

_ergo_packages_begin()
cmake_language(DEFER CALL _ergo_packages_finalize)
