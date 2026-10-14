# ---------------------------------------------------------------------------
# ErgoPackageManifest.cmake — read ergo-packages.json into CMake variables
#
# One responsibility: turn the manifest (schema 1, spec/data/ergo-packages.md)
# into flat list variables that the pure rules (ErgoPackageRules.cmake) and
# the report (ErgoPackageReport.cmake) consume. It only reads the given file;
# it creates no targets and sets no cache entries.
#
#   ergo_pkg_manifest_load(<prefix> <file>)
#
# Sets in the caller's scope (names are sorted):
#   <prefix>_SCHEMA, <prefix>_ERRORS (schema problems, one text per entry)
#   <prefix>_PACKAGES / _LINK / _TOOLS / _MODULES / _OBSERVE
#   per package  <prefix>_PKG_<n>_{KIND,CODE,REPOSITORY,REV,EXPECT_TAG,
#                PLATFORMS,CMAKE_MINIMUM,TARGETS,FIND_PACKAGE,REQUIRES,
#                FEATURES,CACHE,ACQUIRE,PATH}
#   per feature  <prefix>_PKG_<n>_FEAT_<f>_{DEFAULT,PLATFORMS,RESERVED,
#                PROVIDES,CONTRACT,REQUIRES,CONFLICTS}
#   per module   <prefix>_MOD_<m>_{NEEDS,OPTIONAL,NEED_KIND,WHEN,PORTED_FROM,
#                REAL_PATH_VAR,REASON_VAR}
#   per observe  <prefix>_OBS_<o>_{CODE,PATH_HINT,GROUP,RELATIONS}
#                (RELATIONS entries are "<from>:<kind>")
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

# Internal: string member at <path...>, or "" when absent / null.
function(_ergo_pkg_json_string out_var json)
    string(JSON _type ERROR_VARIABLE _err TYPE "${json}" ${ARGN})
    if(_err OR _type STREQUAL "NULL")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()
    string(JSON _value GET "${json}" ${ARGN})
    set(${out_var} "${_value}" PARENT_SCOPE)
endfunction()

# Internal: array of strings at <path...> as a CMake list ("" when absent).
function(_ergo_pkg_json_array out_var json)
    set(_items "")
    string(JSON _type ERROR_VARIABLE _err TYPE "${json}" ${ARGN})
    if(NOT _err AND _type STREQUAL "ARRAY")
        string(JSON _len LENGTH "${json}" ${ARGN})
        if(_len GREATER 0)
            math(EXPR _last "${_len} - 1")
            foreach(_i RANGE ${_last})
                string(JSON _item GET "${json}" ${ARGN} ${_i})
                list(APPEND _items "${_item}")
            endforeach()
        endif()
    endif()
    set(${out_var} "${_items}" PARENT_SCOPE)
endfunction()

# Internal: sorted member names of the object at <path...> ("" when absent).
function(_ergo_pkg_json_keys out_var json)
    set(_keys "")
    string(JSON _type ERROR_VARIABLE _err TYPE "${json}" ${ARGN})
    if(NOT _err AND _type STREQUAL "OBJECT")
        string(JSON _len LENGTH "${json}" ${ARGN})
        if(_len GREATER 0)
            math(EXPR _last "${_len} - 1")
            foreach(_i RANGE ${_last})
                string(JSON _key MEMBER "${json}" ${ARGN} ${_i})
                list(APPEND _keys "${_key}")
            endforeach()
        endif()
    endif()
    list(SORT _keys)
    set(${out_var} "${_keys}" PARENT_SCOPE)
endfunction()

# Internal: object at <path...> as "<key>=<value>" pairs (sorted by key).
function(_ergo_pkg_json_pairs out_var json)
    _ergo_pkg_json_keys(_keys "${json}" ${ARGN})
    set(_pairs "")
    foreach(_k IN LISTS _keys)
        _ergo_pkg_json_string(_v "${json}" ${ARGN} "${_k}")
        list(APPEND _pairs "${_k}=${_v}")
    endforeach()
    set(${out_var} "${_pairs}" PARENT_SCOPE)
endfunction()

function(ergo_pkg_manifest_load prefix file)
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "ergo packages: manifest '${file}' does not exist")
    endif()
    file(READ "${file}" _json)
    string(JSON _type ERROR_VARIABLE _err TYPE "${_json}")
    if(_err OR NOT _type STREQUAL "OBJECT")
        message(FATAL_ERROR "ergo packages: manifest '${file}' is not a JSON object (${_err})")
    endif()

    set(_out "")       # names of every variable to hand back
    set(_errors "")

    _ergo_pkg_json_string(_schema "${_json}" schema)
    if(NOT _schema STREQUAL "1")
        list(APPEND _errors "schema is '${_schema}' but this Ergo reads schema 1")
    endif()

    _ergo_pkg_json_keys(_packages "${_json}" packages)
    set(_link "")
    set(_tools "")
    foreach(_n IN LISTS _packages)
        set(_p "${prefix}_PKG_${_n}")
        _ergo_pkg_json_string(${_p}_KIND        "${_json}" packages "${_n}" kind)
        _ergo_pkg_json_string(${_p}_CODE        "${_json}" packages "${_n}" code)
        _ergo_pkg_json_string(${_p}_REPOSITORY  "${_json}" packages "${_n}" repository)
        _ergo_pkg_json_string(${_p}_REV         "${_json}" packages "${_n}" rev)
        _ergo_pkg_json_string(${_p}_EXPECT_TAG  "${_json}" packages "${_n}" expect_tag)
        _ergo_pkg_json_array (${_p}_PLATFORMS   "${_json}" packages "${_n}" platforms)
        _ergo_pkg_json_string(${_p}_CMAKE_MINIMUM "${_json}" packages "${_n}" cmake minimum)
        _ergo_pkg_json_array (${_p}_TARGETS     "${_json}" packages "${_n}" cmake targets)
        _ergo_pkg_json_string(${_p}_FIND_PACKAGE "${_json}" packages "${_n}" cmake find_package name)
        _ergo_pkg_json_pairs (${_p}_CACHE       "${_json}" packages "${_n}" cmake cache)
        _ergo_pkg_json_keys  (${_p}_REQUIRES    "${_json}" packages "${_n}" requires)
        _ergo_pkg_json_string(${_p}_ACQUIRE     "${_json}" packages "${_n}" acquire)
        _ergo_pkg_json_string(${_p}_PATH        "${_json}" packages "${_n}" path)
        _ergo_pkg_json_keys  (${_p}_FEATURES    "${_json}" packages "${_n}" features)
        list(APPEND _out ${_p}_KIND ${_p}_CODE ${_p}_REPOSITORY ${_p}_REV ${_p}_EXPECT_TAG
            ${_p}_PLATFORMS ${_p}_CMAKE_MINIMUM ${_p}_TARGETS ${_p}_FIND_PACKAGE ${_p}_CACHE
            ${_p}_REQUIRES ${_p}_ACQUIRE ${_p}_PATH ${_p}_FEATURES)

        if(${_p}_KIND STREQUAL "link")
            list(APPEND _link "${_n}")
        elseif(${_p}_KIND STREQUAL "tool")
            list(APPEND _tools "${_n}")
        else()
            list(APPEND _errors "package '${_n}' has kind '${${_p}_KIND}' (want link or tool)")
        endif()

        foreach(_f IN LISTS ${_p}_FEATURES)
            set(_q "${_p}_FEAT_${_f}")
            _ergo_pkg_json_string(${_q}_DEFAULT   "${_json}" packages "${_n}" features "${_f}" default)
            _ergo_pkg_json_array (${_q}_PLATFORMS "${_json}" packages "${_n}" features "${_f}" platforms)
            _ergo_pkg_json_string(${_q}_RESERVED  "${_json}" packages "${_n}" features "${_f}" reserved)
            _ergo_pkg_json_array (${_q}_PROVIDES  "${_json}" packages "${_n}" features "${_f}" provides)
            _ergo_pkg_json_string(${_q}_CONTRACT  "${_json}" packages "${_n}" features "${_f}" contract)
            _ergo_pkg_json_array (${_q}_REQUIRES  "${_json}" packages "${_n}" features "${_f}" requires)
            _ergo_pkg_json_array (${_q}_CONFLICTS "${_json}" packages "${_n}" features "${_f}" conflicts)
            if(${_q}_RESERVED STREQUAL "ON")
                set(${_q}_RESERVED TRUE)
            else()
                set(${_q}_RESERVED FALSE)
            endif()
            list(APPEND _out ${_q}_DEFAULT ${_q}_PLATFORMS ${_q}_RESERVED ${_q}_PROVIDES
                ${_q}_CONTRACT ${_q}_REQUIRES ${_q}_CONFLICTS)
        endforeach()
    endforeach()

    _ergo_pkg_json_keys(_modules "${_json}" modules)
    foreach(_m IN LISTS _modules)
        set(_q "${prefix}_MOD_${_m}")
        _ergo_pkg_json_array (${_q}_NEEDS         "${_json}" modules "${_m}" needs)
        _ergo_pkg_json_array (${_q}_OPTIONAL      "${_json}" modules "${_m}" optional)
        _ergo_pkg_json_string(${_q}_NEED_KIND     "${_json}" modules "${_m}" need_kind)
        _ergo_pkg_json_string(${_q}_WHEN          "${_json}" modules "${_m}" when)
        _ergo_pkg_json_string(${_q}_REAL_PATH_VAR "${_json}" modules "${_m}" real_path_var)
        _ergo_pkg_json_string(${_q}_REASON_VAR    "${_json}" modules "${_m}" reason_var)
        set(${_q}_PORTED_FROM "")
        string(JSON _type ERROR_VARIABLE _err TYPE "${_json}" modules "${_m}" ported_from)
        if(NOT _err AND _type STREQUAL "ARRAY")
            string(JSON _len LENGTH "${_json}" modules "${_m}" ported_from)
            if(_len GREATER 0)
                math(EXPR _last "${_len} - 1")
                foreach(_i RANGE ${_last})
                    _ergo_pkg_json_string(_from "${_json}" modules "${_m}" ported_from ${_i} package)
                    list(APPEND ${_q}_PORTED_FROM "${_from}")
                endforeach()
            endif()
        endif()
        list(APPEND _out ${_q}_NEEDS ${_q}_OPTIONAL ${_q}_NEED_KIND ${_q}_WHEN
            ${_q}_REAL_PATH_VAR ${_q}_REASON_VAR ${_q}_PORTED_FROM)
    endforeach()

    _ergo_pkg_json_keys(_observe "${_json}" observe)
    foreach(_o IN LISTS _observe)
        set(_q "${prefix}_OBS_${_o}")
        _ergo_pkg_json_string(${_q}_CODE      "${_json}" observe "${_o}" code)
        _ergo_pkg_json_string(${_q}_PATH_HINT "${_json}" observe "${_o}" path_hint)
        _ergo_pkg_json_string(${_q}_GROUP     "${_json}" observe "${_o}" group)
        set(${_q}_RELATIONS "")
        string(JSON _type ERROR_VARIABLE _err TYPE "${_json}" observe "${_o}" relations)
        if(NOT _err AND _type STREQUAL "ARRAY")
            string(JSON _len LENGTH "${_json}" observe "${_o}" relations)
            if(_len GREATER 0)
                math(EXPR _last "${_len} - 1")
                foreach(_i RANGE ${_last})
                    _ergo_pkg_json_string(_from "${_json}" observe "${_o}" relations ${_i} from)
                    _ergo_pkg_json_string(_kind "${_json}" observe "${_o}" relations ${_i} kind)
                    list(APPEND ${_q}_RELATIONS "${_from}:${_kind}")
                endforeach()
            endif()
        endif()
        list(APPEND _out ${_q}_CODE ${_q}_PATH_HINT ${_q}_GROUP ${_q}_RELATIONS)
    endforeach()

    set(${prefix}_SCHEMA   "${_schema}"   PARENT_SCOPE)
    set(${prefix}_ERRORS   "${_errors}"   PARENT_SCOPE)
    set(${prefix}_PACKAGES "${_packages}" PARENT_SCOPE)
    set(${prefix}_LINK     "${_link}"     PARENT_SCOPE)
    set(${prefix}_TOOLS    "${_tools}"    PARENT_SCOPE)
    set(${prefix}_MODULES  "${_modules}"  PARENT_SCOPE)
    set(${prefix}_OBSERVE  "${_observe}"  PARENT_SCOPE)
    foreach(_v IN LISTS _out)
        set(${_v} "${${_v}}" PARENT_SCOPE)
    endforeach()
endfunction()
