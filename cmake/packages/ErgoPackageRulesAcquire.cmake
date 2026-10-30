# ---------------------------------------------------------------------------
# ErgoPackageRulesAcquire.cmake — pure check rules of the acquisition stage
#
# Same contract as ErgoPackageRules.cmake (included from there): list
# variables in, findings "<id>|<severity>|<subject>|<message>" out. The live
# facts (git HEAD, ancestry, target presence) are probed elsewhere
# (ErgoPackageGit.cmake / ErgoPackageResolve.cmake) and passed in, so every
# rule here runs under `cmake -P` (tests/packages/).
#
# Rules: PKG-03/04/05/07/08/09/10/16 (spec/feature/package-hub.md §4) and
# the drift classification none / ahead / behind / diverged / unknown.
# ---------------------------------------------------------------------------
include_guard(GLOBAL)
cmake_policy(SET CMP0057 NEW)  # if(IN_LIST) also under cmake -P, where no policy version is set

# ---------------------------------------------------------------------------
# PKG-03: a selected package resolved through none of the routes. Always an
# error; the message lists the routes tried and how to point at a source.
#   ergo_pkg_rule_unresolved(<out> PACKAGE <name> TRIED <routes...>
#                            [HINT <text>])
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_unresolved out_var)
    cmake_parse_arguments(A "" "PACKAGE;HINT" "TRIED" ${ARGN})
    string(TOUPPER "${A_PACKAGE}" _upper)
    string(REPLACE "-" "_" _upper "${_upper}")
    string(REPLACE ";" ", " _tried "${A_TRIED}")
    set(_text "no route resolved the package (tried: ${_tried}). Point ERGO_PACKAGE_SOURCE_DIR_${_upper} at a checkout, set ERGO_PACKAGES_SIBLING_ROOT, create the target before Ergo, or allow ERGO_PACKAGES_FETCH=ON")
    if(NOT "${A_HINT}" STREQUAL "")
        set(_text "${_text} (${A_HINT})")
    endif()
    set(_findings "")
    ergo_pkg_add_finding(_findings PKG-03 error "${A_PACKAGE}" "${_text}")
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# PKG-04: after a fetch the checked-out HEAD must equal the pin (the hub takes
# over the SHA verification hosts used to do themselves). Always an error.
#   ergo_pkg_rule_fetched_head(<out> PACKAGE <name> PIN <sha> HEAD <sha>)
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_fetched_head out_var)
    cmake_parse_arguments(A "" "PACKAGE;PIN;HEAD" "" ${ARGN})
    set(_findings "")
    if(NOT "${A_HEAD}" STREQUAL "${A_PIN}")
        set(_head "${A_HEAD}")
        if(_head STREQUAL "")
            set(_head "(unreadable)")
        endif()
        ergo_pkg_add_finding(_findings PKG-04 error "${A_PACKAGE}"
            "fetched HEAD ${_head} is not the pin ${A_PIN}")
    endif()
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Drift of a checkout against its pin:
#   none      HEAD == pin
#   ahead     pin is an ancestor of HEAD
#   behind    HEAD is an ancestor of pin
#   diverged  both commits are known and neither contains the other
#   unknown   HEAD unreadable (no git / not a repository) or the pin commit
#             is not in the checkout, so the relation cannot be decided
#   ergo_pkg_drift_classify(<out> HEAD <sha> PIN <sha> PIN_KNOWN <bool>
#                           PIN_IS_ANCESTOR <bool> HEAD_IS_ANCESTOR <bool>)
# ---------------------------------------------------------------------------
function(ergo_pkg_drift_classify out_var)
    cmake_parse_arguments(A "" "HEAD;PIN;PIN_KNOWN;PIN_IS_ANCESTOR;HEAD_IS_ANCESTOR" "" ${ARGN})
    if("${A_HEAD}" STREQUAL "")
        set(_drift unknown)
    elseif("${A_HEAD}" STREQUAL "${A_PIN}")
        set(_drift none)
    elseif(NOT A_PIN_KNOWN)
        set(_drift unknown)
    elseif(A_PIN_IS_ANCESTOR)
        set(_drift ahead)
    elseif(A_HEAD_IS_ANCESTOR)
        set(_drift behind)
    else()
        set(_drift diverged)
    endif()
    set(${out_var} ${_drift} PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# PKG-05 / PKG-09: drift of a host-provided / local / sibling checkout.
#   * ahead / behind / diverged, or unknown with a readable HEAD (pin commit
#     missing from the checkout): PKG-05, warning (error under STRICT)
#   * unknown without a readable HEAD (no git, not a repository): PKG-09,
#     warning (error under STRICT)
#   * ROUTE installed (find_package): the SHA cannot be read at all, only the
#     version; recorded as PKG-09 "record" (the route is unused under STRICT)
# The fetch route is checked by PKG-04 instead and yields nothing here.
#   ergo_pkg_rule_drift(<out> PACKAGE <name> ROUTE <route> DRIFT <drift>
#                       HEAD <sha> PIN <sha> [VERSION <v>] [STRICT <bool>])
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_drift out_var)
    cmake_parse_arguments(A "" "PACKAGE;ROUTE;DRIFT;HEAD;PIN;VERSION;STRICT" "" ${ARGN})
    set(_findings "")
    _ergo_pkg_strict_severity(_soft "${A_STRICT}")
    if("${A_ROUTE}" STREQUAL "fetch")
        # PKG-04 owns the fetched checkout.
    elseif("${A_ROUTE}" STREQUAL "installed")
        set(_version "${A_VERSION}")
        if(_version STREQUAL "")
            set(_version "unversioned")
        endif()
        ergo_pkg_add_finding(_findings PKG-09 record "${A_PACKAGE}"
            "installed package (find_package): version only (${_version}), SHA against pin ${A_PIN} not verified")
    elseif("${A_DRIFT}" MATCHES "^(ahead|behind|diverged)$")
        ergo_pkg_add_finding(_findings PKG-05 ${_soft} "${A_PACKAGE}"
            "checkout HEAD ${A_HEAD} is ${A_DRIFT} of the pin ${A_PIN}")
    elseif("${A_DRIFT}" STREQUAL "unknown")
        if("${A_HEAD}" STREQUAL "")
            ergo_pkg_add_finding(_findings PKG-09 ${_soft} "${A_PACKAGE}"
                "git is unavailable or the source is not a git checkout: HEAD unknown, pin ${A_PIN} not verified")
        else()
            ergo_pkg_add_finding(_findings PKG-05 ${_soft} "${A_PACKAGE}"
                "checkout HEAD ${A_HEAD} differs from the pin ${A_PIN} (pin commit not in the checkout: drift unknown)")
        endif()
    endif()
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# PKG-07: uncommitted changes in the checkout that was built. Recorded; an
# error under STRICT.
#   ergo_pkg_rule_dirty(<out> PACKAGE <name> DIRTY <bool> [STRICT <bool>])
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_dirty out_var)
    cmake_parse_arguments(A "" "PACKAGE;DIRTY;STRICT" "" ${ARGN})
    set(_findings "")
    if(A_DIRTY)
        if(A_STRICT)
            set(_severity error)
        else()
            set(_severity record)
        endif()
        ergo_pkg_add_finding(_findings PKG-07 ${_severity} "${A_PACKAGE}"
            "checkout has uncommitted changes (dirty)")
    endif()
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# PKG-08: `expect_tag` is present in the checkout but the pin does not descend
# from it. Warning; an error under STRICT. A tag the checkout lacks (shallow
# fetch, no tags) is not judged.
#   ergo_pkg_rule_expect_tag(<out> PACKAGE <name> TAG <tag> TAG_FOUND <bool>
#                            PIN_DESCENDS <bool> PIN <sha> [STRICT <bool>])
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_expect_tag out_var)
    cmake_parse_arguments(A "" "PACKAGE;TAG;TAG_FOUND;PIN_DESCENDS;PIN;STRICT" "" ${ARGN})
    set(_findings "")
    if(NOT "${A_TAG}" STREQUAL "" AND A_TAG_FOUND AND NOT A_PIN_DESCENDS)
        _ergo_pkg_strict_severity(_severity "${A_STRICT}")
        ergo_pkg_add_finding(_findings PKG-08 ${_severity} "${A_PACKAGE}"
            "pin ${A_PIN} does not descend from expect_tag ${A_TAG}")
    endif()
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Targets a resolved package must have: every `cmake.targets` entry that no
# feature claims in `provides`, plus the `provides` of the selected features.
# A target claimed by a feature (pictor by vulkan) is required only with
# that feature, so platforms where the feature is off (emscripten) pass.
#   ergo_pkg_required_targets(<out> TARGETS <names...> SELECTED <features...>
#                             PROVIDES <f>=<t1>,<t2>...)
# ---------------------------------------------------------------------------
function(ergo_pkg_required_targets out_var)
    cmake_parse_arguments(A "" "" "TARGETS;SELECTED;PROVIDES" ${ARGN})
    set(_claimed "")
    set(_required "")
    foreach(_entry IN LISTS A_PROVIDES)
        if(NOT _entry MATCHES "^([^=]+)=(.*)$")
            continue()
        endif()
        set(_feature "${CMAKE_MATCH_1}")
        string(REPLACE "," ";" _targets "${CMAKE_MATCH_2}")
        list(APPEND _claimed ${_targets})
        if(_feature IN_LIST A_SELECTED)
            list(APPEND _required ${_targets})
        endif()
    endforeach()
    foreach(_t IN LISTS A_TARGETS)
        if(NOT _t IN_LIST _claimed)
            list(APPEND _required "${_t}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _required)
    set(${out_var} "${_required}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# PKG-10: required targets and feature contracts after resolution. A missing
# target, or a contract compile definition the target does not publish, is
# an error on every route (host-provided declaration violation, or a layout
# change of a fetched / local source).
#   ergo_pkg_rule_layout(<out> PACKAGE <name> ROUTE <route>
#       REQUIRED <targets...> PRESENT <targets...>
#       CONTRACTS <feature>:<target>:<DEF>... HONOURED <feature>:<target>:<DEF>...)
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_layout out_var)
    cmake_parse_arguments(A "" "PACKAGE;ROUTE" "REQUIRED;PRESENT;CONTRACTS;HONOURED" ${ARGN})
    set(_findings "")
    if("${A_ROUTE}" MATCHES "^(host|provided)$")
        set(_why "host-provided package violates the manifest")
    else()
        set(_why "package layout differs from the manifest")
    endif()
    foreach(_t IN LISTS A_REQUIRED)
        if(NOT _t IN_LIST A_PRESENT)
            ergo_pkg_add_finding(_findings PKG-10 error "${A_PACKAGE}"
                "${_why}: target '${_t}' is missing (route ${A_ROUTE})")
        endif()
    endforeach()
    foreach(_c IN LISTS A_CONTRACTS)
        if(NOT _c MATCHES "^([^:]+):(.+):([^:]+)$")
            message(FATAL_ERROR "ergo_pkg_rule_layout: malformed contract '${_c}' (want <feature>:<target>:<DEF>)")
        endif()
        set(_feature "${CMAKE_MATCH_1}")
        set(_target "${CMAKE_MATCH_2}")
        set(_def "${CMAKE_MATCH_3}")
        if(_target IN_LIST A_PRESENT AND NOT _c IN_LIST A_HONOURED)
            ergo_pkg_add_finding(_findings PKG-10 error "${A_PACKAGE}/${_feature}"
                "${_why}: target '${_target}' does not publish the contract definition ${_def} (route ${A_ROUTE})")
        endif()
    endforeach()
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# PKG-16: a tool package's submodule is not initialised. Warning (the C++
# build does not need it); an error under STRICT.
#   ergo_pkg_rule_submodule(<out> PACKAGE <name> PATH <path>
#                           INITIALIZED <bool> [STRICT <bool>])
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_submodule out_var)
    cmake_parse_arguments(A "" "PACKAGE;PATH;INITIALIZED;STRICT" "" ${ARGN})
    set(_findings "")
    if(NOT A_INITIALIZED)
        _ergo_pkg_strict_severity(_severity "${A_STRICT}")
        ergo_pkg_add_finding(_findings PKG-16 ${_severity} "${A_PACKAGE}"
            "tool submodule '${A_PATH}' is not initialised (git submodule update --init ${A_PATH})")
    endif()
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()
