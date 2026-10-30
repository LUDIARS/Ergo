# ---------------------------------------------------------------------------
# ErgoPackageWiring.cmake — building blocks for cmake/packages/<name>.cmake
#
# A wiring file (one per package, named by the manifest's `cmake.wiring`) is
# include()d by ErgoPackageResolve.cmake inside a function, with:
#   ERGO_PKG_NAME        package name (manifest key)
#   ERGO_PKG_ROUTE       host / provided / source / sibling / installed / fetch
#   ERGO_PKG_SOURCE_DIR  package root ("" when unknown, e.g. imported targets)
#   ERGO_PKG_BINARY_DIR  binary dir for add_subdirectory
#   ERGO_PKG_FEATURES    selected features
#   ERGO_PKG_MANIFEST    prefix of the loaded manifest variables
# The wiring file applies package-specific quirks, then calls
# ergo_pkg_wire_default() (or the steps below one by one):
#   ergo_pkg_wire_cache_defaults()   `cmake.cache` as defaults, never FORCE
#   ergo_pkg_wire_feature_cache()    selected features' `cache` (macro: sets
#                                    normal variables in the wiring scope, so
#                                    only the package's subtree sees them)
#   ergo_pkg_wire_add_subdirectory() add_subdirectory(... EXCLUDE_FROM_ALL)
#   ergo_pkg_wire_verify()           targets / provides / contract (PKG-10)
#   ergo_pkg_wire_exports()          `exports` → named absolute paths
# Only routes that acquire (source / sibling / fetch) set anything or call
# add_subdirectory; host / provided / installed packages are configured
# already, so the helpers only compare and verify there.
#
# Results go to global properties (read by ErgoPackages.cmake):
#   ERGO_PACKAGES_ACQUIRE_FINDINGS   findings (append)
#   ERGO_PKG_RES_<name>_EXPORTS      "<VAR>=<absolute path>" pairs
#   ERGO_PKG_RES_<name>_PRESENT      declared targets that exist
#   ERGO_PKG_RES_<name>_IMPORTED / _VERSION  facts of the first present target
# ---------------------------------------------------------------------------
include_guard(GLOBAL)
cmake_policy(SET CMP0057 NEW)  # if(IN_LIST) also under cmake -P, where no policy version is set

# Append findings to the hub-wide acquisition list.
function(ergo_pkg_res_add_findings)
    if(ARGN)
        set_property(GLOBAL APPEND PROPERTY ERGO_PACKAGES_ACQUIRE_FINDINGS ${ARGN})
    endif()
endfunction()

# TRUE when the route builds the package from source inside this configure.
function(ergo_pkg_route_acquires out_var route)
    if("${route}" MATCHES "^(source|sibling|fetch)$")
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Internal: current value the host gave <var> ("" + FALSE when unset).
function(_ergo_pkg_host_value out_value out_defined var)
    if(DEFINED CACHE{${var}})
        set(${out_value} "$CACHE{${var}}" PARENT_SCOPE)
        set(${out_defined} TRUE PARENT_SCOPE)
    elseif(DEFINED ${var})
        set(${out_value} "${${var}}" PARENT_SCOPE)
        set(${out_defined} TRUE PARENT_SCOPE)
    else()
        set(${out_value} "" PARENT_SCOPE)
        set(${out_defined} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Internal: PKG-15 for every "<VAR>=<value>" the host already set differently.
function(_ergo_pkg_wire_collisions)
    set(_host "")
    foreach(_pair IN LISTS ARGN)
        if(_pair MATCHES "^([^=]+)=(.*)$")
            _ergo_pkg_host_value(_value _defined "${CMAKE_MATCH_1}")
            if(_defined)
                list(APPEND _host "${CMAKE_MATCH_1}=${_value}")
            endif()
        endif()
    endforeach()
    ergo_pkg_rule_collisions(_f STRICT "${ERGO_PACKAGES_STRICT}"
        HOST_CACHE ${_host} WIRING_CACHE ${ARGN})
    ergo_pkg_res_add_findings(${_f})
endfunction()

function(ergo_pkg_wire_cache_defaults)
    set(_pairs ${${ERGO_PKG_MANIFEST}_PKG_${ERGO_PKG_NAME}_CACHE})
    _ergo_pkg_wire_collisions(${_pairs})
    ergo_pkg_route_acquires(_acquires "${ERGO_PKG_ROUTE}")
    if(NOT _acquires)
        return()
    endif()
    foreach(_pair IN LISTS _pairs)
        if(NOT _pair MATCHES "^([^=]+)=(.*)$")
            continue()
        endif()
        set(_var "${CMAKE_MATCH_1}")
        set(_value "${CMAKE_MATCH_2}")
        if(DEFINED CACHE{${_var}})
            continue()  # the host decided; never FORCE
        endif()
        if(_value MATCHES "^(ON|OFF|TRUE|FALSE)$")
            set(_type BOOL)
        else()
            set(_type STRING)
        endif()
        set(${_var} "${_value}" CACHE ${_type}
            "Default from ergo-packages.json (${ERGO_PKG_NAME}); the host may override")
    endforeach()
endfunction()

# Internal: "<VAR>=<value>" pairs of the selected features (declared order).
function(_ergo_pkg_wire_feature_pairs out_var)
    set(_pairs "")
    foreach(_f IN LISTS ERGO_PKG_FEATURES)
        list(APPEND _pairs ${${ERGO_PKG_MANIFEST}_PKG_${ERGO_PKG_NAME}_FEAT_${_f}_CACHE})
    endforeach()
    _ergo_pkg_wire_collisions(${_pairs})
    ergo_pkg_route_acquires(_acquires "${ERGO_PKG_ROUTE}")
    if(NOT _acquires)
        set(_pairs "")
    endif()
    set(${out_var} "${_pairs}" PARENT_SCOPE)
endfunction()

macro(ergo_pkg_wire_feature_cache)
    _ergo_pkg_wire_feature_pairs(_ergo_pkg_wire_pairs)
    foreach(_ergo_pkg_wire_pair IN LISTS _ergo_pkg_wire_pairs)
        if(_ergo_pkg_wire_pair MATCHES "^([^=]+)=(.*)$")
            # Normal variable: option() in the package (CMP0077 NEW) honours
            # it, and it does not outlive the wiring scope.
            set(${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
        endif()
    endforeach()
endmacro()

function(ergo_pkg_wire_add_subdirectory)
    ergo_pkg_route_acquires(_acquires "${ERGO_PKG_ROUTE}")
    if(_acquires)
        add_subdirectory("${ERGO_PKG_SOURCE_DIR}" "${ERGO_PKG_BINARY_DIR}" EXCLUDE_FROM_ALL)
    endif()
endfunction()

# Internal: does <target> publish compile definition <def> (DEF or DEF=...)?
function(_ergo_pkg_target_defines out_var target def)
    set(_real "${target}")
    get_target_property(_aliased "${target}" ALIASED_TARGET)
    if(_aliased)
        set(_real "${_aliased}")
    endif()
    get_target_property(_defs "${_real}" INTERFACE_COMPILE_DEFINITIONS)
    set(_hit FALSE)
    if(_defs)
        foreach(_d IN LISTS _defs)
            if(_d STREQUAL "${def}" OR _d MATCHES "^${def}=")
                set(_hit TRUE)
            endif()
        endforeach()
    endif()
    set(${out_var} ${_hit} PARENT_SCOPE)
endfunction()

# ergo_pkg_verify_layout(<prefix> <name> <route> <features...>)
# Usable outside a wiring file too (late check of ERGO_PACKAGE_PROVIDED_*).
function(ergo_pkg_verify_layout prefix name route)
    set(_features ${ARGN})
    set(_q "${prefix}_PKG_${name}")
    set(_provides "")
    set(_declared ${${_q}_TARGETS})
    set(_contracts "")
    foreach(_f IN LISTS ${_q}_FEATURES)
        string(REPLACE ";" "," _pl "${${_q}_FEAT_${_f}_PROVIDES}")
        list(APPEND _provides "${_f}=${_pl}")
        list(APPEND _declared ${${_q}_FEAT_${_f}_PROVIDES})
        if(_f IN_LIST _features AND NOT "${${_q}_FEAT_${_f}_CONTRACT}" STREQUAL "")
            set(_owner ${${_q}_FEAT_${_f}_PROVIDES} ${${_q}_TARGETS})
            list(GET _owner 0 _owner)
            list(APPEND _contracts "${_f}:${_owner}:${${_q}_FEAT_${_f}_CONTRACT}")
        endif()
    endforeach()
    ergo_pkg_required_targets(_required TARGETS ${${_q}_TARGETS}
        SELECTED ${_features} PROVIDES ${_provides})

    set(_present "")
    foreach(_t IN LISTS _declared)
        if(TARGET "${_t}")
            list(APPEND _present "${_t}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _present)
    set(_honoured "")
    foreach(_c IN LISTS _contracts)
        string(REGEX MATCH "^([^:]+):(.+):([^:]+)$" _m "${_c}")
        if(TARGET "${CMAKE_MATCH_2}")
            _ergo_pkg_target_defines(_hit "${CMAKE_MATCH_2}" "${CMAKE_MATCH_3}")
            if(_hit)
                list(APPEND _honoured "${_c}")
            endif()
        endif()
    endforeach()
    ergo_pkg_rule_layout(_f PACKAGE "${name}" ROUTE "${route}"
        REQUIRED ${_required} PRESENT ${_present}
        CONTRACTS ${_contracts} HONOURED ${_honoured})
    ergo_pkg_res_add_findings(${_f})
    set_property(GLOBAL PROPERTY ERGO_PKG_RES_${name}_PRESENT "${_present}")
endfunction()

function(ergo_pkg_wire_verify)
    ergo_pkg_verify_layout("${ERGO_PKG_MANIFEST}" "${ERGO_PKG_NAME}" "${ERGO_PKG_ROUTE}"
        ${ERGO_PKG_FEATURES})
endfunction()

function(ergo_pkg_wire_exports)
    set(_out "")
    if(NOT "${ERGO_PKG_SOURCE_DIR}" STREQUAL "")
        foreach(_pair IN LISTS ${ERGO_PKG_MANIFEST}_PKG_${ERGO_PKG_NAME}_EXPORTS)
            if(_pair MATCHES "^([^=]+)=(.*)$")
                list(APPEND _out "${CMAKE_MATCH_1}=${ERGO_PKG_SOURCE_DIR}/${CMAKE_MATCH_2}")
            endif()
        endforeach()
    endif()
    set_property(GLOBAL PROPERTY ERGO_PKG_RES_${ERGO_PKG_NAME}_EXPORTS "${_out}")
endfunction()

macro(ergo_pkg_wire_default)
    ergo_pkg_wire_cache_defaults()
    ergo_pkg_wire_feature_cache()
    ergo_pkg_wire_add_subdirectory()
    ergo_pkg_wire_verify()
    ergo_pkg_wire_exports()
endmacro()
