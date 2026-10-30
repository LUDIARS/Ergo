# ---------------------------------------------------------------------------
# ErgoPackageRules.cmake — pure check rules of the package hub (PKG-xx)
#
# Every function here takes list variables in and hands a list of findings
# back. Nothing reads files, creates targets, touches the cache or looks at
# the platform: callers pass all facts in. That keeps the rules unit-testable
# with `cmake -P` (tests/packages/) and keeps the entry point
# (ErgoPackages.cmake) the only place that knows about the live build.
#
# Finding format (one list element per finding):
#     <id>|<severity>|<subject>|<message>
#   id:       PKG-01 .. PKG-17 (spec/feature/package-hub.md §4)
#   severity: error / warning / record
#   subject:  package, feature (<package>/<feature>) or module name
#   message:  plain text without ';' or '|'
#
# Rules here: PKG-01/02/06/11/12/13/14/15 (stage S1). The acquisition rules
# (PKG-03/04/05/07/08/09/10/16, stage S2) live in ErgoPackageRulesAcquire.cmake,
# included at the end of this file so callers include one file.
# ---------------------------------------------------------------------------
include_guard(GLOBAL)
cmake_policy(SET CMP0057 NEW)  # if(IN_LIST) also under cmake -P, where no policy version is set

# Append one finding to the list variable named <list_var> (caller scope).
macro(ergo_pkg_add_finding list_var id severity subject message)
    string(REPLACE ";" "," _ergo_pkg_msg "${message}")
    string(REPLACE "|" "/" _ergo_pkg_msg "${_ergo_pkg_msg}")
    list(APPEND ${list_var} "${id}|${severity}|${subject}|${_ergo_pkg_msg}")
endmacro()

# Severity for a "warning + record, error under STRICT" rule.
function(_ergo_pkg_strict_severity out_var strict)
    if(strict)
        set(${out_var} error PARENT_SCOPE)
    else()
        set(${out_var} warning PARENT_SCOPE)
    endif()
endfunction()

# ergo_pkg_findings_with_severity(<out> <severity> <findings...>)
# Keep only the findings whose severity matches.
function(ergo_pkg_findings_with_severity out_var severity)
    set(_kept "")
    foreach(_f IN LISTS ARGN)
        if(_f MATCHES "^[^|]*\\|${severity}\\|")
            list(APPEND _kept "${_f}")
        endif()
    endforeach()
    set(${out_var} "${_kept}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# PKG-01: ERGO_PACKAGES names a package the manifest does not know (or one
# that is not a link package — tools and observed repos are not selectable).
#   ergo_pkg_rule_unknown_names(<out> SELECTED <names...> KNOWN <names...>)
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_unknown_names out_var)
    cmake_parse_arguments(A "" "" "SELECTED;KNOWN" ${ARGN})
    set(_findings "")
    foreach(_name IN LISTS A_SELECTED)
        if(NOT _name IN_LIST A_KNOWN)
            string(REPLACE ";" ", " _known_text "${A_KNOWN}")
            ergo_pkg_add_finding(_findings PKG-01 error "${_name}"
                "ERGO_PACKAGES names an unknown link package (known: ${_known_text})")
        endif()
    endforeach()
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# PKG-02: a pin must be a full 40-digit lowercase commit SHA. Branch names,
# short SHAs and tags are rejected (they are not reproducible pins).
#   ergo_pkg_rule_rev_format(<out> PACKAGE <name> REV <rev> [FIELD <label>])
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_rev_format out_var)
    cmake_parse_arguments(A "" "PACKAGE;REV;FIELD" "" ${ARGN})
    if(NOT A_FIELD)
        set(A_FIELD "rev")
    endif()
    set(_findings "")
    # CMake regex has no {n} repetition: check the alphabet, then the length.
    string(LENGTH "${A_REV}" _len)
    if(NOT "${A_REV}" MATCHES "^[0-9a-f]+$" OR NOT _len EQUAL 40)
        if("${A_REV}" STREQUAL "")
            set(_shown "(empty)")
        else()
            set(_shown "${A_REV}")
        endif()
        ergo_pkg_add_finding(_findings PKG-02 error "${A_PACKAGE}"
            "${A_FIELD} '${_shown}' is not a 40-digit lowercase commit SHA")
    endif()
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# PKG-06: ERGO_PACKAGE_REV_<NAME> overrides the manifest pin (verification
# use only). Recorded as a warning; an error under STRICT. The override value
# itself must still be a full SHA (PKG-02, field ERGO_PACKAGE_REV_<NAME>).
#   ergo_pkg_rule_rev_override(<out> PACKAGE <name> PIN <rev> OVERRIDE <rev>
#                              [STRICT <bool>])
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_rev_override out_var)
    cmake_parse_arguments(A "" "PACKAGE;PIN;OVERRIDE;STRICT" "" ${ARGN})
    set(_findings "")
    if(NOT "${A_OVERRIDE}" STREQUAL "")
        string(TOUPPER "${A_PACKAGE}" _upper)
        string(REPLACE "-" "_" _upper "${_upper}")
        ergo_pkg_rule_rev_format(_format PACKAGE "${A_PACKAGE}" REV "${A_OVERRIDE}"
            FIELD "ERGO_PACKAGE_REV_${_upper}")
        list(APPEND _findings ${_format})
        if(NOT "${A_OVERRIDE}" STREQUAL "${A_PIN}")
            _ergo_pkg_strict_severity(_severity "${A_STRICT}")
            ergo_pkg_add_finding(_findings PKG-06 ${_severity} "${A_PACKAGE}"
                "pin ${A_PIN} overridden by ERGO_PACKAGE_REV_${_upper}=${A_OVERRIDE}")
        endif()
    endif()
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# PKG-11 (a): a package's declared `requires` must all be selected.
#   ergo_pkg_rule_requires(<out> PACKAGE <name> REQUIRES <names...>
#                          AVAILABLE <names...>)
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_requires out_var)
    cmake_parse_arguments(A "" "PACKAGE" "REQUIRES;AVAILABLE" ${ARGN})
    set(_findings "")
    foreach(_req IN LISTS A_REQUIRES)
        if(NOT _req IN_LIST A_AVAILABLE)
            ergo_pkg_add_finding(_findings PKG-11 error "${A_PACKAGE}"
                "requires '${_req}' which is not selected")
        endif()
    endforeach()
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# PKG-11 (b): a package's cmake.minimum must not exceed the running CMake.
#   ergo_pkg_rule_cmake_minimum(<out> PACKAGE <name> MINIMUM <ver>
#                               CURRENT <ver>)
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_cmake_minimum out_var)
    cmake_parse_arguments(A "" "PACKAGE;MINIMUM;CURRENT" "" ${ARGN})
    set(_findings "")
    if(NOT "${A_MINIMUM}" STREQUAL "" AND "${A_MINIMUM}" VERSION_GREATER "${A_CURRENT}")
        ergo_pkg_add_finding(_findings PKG-11 error "${A_PACKAGE}"
            "needs CMake ${A_MINIMUM} but CMake ${A_CURRENT} is running")
    endif()
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# PKG-12: cycle in the declared graph (package `requires` and feature
# `requires`). EDGES are "<from>><to>" pairs; nodes not listed in NODES are
# added implicitly. Reports one finding naming every node on or behind a cycle
# plus one concrete cycle path. Deterministic: ties break by sorted name.
#   ergo_pkg_rule_cycles(<out> NODES <names...> EDGES <from>><to>...)
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_cycles out_var)
    cmake_parse_arguments(A "" "" "NODES;EDGES" ${ARGN})
    set(_nodes ${A_NODES})
    set(_from_list "")
    set(_to_list "")
    foreach(_edge IN LISTS A_EDGES)
        if(NOT _edge MATCHES "^([^>]+)>([^>]+)$")
            message(FATAL_ERROR "ergo_pkg_rule_cycles: malformed edge '${_edge}' (want <from>><to>)")
        endif()
        list(APPEND _from_list "${CMAKE_MATCH_1}")
        list(APPEND _to_list "${CMAKE_MATCH_2}")
        list(APPEND _nodes "${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
    endforeach()
    list(REMOVE_DUPLICATES _nodes)
    list(SORT _nodes)
    list(LENGTH _from_list _edge_count)

    # Kahn: peel nodes without incoming edges from the remaining set.
    set(_remaining ${_nodes})
    set(_changed TRUE)
    while(_changed)
        set(_changed FALSE)
        foreach(_n IN LISTS _remaining)
            set(_has_incoming FALSE)
            if(_edge_count GREATER 0)
                math(EXPR _last "${_edge_count} - 1")
                foreach(_i RANGE ${_last})
                    list(GET _to_list ${_i} _to)
                    list(GET _from_list ${_i} _from)
                    if(_to STREQUAL _n AND _from IN_LIST _remaining)
                        set(_has_incoming TRUE)
                        break()
                    endif()
                endforeach()
            endif()
            if(NOT _has_incoming)
                list(REMOVE_ITEM _remaining "${_n}")
                set(_changed TRUE)
            endif()
        endforeach()
    endwhile()

    set(_findings "")
    if(_remaining)
        # Every remaining node has a predecessor among the remaining nodes, so
        # walking predecessors must revisit a node: that loop is a cycle.
        list(GET _remaining 0 _cur)
        set(_walk "")
        while(NOT _cur IN_LIST _walk)
            list(APPEND _walk "${_cur}")
            set(_preds "")
            math(EXPR _last "${_edge_count} - 1")
            foreach(_i RANGE ${_last})
                list(GET _to_list ${_i} _to)
                list(GET _from_list ${_i} _from)
                if(_to STREQUAL _cur AND _from IN_LIST _remaining)
                    list(APPEND _preds "${_from}")
                endif()
            endforeach()
            list(SORT _preds)
            list(GET _preds 0 _cur)
        endwhile()
        list(FIND _walk "${_cur}" _start)
        list(SUBLIST _walk ${_start} -1 _loop)
        list(REVERSE _loop)
        list(GET _loop 0 _first)
        list(APPEND _loop "${_first}")
        string(REPLACE ";" " -> " _path "${_loop}")
        string(REPLACE ";" ", " _involved "${_remaining}")
        list(GET _loop 0 _subject)
        ergo_pkg_add_finding(_findings PKG-12 error "${_subject}"
            "declared requires form a cycle: ${_path} (involved: ${_involved})")
    endif()
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()

# Internal: value of "<key>=<v1>,<v2>" entries for <key> as a CMake list.
function(_ergo_pkg_map_get out_var key)
    set(_value "")
    foreach(_entry IN LISTS ARGN)
        if(_entry MATCHES "^([^=]+)=(.*)$" AND CMAKE_MATCH_1 STREQUAL key)
            string(REPLACE "," ";" _value "${CMAKE_MATCH_2}")
            break()
        endif()
    endforeach()
    set(${out_var} "${_value}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Feature selection (the "+web;-vulkan" add/subtract of
# ERGO_PACKAGE_FEATURES_<NAME>). Defaults first: "on" is on, "off" is off,
# "auto" is on when the feature lists no platforms or lists PLATFORM; reserved
# features are always off by default. Then REQUEST tokens apply in order
# ("+f" / "f" adds, "-f" removes). Unknown names, malformed tokens and adding
# a reserved feature are PKG-13 errors (the reserved one stays selected so the
# combination rule reports it too, rather than being silently dropped).
#   ergo_pkg_features_select(<out_selected> <out_findings>
#       PACKAGE <name> PLATFORM <platform> FEATURES <names...>
#       DEFAULTS <f>=<on|off|auto>... PLATFORMS <f>=<p1>,<p2>...
#       RESERVED <names...> REQUEST <tokens...>)
# ---------------------------------------------------------------------------
function(ergo_pkg_features_select out_selected out_findings)
    cmake_parse_arguments(A "" "PACKAGE;PLATFORM" "FEATURES;DEFAULTS;PLATFORMS;RESERVED;REQUEST" ${ARGN})
    set(_selected "")
    set(_findings "")
    foreach(_f IN LISTS A_FEATURES)
        if(_f IN_LIST A_RESERVED)
            continue()
        endif()
        _ergo_pkg_map_get(_default "${_f}" ${A_DEFAULTS})
        if(_default STREQUAL "")
            set(_default auto)
        endif()
        if(_default STREQUAL "on")
            list(APPEND _selected "${_f}")
        elseif(_default STREQUAL "auto")
            _ergo_pkg_map_get(_platforms "${_f}" ${A_PLATFORMS})
            if(NOT _platforms OR A_PLATFORM IN_LIST _platforms)
                list(APPEND _selected "${_f}")
            endif()
        elseif(NOT _default STREQUAL "off")
            ergo_pkg_add_finding(_findings PKG-13 error "${A_PACKAGE}/${_f}"
                "default '${_default}' is not one of on / off / auto")
        endif()
    endforeach()

    foreach(_token IN LISTS A_REQUEST)
        if(_token MATCHES "^([+-]?)([A-Za-z0-9_.-]+)$")
            set(_op "${CMAKE_MATCH_1}")
            set(_f "${CMAKE_MATCH_2}")
        else()
            ergo_pkg_add_finding(_findings PKG-13 error "${A_PACKAGE}"
                "malformed feature token '${_token}' (want +name or -name)")
            continue()
        endif()
        if(NOT _f IN_LIST A_FEATURES)
            string(REPLACE ";" ", " _known "${A_FEATURES}")
            ergo_pkg_add_finding(_findings PKG-13 error "${A_PACKAGE}/${_f}"
                "unknown feature (known: ${_known})")
            continue()
        endif()
        if(_op STREQUAL "-")
            list(REMOVE_ITEM _selected "${_f}")
        else()
            if(_f IN_LIST A_RESERVED)
                ergo_pkg_add_finding(_findings PKG-13 error "${A_PACKAGE}/${_f}"
                    "feature is reserved (name only) and cannot be selected yet")
            endif()
            if(NOT _f IN_LIST _selected)
                list(APPEND _selected "${_f}")
            endif()
        endif()
    endforeach()

    # Keep the declared order so the result does not depend on token order.
    set(_ordered "")
    foreach(_f IN LISTS A_FEATURES)
        if(_f IN_LIST _selected)
            list(APPEND _ordered "${_f}")
        endif()
    endforeach()
    set(${out_selected} "${_ordered}" PARENT_SCOPE)
    set(${out_findings} "${_findings}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# PKG-13 (+ PKG-11 for feature requires): validate a selected feature set.
#   * a selected feature whose `platforms` excludes PLATFORM
#   * two selected features that `conflicts` with each other
#   * a selected reserved feature
#   * REQUIRE_ANY given (e.g. the render features of a package that a
#     real-path module needs) but none of them selected
#   * a selected feature whose `requires` (same-package features) is missing
#   ergo_pkg_rule_feature_combination(<out> PACKAGE <name> PLATFORM <p>
#       SELECTED <names...> PLATFORMS <f>=<p1>,<p2>... RESERVED <names...>
#       CONFLICTS <a>!<b>... REQUIRES <f>=<g1>,<g2>... REQUIRE_ANY <names...>)
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_feature_combination out_var)
    cmake_parse_arguments(A "" "PACKAGE;PLATFORM"
        "SELECTED;PLATFORMS;RESERVED;CONFLICTS;REQUIRES;REQUIRE_ANY" ${ARGN})
    set(_findings "")
    foreach(_f IN LISTS A_SELECTED)
        if(_f IN_LIST A_RESERVED)
            ergo_pkg_add_finding(_findings PKG-13 error "${A_PACKAGE}/${_f}"
                "reserved feature selected")
        endif()
        _ergo_pkg_map_get(_platforms "${_f}" ${A_PLATFORMS})
        if(_platforms AND NOT A_PLATFORM IN_LIST _platforms)
            string(REPLACE ";" ", " _ptext "${_platforms}")
            ergo_pkg_add_finding(_findings PKG-13 error "${A_PACKAGE}/${_f}"
                "not available on platform '${A_PLATFORM}' (platforms: ${_ptext})")
        endif()
        _ergo_pkg_map_get(_reqs "${_f}" ${A_REQUIRES})
        foreach(_r IN LISTS _reqs)
            if(NOT _r IN_LIST A_SELECTED)
                ergo_pkg_add_finding(_findings PKG-11 error "${A_PACKAGE}/${_f}"
                    "requires feature '${_r}' which is not selected")
            endif()
        endforeach()
    endforeach()

    foreach(_pair IN LISTS A_CONFLICTS)
        if(NOT _pair MATCHES "^([^!]+)!([^!]+)$")
            message(FATAL_ERROR "ergo_pkg_rule_feature_combination: malformed conflict '${_pair}' (want <a>!<b>)")
        endif()
        if(CMAKE_MATCH_1 IN_LIST A_SELECTED AND CMAKE_MATCH_2 IN_LIST A_SELECTED)
            ergo_pkg_add_finding(_findings PKG-13 error "${A_PACKAGE}/${CMAKE_MATCH_1}"
                "conflicts with selected feature '${CMAKE_MATCH_2}'")
        endif()
    endforeach()

    if(A_REQUIRE_ANY)
        set(_hit FALSE)
        foreach(_f IN LISTS A_REQUIRE_ANY)
            if(_f IN_LIST A_SELECTED)
                set(_hit TRUE)
            endif()
        endforeach()
        if(NOT _hit)
            string(REPLACE ";" ", " _any "${A_REQUIRE_ANY}")
            ergo_pkg_add_finding(_findings PKG-13 error "${A_PACKAGE}"
                "a render feature is required but none of (${_any}) is selected on '${A_PLATFORM}'")
        endif()
    endif()
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# PKG-14: a module that is ON `needs` a package that is not available. With
# NEED_KIND real-path the need is the real render path (REAL_PATH, i.e. the
# verdict of ergo_render_enforce_backend), not just the target. In MODE M0 the
# hub keeps today's behaviour (ergo_render_enforce_backend decides), so the
# finding is only recorded.
#   ergo_pkg_rule_module_needs(<out> MODULE <name> ENABLED <bool>
#       NEEDS <names...> AVAILABLE <names...> [NEED_KIND real-path]
#       [REAL_PATH <bool>] [REASON <text>] MODE <M0|selected>)
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_module_needs out_var)
    cmake_parse_arguments(A "" "MODULE;ENABLED;NEED_KIND;REAL_PATH;REASON;MODE" "NEEDS;AVAILABLE" ${ARGN})
    set(_findings "")
    if(NOT A_ENABLED)
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()
    if("${A_MODE}" STREQUAL "M0")
        set(_severity record)
    else()
        set(_severity error)
    endif()
    foreach(_need IN LISTS A_NEEDS)
        if(NOT _need IN_LIST A_AVAILABLE)
            ergo_pkg_add_finding(_findings PKG-14 ${_severity} "${A_MODULE}"
                "module is ON but needed package '${_need}' is not available")
        elseif("${A_NEED_KIND}" STREQUAL "real-path" AND NOT A_REAL_PATH)
            set(_why "")
            if(NOT "${A_REASON}" STREQUAL "")
                set(_why " (${A_REASON})")
            endif()
            ergo_pkg_add_finding(_findings PKG-14 ${_severity} "${A_MODULE}"
                "module is ON but the real path through '${_need}' is unavailable${_why}")
        endif()
    endforeach()
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# PKG-15: collisions between hub-managed declarations and the host.
#   CACHE    : HOST_CACHE / WIRING_CACHE "<VAR>=<value>" pairs — the same
#              cache variable forced to different values (warning, STRICT
#              error).
#   TARGETS  : PROVIDED "<package>:<target>" pairs — one target name claimed
#              by two packages (warning, STRICT error).
#   NAMES    : BASE_NAMES vs EXTRA_NAMES — a host extra manifest reusing a
#              name of the public manifest (always an error).
#   ergo_pkg_rule_collisions(<out> [STRICT <bool>]
#       [HOST_CACHE ...] [WIRING_CACHE ...] [PROVIDED ...]
#       [BASE_NAMES ...] [EXTRA_NAMES ...])
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_collisions out_var)
    cmake_parse_arguments(A "" "STRICT" "HOST_CACHE;WIRING_CACHE;PROVIDED;BASE_NAMES;EXTRA_NAMES" ${ARGN})
    set(_findings "")
    _ergo_pkg_strict_severity(_soft "${A_STRICT}")

    foreach(_w IN LISTS A_WIRING_CACHE)
        if(NOT _w MATCHES "^([^=]+)=(.*)$")
            continue()
        endif()
        set(_key "${CMAKE_MATCH_1}")
        set(_wval "${CMAKE_MATCH_2}")
        foreach(_h IN LISTS A_HOST_CACHE)
            if(_h MATCHES "^([^=]+)=(.*)$" AND CMAKE_MATCH_1 STREQUAL _key
               AND NOT CMAKE_MATCH_2 STREQUAL _wval)
                ergo_pkg_add_finding(_findings PKG-15 ${_soft} "${_key}"
                    "cache variable forced to '${CMAKE_MATCH_2}' by the host and '${_wval}' by the package wiring")
            endif()
        endforeach()
    endforeach()

    set(_seen_targets "")
    set(_seen_owners "")
    foreach(_p IN LISTS A_PROVIDED)
        if(NOT _p MATCHES "^([^:]+):(.+)$")
            message(FATAL_ERROR "ergo_pkg_rule_collisions: malformed PROVIDED '${_p}' (want <package>:<target>)")
        endif()
        set(_owner "${CMAKE_MATCH_1}")
        set(_target "${CMAKE_MATCH_2}")
        list(FIND _seen_targets "${_target}" _idx)
        if(_idx GREATER -1)
            list(GET _seen_owners ${_idx} _first)
            if(NOT _first STREQUAL _owner)
                ergo_pkg_add_finding(_findings PKG-15 ${_soft} "${_target}"
                    "target is provided by both '${_first}' and '${_owner}'")
            endif()
        else()
            list(APPEND _seen_targets "${_target}")
            list(APPEND _seen_owners "${_owner}")
        endif()
    endforeach()

    foreach(_n IN LISTS A_EXTRA_NAMES)
        if(_n IN_LIST A_BASE_NAMES)
            ergo_pkg_add_finding(_findings PKG-15 error "${_n}"
                "extra manifest reuses a package name of ergo-packages.json")
        endif()
    endforeach()
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()

include("${CMAKE_CURRENT_LIST_DIR}/ErgoPackageRulesAcquire.cmake")
