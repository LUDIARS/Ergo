# ---------------------------------------------------------------------------
# ErgoPackageHostDefaults.cmake — the host declaration's configure defaults
#
# One responsibility: decide which value a hub switch takes when the host
# declaration (`host` section, spec/feature/package-manager.md §2.1) and the
# cache both may give one. Pure functions over list variables, like
# ErgoPackageRules.cmake, so tests/packages/test_host_defaults.cmake runs
# them under `cmake -P`.
#
#   host.use       -> default of ERGO_PACKAGES
#   host.optional  -> default of ERGO_PACKAGES_OPTIONAL (selected packages
#                     no route resolves are not used instead of PKG-03)
#   host.features  -> default of ERGO_PACKAGE_FEATURES_<NAME>
#   host.legacy_vars "<VAR>": "<name>.source_dir" -> <VAR> stands in for an
#                     unset ERGO_PACKAGE_SOURCE_DIR_<NAME> (DEPRECATION)
#
# A non-empty cache value always wins and is recorded as "cache"; an empty
# one takes the declaration ("declaration"); without a declaration the
# value is the cache's ("cache" when non-empty, "none" otherwise).
# ---------------------------------------------------------------------------
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/ErgoPackageRules.cmake")

# ergo_pkg_host_default(<out_value> <out_from> DECLARED <bool>
#                       CACHE <values...> DEFAULT <values...>)
function(ergo_pkg_host_default out_value out_from)
    cmake_parse_arguments(A "" "DECLARED" "CACHE;DEFAULT" ${ARGN})
    if(NOT "${A_CACHE}" STREQUAL "")
        set(_value ${A_CACHE})
        set(_from cache)
    elseif(A_DECLARED)
        set(_value ${A_DEFAULT})
        set(_from declaration)
    else()
        set(_value "")
        set(_from none)
    endif()
    set(${out_value} "${_value}" PARENT_SCOPE)
    set(${out_from} "${_from}" PARENT_SCOPE)
endfunction()

# ergo_pkg_host_feature_default(<out_tokens> PACKAGE <name> PAIRS <pairs...>)
# Feature request tokens the declaration gives <name> ("" when none). PAIRS
# are "<package>=<token>,<token>" as ergo_pkg_manifest_load(... HOST) reads
# host.features ("+vulkan,-web" and "vulkan;web" both arrive comma-joined).
function(ergo_pkg_host_feature_default out_tokens)
    cmake_parse_arguments(A "" "PACKAGE" "PAIRS" ${ARGN})
    set(_tokens "")
    foreach(_pair IN LISTS A_PAIRS)
        if(_pair MATCHES "^([^=]+)=(.*)$" AND CMAKE_MATCH_1 STREQUAL A_PACKAGE)
            string(REPLACE "," ";" _split "${CMAKE_MATCH_2}")
            foreach(_t IN LISTS _split)
                string(STRIP "${_t}" _t)
                if(NOT _t STREQUAL "")
                    list(APPEND _tokens "${_t}")
                endif()
            endforeach()
        endif()
    endforeach()
    set(${out_tokens} "${_tokens}" PARENT_SCOPE)
endfunction()

# ergo_pkg_host_legacy_source(<out_var_name> PACKAGE <name> PAIRS <pairs...>)
# Name of the legacy variable host.legacy_vars maps to "<name>.source_dir"
# ("" when none). PAIRS are "<VAR>=<target>" (first match in sorted order).
function(ergo_pkg_host_legacy_source out_var)
    cmake_parse_arguments(A "" "PACKAGE" "PAIRS" ${ARGN})
    set(_name "")
    foreach(_pair IN LISTS A_PAIRS)
        if(_name STREQUAL "" AND _pair MATCHES "^([^=]+)=(.*)$"
           AND CMAKE_MATCH_2 STREQUAL "${A_PACKAGE}.source_dir")
            set(_name "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    set(${out_var} "${_name}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# An optional package (host.optional / ERGO_PACKAGES_OPTIONAL) that no route
# resolved — or that was declared provided and never created — is not used.
# Recorded instead of the PKG-03 error.
#   ergo_pkg_rule_optional_unused(<out> PACKAGE <name> TRIED <routes...>)
# ---------------------------------------------------------------------------
function(ergo_pkg_rule_optional_unused out_var)
    cmake_parse_arguments(A "" "PACKAGE" "TRIED" ${ARGN})
    string(REPLACE ";" ", " _tried "${A_TRIED}")
    set(_findings "")
    ergo_pkg_add_finding(_findings PKG-03 record "${A_PACKAGE}"
        "optional package not found (tried: ${_tried}): not used")
    set(${out_var} "${_findings}" PARENT_SCOPE)
endfunction()
