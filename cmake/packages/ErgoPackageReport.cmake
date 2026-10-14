# ---------------------------------------------------------------------------
# ErgoPackageReport.cmake — write <build>/ergo-packages.resolved.json and the
# CMake File API query of the package hub
#
# Two responsibilities that both produce build-tree artefacts for the later
# visualisation stages (spec/feature/package-hub.md §5):
#   * ergo_pkg_report_render(<out> <prefix>)   facts → JSON text (pure)
#   * ergo_pkg_report_write(<file> <json>)      JSON text → file, once
#   * ergo_pkg_report_install_query(<build_dir>) client-ergo codemodel v2
#
# Determinism: the JSON carries no time stamps, every object key and every
# array is emitted in sorted order, and the file is written with
# file(CONFIGURE), which leaves the file untouched when the bytes are equal.
# The same configuration therefore yields the same bytes (and mtime).
#
# Input variables read by ergo_pkg_report_render (all optional, "" = absent):
#   <p>_MODE <p>_STRICT <p>_FETCH <p>_PLATFORM <p>_CMAKE_VERSION <p>_MANIFEST
#   <p>_MANIFEST_EXTRA <p>_SELECTED <p>_FINDINGS <p>_HOST_ID
#   with a host declaration (<p>_HOST_DECLARED): <p>_HOST_{SELECTED_FROM,
#       OPTIONAL,OPTIONAL_FROM,UNUSED,FEATURES_FROM ("<pkg>=<from>" pairs)}
#       and Ergo's pin <p>_HOST_ERGO_{PIN,EXPECT_TAG,ROUTE,HEAD,DRIFT,DIRTY}
#   <p>_PACKAGES, per package <p>_PKG_<n>_{KIND,CODE,GROUP,PIN,EXPECT_TAG,
#       OVERRIDE,STATE,SOURCE_DIR,IMPORTED,TARGETS_PRESENT,FEATURES,REQUIRES,
#       VERSION,ROUTE,MODE,HEAD,DRIFT,DIRTY,EXPORTS,PATH,GITLINK,INITIALIZED}
#   <p>_MODULES, per module <p>_MOD_<m>_{ENABLED,NEEDS,OPTIONAL,PORTED_FROM,
#       REASON}
#   <p>_OBSERVE, per entry <p>_OBS_<o>_{CODE,GROUP,PATH_HINT,RELATIONS}
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

# Internal: JSON string literal for <value>.
function(_ergo_pkg_json_quote out_var value)
    string(REPLACE "\\" "\\\\" value "${value}")
    string(REPLACE "\"" "\\\"" value "${value}")
    string(REPLACE "\n" "\\n" value "${value}")
    string(REPLACE "\r" "\\r" value "${value}")
    string(REPLACE "\t" "\\t" value "${value}")
    set(${out_var} "\"${value}\"" PARENT_SCOPE)
endfunction()

# Internal: JSON string literal, or null for "".
function(_ergo_pkg_json_nullable out_var value)
    if(value STREQUAL "")
        set(${out_var} "null" PARENT_SCOPE)
    else()
        _ergo_pkg_json_quote(_q "${value}")
        set(${out_var} "${_q}" PARENT_SCOPE)
    endif()
endfunction()

# Internal: JSON boolean (CMake truthiness).
function(_ergo_pkg_json_bool out_var value)
    if(value)
        set(${out_var} "true" PARENT_SCOPE)
    else()
        set(${out_var} "false" PARENT_SCOPE)
    endif()
endfunction()

# Internal: sorted JSON array of strings from the remaining arguments.
function(_ergo_pkg_json_list out_var)
    set(_items ${ARGN})
    list(REMOVE_DUPLICATES _items)
    list(SORT _items)
    set(_parts "")
    foreach(_i IN LISTS _items)
        _ergo_pkg_json_quote(_q "${_i}")
        list(APPEND _parts "${_q}")
    endforeach()
    list(JOIN _parts ", " _joined)
    set(${out_var} "[${_joined}]" PARENT_SCOPE)
endfunction()

# Internal: "{ k: v, ... }" body from "<key>=<json value>" pairs (keys never
# contain "="), sorted, at the given indentation.
function(_ergo_pkg_json_object out_var indent)
    set(_pairs ${ARGN})
    list(SORT _pairs)
    if(NOT _pairs)
        set(${out_var} "{}" PARENT_SCOPE)
        return()
    endif()
    set(_lines "")
    foreach(_pair IN LISTS _pairs)
        string(FIND "${_pair}" "=" _cut)
        string(SUBSTRING "${_pair}" 0 ${_cut} _key)
        math(EXPR _vstart "${_cut} + 1")
        string(SUBSTRING "${_pair}" ${_vstart} -1 _value)
        _ergo_pkg_json_quote(_qk "${_key}")
        list(APPEND _lines "${indent}  ${_qk}: ${_value}")
    endforeach()
    list(JOIN _lines ",\n" _body)
    set(${out_var} "{\n${_body}\n${indent}}" PARENT_SCOPE)
endfunction()

# Internal: append "<key>=<json>" to the list variable <list_var>. These are
# functions, not macros: macro arguments are substituted as text and would
# re-interpret the backslash escapes of already-quoted JSON.
function(_ergo_pkg_kv list_var key json)
    set(_list ${${list_var}})
    list(APPEND _list "${key}=${json}")
    set(${list_var} "${_list}" PARENT_SCOPE)
endfunction()

# Internal: typed field appenders (<list_var> <key> <value...>).
function(_ergo_pkg_add_quote list_var key value)
    _ergo_pkg_json_quote(_v "${value}")
    _ergo_pkg_kv(${list_var} ${key} "${_v}")
    set(${list_var} "${${list_var}}" PARENT_SCOPE)
endfunction()
function(_ergo_pkg_add_nullable list_var key value)
    _ergo_pkg_json_nullable(_v "${value}")
    _ergo_pkg_kv(${list_var} ${key} "${_v}")
    set(${list_var} "${${list_var}}" PARENT_SCOPE)
endfunction()
function(_ergo_pkg_add_bool list_var key value)
    _ergo_pkg_json_bool(_v "${value}")
    _ergo_pkg_kv(${list_var} ${key} "${_v}")
    set(${list_var} "${${list_var}}" PARENT_SCOPE)
endfunction()
function(_ergo_pkg_add_list list_var key)
    _ergo_pkg_json_list(_v ${ARGN})
    _ergo_pkg_kv(${list_var} ${key} "${_v}")
    set(${list_var} "${${list_var}}" PARENT_SCOPE)
endfunction()

function(ergo_pkg_report_render out_var p)
    # --- packages -----------------------------------------------------------
    set(_pkg_pairs "")
    set(_names ${${p}_PACKAGES})
    list(SORT _names)
    foreach(_n IN LISTS _names)
        set(_q "${p}_PKG_${_n}")
        set(_f "")
        _ergo_pkg_add_nullable(_f kind "${${_q}_KIND}")
        _ergo_pkg_add_nullable(_f code "${${_q}_CODE}")
        _ergo_pkg_add_nullable(_f group "${${_q}_GROUP}")
        _ergo_pkg_add_nullable(_f state "${${_q}_STATE}")
        if(${_q}_KIND STREQUAL "tool")
            _ergo_pkg_add_nullable(_f path "${${_q}_PATH}")
            _ergo_pkg_add_nullable(_f gitlink "${${_q}_GITLINK}")
            _ergo_pkg_add_bool(_f initialized "${${_q}_INITIALIZED}")
        else()
            _ergo_pkg_add_nullable(_f pin "${${_q}_PIN}")
            _ergo_pkg_add_nullable(_f expect_tag "${${_q}_EXPECT_TAG}")
            _ergo_pkg_add_nullable(_f rev_override "${${_q}_OVERRIDE}")
            _ergo_pkg_add_nullable(_f source_dir "${${_q}_SOURCE_DIR}")
            _ergo_pkg_add_bool(_f imported "${${_q}_IMPORTED}")
            _ergo_pkg_add_list(_f targets_present ${${_q}_TARGETS_PRESENT})
            _ergo_pkg_add_list(_f features ${${_q}_FEATURES})
            _ergo_pkg_add_list(_f requires ${${_q}_REQUIRES})
            _ergo_pkg_add_nullable(_f version "${${_q}_VERSION}")
            _ergo_pkg_add_nullable(_f route "${${_q}_ROUTE}")
            _ergo_pkg_add_nullable(_f mode "${${_q}_MODE}")
            _ergo_pkg_add_nullable(_f head "${${_q}_HEAD}")
            _ergo_pkg_add_nullable(_f drift "${${_q}_DRIFT}")
            _ergo_pkg_add_bool(_f dirty "${${_q}_DIRTY}")
            _ergo_pkg_add_list(_f exports ${${_q}_EXPORTS})
        endif()
        _ergo_pkg_json_object(_obj "    " ${_f})
        _ergo_pkg_kv(_pkg_pairs "${_n}" "${_obj}")
    endforeach()
    _ergo_pkg_json_object(_packages_json "  " ${_pkg_pairs})

    # --- modules ------------------------------------------------------------
    set(_mod_pairs "")
    set(_names ${${p}_MODULES})
    list(SORT _names)
    foreach(_m IN LISTS _names)
        set(_q "${p}_MOD_${_m}")
        set(_f "")
        _ergo_pkg_add_bool(_f enabled "${${_q}_ENABLED}")
        _ergo_pkg_add_list(_f needs ${${_q}_NEEDS})
        _ergo_pkg_add_list(_f optional ${${_q}_OPTIONAL})
        _ergo_pkg_add_list(_f ported_from ${${_q}_PORTED_FROM})
        _ergo_pkg_add_nullable(_f reason "${${_q}_REASON}")
        _ergo_pkg_json_object(_obj "    " ${_f})
        _ergo_pkg_kv(_mod_pairs "${_m}" "${_obj}")
    endforeach()
    _ergo_pkg_json_object(_modules_json "  " ${_mod_pairs})

    # --- observe ------------------------------------------------------------
    set(_obs_pairs "")
    set(_names ${${p}_OBSERVE})
    list(SORT _names)
    foreach(_o IN LISTS _names)
        set(_q "${p}_OBS_${_o}")
        set(_f "")
        _ergo_pkg_add_nullable(_f code "${${_q}_CODE}")
        _ergo_pkg_add_nullable(_f group "${${_q}_GROUP}")
        _ergo_pkg_add_nullable(_f path_hint "${${_q}_PATH_HINT}")
        _ergo_pkg_add_list(_f relations ${${_q}_RELATIONS})
        _ergo_pkg_json_object(_obj "    " ${_f})
        _ergo_pkg_kv(_obs_pairs "${_o}" "${_obj}")
    endforeach()
    _ergo_pkg_json_object(_observe_json "  " ${_obs_pairs})

    # --- findings (sorted; "<id>|<severity>|<subject>|<message>") ----------
    set(_findings ${${p}_FINDINGS})
    list(REMOVE_DUPLICATES _findings)
    list(SORT _findings)
    set(_finding_lines "")
    foreach(_line IN LISTS _findings)
        string(REPLACE "|" ";" _parts "${_line}")
        list(LENGTH _parts _count)
        if(_count LESS 4)
            message(FATAL_ERROR "ergo_pkg_report_render: malformed finding '${_line}'")
        endif()
        list(GET _parts 0 _id)
        list(GET _parts 1 _severity)
        list(GET _parts 2 _subject)
        list(SUBLIST _parts 3 -1 _rest)
        list(JOIN _rest "|" _message)
        set(_f "")
        _ergo_pkg_add_quote(_f id "${_id}")
        _ergo_pkg_add_quote(_f severity "${_severity}")
        _ergo_pkg_add_quote(_f subject "${_subject}")
        _ergo_pkg_add_quote(_f message "${_message}")
        _ergo_pkg_json_object(_obj "    " ${_f})
        list(APPEND _finding_lines "    ${_obj}")
    endforeach()
    if(_finding_lines)
        list(JOIN _finding_lines ",\n" _fbody)
        set(_findings_json "[\n${_fbody}\n  ]")
    else()
        set(_findings_json "[]")
    endif()

    # --- top level ----------------------------------------------------------
    set(_top "")
    _ergo_pkg_kv(_top schema "1")
    set(_host "")
    _ergo_pkg_add_nullable(_host id "${${p}_HOST_ID}")
    if(${p}_HOST_DECLARED)
        # Where the configure defaults came from (cache overrides stay
        # visible) and Ergo itself against the declared pin.
        _ergo_pkg_add_nullable(_host selected_from "${${p}_HOST_SELECTED_FROM}")
        _ergo_pkg_add_list(_host optional ${${p}_HOST_OPTIONAL})
        _ergo_pkg_add_nullable(_host optional_from "${${p}_HOST_OPTIONAL_FROM}")
        _ergo_pkg_add_list(_host unused ${${p}_HOST_UNUSED})
        set(_ff "")
        foreach(_pair IN LISTS ${p}_HOST_FEATURES_FROM)
            if(_pair MATCHES "^([^=]+)=(.*)$")
                _ergo_pkg_add_quote(_ff "${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
            endif()
        endforeach()
        _ergo_pkg_json_object(_ff_json "    " ${_ff})
        _ergo_pkg_kv(_host features_from "${_ff_json}")
        if("${${p}_HOST_ERGO_PIN}" STREQUAL "")
            _ergo_pkg_kv(_host ergo "null")
        else()
            set(_e "")
            _ergo_pkg_add_nullable(_e pin "${${p}_HOST_ERGO_PIN}")
            _ergo_pkg_add_nullable(_e expect_tag "${${p}_HOST_ERGO_EXPECT_TAG}")
            _ergo_pkg_add_nullable(_e route "${${p}_HOST_ERGO_ROUTE}")
            _ergo_pkg_add_nullable(_e head "${${p}_HOST_ERGO_HEAD}")
            _ergo_pkg_add_nullable(_e drift "${${p}_HOST_ERGO_DRIFT}")
            _ergo_pkg_add_bool(_e dirty "${${p}_HOST_ERGO_DIRTY}")
            _ergo_pkg_json_object(_e_json "    " ${_e})
            _ergo_pkg_kv(_host ergo "${_e_json}")
        endif()
    endif()
    _ergo_pkg_json_object(_host_json "  " ${_host})
    _ergo_pkg_kv(_top host "${_host_json}")
    _ergo_pkg_add_nullable(_top mode "${${p}_MODE}")
    _ergo_pkg_add_bool(_top strict "${${p}_STRICT}")
    _ergo_pkg_add_bool(_top fetch "${${p}_FETCH}")
    _ergo_pkg_add_nullable(_top manifest_extra "${${p}_MANIFEST_EXTRA}")
    _ergo_pkg_add_nullable(_top platform "${${p}_PLATFORM}")
    _ergo_pkg_add_nullable(_top cmake_version "${${p}_CMAKE_VERSION}")
    _ergo_pkg_add_nullable(_top manifest "${${p}_MANIFEST}")
    _ergo_pkg_add_list(_top selected ${${p}_SELECTED})
    _ergo_pkg_kv(_top packages "${_packages_json}")
    _ergo_pkg_kv(_top modules "${_modules_json}")
    _ergo_pkg_kv(_top observe "${_observe_json}")
    _ergo_pkg_kv(_top findings "${_findings_json}")
    _ergo_pkg_json_object(_doc "" ${_top})
    set(${out_var} "${_doc}\n" PARENT_SCOPE)
endfunction()

# ergo_pkg_report_write(<file> <json>)
# file(CONFIGURE) rewrites only when the content changed. The JSON goes in
# through a variable reference so '@' / '${' inside values are never expanded.
function(ergo_pkg_report_write file json)
    set(_ERGO_PKG_REPORT_CONTENT "${json}")
    file(CONFIGURE OUTPUT "${file}" CONTENT "@_ERGO_PKG_REPORT_CONTENT@" @ONLY)
endfunction()

# ergo_pkg_report_install_query(<build_dir>)
# Stateless-client query so the next generate step answers codemodel v2 for
# the visualisation stage (ergo_package_model, S3). Only adds files under
# <build_dir>/.cmake/api; it does not change the generated build.
function(ergo_pkg_report_install_query build_dir)
    set(_ERGO_PKG_QUERY_CONTENT
        "{\n  \"requests\": [\n    { \"kind\": \"codemodel\", \"version\": 2 }\n  ]\n}\n")
    file(CONFIGURE
        OUTPUT "${build_dir}/.cmake/api/v1/query/client-ergo/query.json"
        CONTENT "@_ERGO_PKG_QUERY_CONTENT@" @ONLY)
endfunction()
