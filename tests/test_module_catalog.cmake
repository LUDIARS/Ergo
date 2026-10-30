cmake_minimum_required(VERSION 3.20)
# ---------------------------------------------------------------------------
# tests/test_module_catalog.cmake — catalog consistency gate (ctest
# test_module_catalog). Toolchain-free: cmake -P only, no compiler, no network.
#
#   cmake -DERGO_SOURCE_DIR=<ergo> -P tests/test_module_catalog.cmake
#
# Checks that the human / machine catalogs still describe the code:
#   C0  module_list.yaml keeps the format this reader relies on (schema 2,
#       quoted summaries, every required key, known category / status)
#   C1  every include/ergo/<x>/ is a module, a shared lib or explicitly excluded
#   C2  per module: name / dirs / option follow ergo_<x>; header, source and test
#       dirs and the spec exist; option(<cmake_option>) is in CMakeLists.txt;
#       extra_targets exist with their option; depends equals the ergo_* link
#       edges of target_link_libraries(<name> ...); tool_plugins are registered
#   C3  every module / shared lib has a row in module_list.md and in README.md's
#       "## モジュール" section, and those tables list nothing else
#   C4  src/<x>/, include/ergo/<x>/ and tests/<x>/ are matched by the patterns
#       of exactly one spec/domains/*.domain.json, which is the yaml domain
#   C5  every ERGO_* cache option declared in CMakeLists.txt, cmake/**,
#       third_party/dependencies.cmake, the bootstrap template and examples/ is
#       named in spec/setup/config-reference.md, and that table names no option
#       that is not declared
#   C6  tools/ergo: every registry.ts plugin id has a plugin dir whose index.ts
#       declares that id, a row in spec/feature/tool/ergo.md's plugin matrix, a
#       "### `<id>`" section there or its own spec/feature/tool/<id>.md, and a
#       URL in tools/ergo/README.md; no plugin dir or matrix row is unregistered
#   C7  relative markdown links in README.md, module_list.md,
#       tools/ergo/README.md and spec/{setup,feature,module,domains,
#       architecture,data} resolve (code fences, inline code, URLs, #anchors
#       and targets outside the repository are skipped; spec/plan and
#       spec/tasks are history and not checked), and every
#       spec/{module,feature/module,feature/tool,tool}/<x>.md named in
#       include/, src/, examples/ or the tools sources exists
#
# Every problem is collected; the script ends with one FATAL_ERROR that lists
# them all (file:line where there is one), or a STATUS line when clean.
# ---------------------------------------------------------------------------

if(NOT DEFINED ERGO_SOURCE_DIR OR ERGO_SOURCE_DIR STREQUAL "")
    get_filename_component(ERGO_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
file(TO_CMAKE_PATH "${ERGO_SOURCE_DIR}" ERGO_SOURCE_DIR)
get_filename_component(ERGO_SOURCE_DIR "${ERGO_SOURCE_DIR}" ABSOLUTE)
set(R "${ERGO_SOURCE_DIR}")

set(_problems "")
set(_problem_count 0)
macro(_fail where msg)
    string(APPEND _problems "\n  ${where}: ${msg}")
    math(EXPR _problem_count "${_problem_count} + 1")
endmacro()

# Read a text file as a list of lines. ';', '[', ']' and '\' would break CMake
# list handling, so they are replaced by <SC> / <LB> / <RB> / <BS>; CR is
# dropped (Windows checkouts). Line N is element N-1.
function(_read_lines path out)
    file(READ "${path}" _c)
    string(REPLACE "\r" "" _c "${_c}")
    string(REPLACE "\\" "<BS>" _c "${_c}")
    string(REPLACE ";" "<SC>" _c "${_c}")
    string(REPLACE "[" "<LB>" _c "${_c}")
    string(REPLACE "]" "<RB>" _c "${_c}")
    string(REPLACE "\n" ";" _c "${_c}")
    set(${out} "${_c}" PARENT_SCOPE)
endfunction()

function(_unescape out s)
    string(REPLACE "<BS>" "\\" s "${s}")
    string(REPLACE "<SC>" ";" s "${s}")
    string(REPLACE "<LB>" "[" s "${s}")
    string(REPLACE "<RB>" "]" s "${s}")
    set(${out} "${s}" PARENT_SCOPE)
endfunction()

# 1-based line of the first occurrence of `needle` in `path` (0 = absent).
function(_line_of out path needle)
    file(READ "${path}" _c)
    string(FIND "${_c}" "${needle}" _pos)
    if(_pos LESS 0)
        set(${out} 0 PARENT_SCOPE)
        return()
    endif()
    string(SUBSTRING "${_c}" 0 ${_pos} _head)
    string(REGEX MATCHALL "\n" _nl "${_head}")
    list(LENGTH _nl _n)
    math(EXPR _n "${_n} + 1")
    set(${out} ${_n} PARENT_SCOPE)
endfunction()

# The text between `begin` and the next `end` marker (both excluded); empty
# when `begin` is missing.
function(_section out text begin end)
    string(FIND "${text}" "${begin}" _b)
    if(_b LESS 0)
        set(${out} "" PARENT_SCOPE)
        return()
    endif()
    string(LENGTH "${begin}" _bl)
    math(EXPR _b "${_b} + ${_bl}")
    string(SUBSTRING "${text}" ${_b} -1 _rest)
    string(FIND "${_rest}" "${end}" _e)
    if(_e GREATER_EQUAL 0)
        string(SUBSTRING "${_rest}" 0 ${_e} _rest)
    endif()
    set(${out} "${_rest}" PARENT_SCOPE)
endfunction()

foreach(_required module_list.yaml module_list.md README.md CMakeLists.txt
        spec/setup/config-reference.md spec/feature/tool/ergo.md
        tools/ergo/README.md tools/ergo/src/core/registry.ts)
    if(NOT EXISTS "${R}/${_required}")
        message(FATAL_ERROR "test_module_catalog: ${R}/${_required} is missing "
            "(is ERGO_SOURCE_DIR an Ergo checkout?)")
    endif()
endforeach()

file(READ "${R}/CMakeLists.txt" _cmakelists)

# ===========================================================================
# C0  module_list.yaml
# ===========================================================================
set(_yaml "module_list.yaml")
_read_lines("${R}/${_yaml}" _ylines)
set(_modules "")
set(_shared "")
set(_excluded "")
set(_section "")
set(_cur "")
set(_schema "")
set(_lineno 0)
set(_keys_required name summary category domain header_dir source_dir test_dir spec
    cmake_option depends tool_plugins status)
set(_keys_shared name summary header_dir source_dir test_dir spec)
foreach(_l IN LISTS _ylines)
    math(EXPR _lineno "${_lineno} + 1")
    if(_l MATCHES "^schema_version:[ ]*([0-9]+)")
        set(_schema "${CMAKE_MATCH_1}")
    elseif(_l MATCHES "^(modules|shared_libs|excluded):")
        set(_section "${CMAKE_MATCH_1}")
        set(_cur "")
    elseif(_l MATCHES "^[a-z_]+:")
        set(_section "")
        set(_cur "")
    elseif(_section AND _l MATCHES "^  - name:[ ]*([a-z0-9_]+)[ ]*$")
        set(_cur "${CMAKE_MATCH_1}")
        if(_section STREQUAL "modules")
            list(APPEND _modules "${_cur}")
        elseif(_section STREQUAL "shared_libs")
            list(APPEND _shared "${_cur}")
        else()
            list(APPEND _excluded "${_cur}")
        endif()
        set(_y_${_cur}_line ${_lineno})
        set(_y_${_cur}_keys name)
    elseif(_cur AND _l MATCHES "^    ([a-z_]+):[ ]*(.*)$")
        set(_k "${CMAKE_MATCH_1}")
        set(_v "${CMAKE_MATCH_2}")
        list(APPEND _y_${_cur}_keys "${_k}")
        if(_k STREQUAL "summary" OR _k STREQUAL "reason")
            if(NOT _v MATCHES "^\"(.*)\"[ ]*(#.*)?$")
                _fail("${_yaml}:${_lineno}" "${_cur}.${_k} must be a double-quoted string")
            elseif(CMAKE_MATCH_1 STREQUAL "")
                _fail("${_yaml}:${_lineno}" "${_cur}.${_k} is empty")
            endif()
        elseif(_v MATCHES "^<LB>(.*)<RB>")
            # flow list
            set(_y_${_cur}_${_k} "${CMAKE_MATCH_1}")
        else()
            string(REGEX REPLACE "[ ]*#.*$" "" _v "${_v}")
            string(STRIP "${_v}" _v)
            set(_y_${_cur}_${_k} "${_v}")
        endif()
    elseif(_cur AND NOT _l MATCHES "^[ ]*(#.*)?$")
        _fail("${_yaml}:${_lineno}" "unexpected line in ${_cur} (keys are one per line, '    key: value')")
    endif()
endforeach()

if(NOT _schema STREQUAL "2")
    _fail("${_yaml}" "schema_version is '${_schema}', expected 2")
endif()
list(LENGTH _modules _nmod)
if(_nmod EQUAL 0)
    _fail("${_yaml}" "no modules parsed")
endif()

# A flow list "a, b" -> CMake list; "{...}" maps are kept whole.
function(_flow_items out s)
    string(REPLACE "," ";" s "${s}")
    set(_items "")
    foreach(_i IN LISTS s)
        string(STRIP "${_i}" _i)
        if(NOT _i STREQUAL "")
            list(APPEND _items "${_i}")
        endif()
    endforeach()
    set(${out} "${_items}" PARENT_SCOPE)
endfunction()

foreach(_m IN LISTS _modules)
    foreach(_k IN LISTS _keys_required)
        if(NOT _k IN_LIST _y_${_m}_keys)
            _fail("${_yaml}:${_y_${_m}_line}" "${_m} has no '${_k}' key")
        endif()
    endforeach()
    if(NOT "${_y_${_m}_status}" MATCHES "^(stable|preview|incomplete)$")
        _fail("${_yaml}:${_y_${_m}_line}" "${_m}.status '${_y_${_m}_status}' is not stable / preview / incomplete")
    endif()
    if(NOT "${_y_${_m}_category}" MATCHES "^(UI|ロジック|システム|ゲームオブジェクト)$")
        _fail("${_yaml}:${_y_${_m}_line}" "${_m}.category '${_y_${_m}_category}' is not UI / ロジック / システム / ゲームオブジェクト")
    endif()
endforeach()
foreach(_s IN LISTS _shared)
    foreach(_k IN LISTS _keys_shared)
        if(NOT _k IN_LIST _y_${_s}_keys)
            _fail("${_yaml}:${_y_${_s}_line}" "shared lib ${_s} has no '${_k}' key")
        endif()
    endforeach()
endforeach()
foreach(_e IN LISTS _excluded)
    foreach(_k name reason ref)
        if(NOT _k IN_LIST _y_${_e}_keys)
            _fail("${_yaml}:${_y_${_e}_line}" "excluded ${_e} has no '${_k}' key")
        endif()
    endforeach()
    string(REGEX MATCH "^[^ ]+" _ref "${_y_${_e}_ref}")
    if(_ref AND NOT EXISTS "${R}/${_ref}")
        _fail("${_yaml}:${_y_${_e}_line}" "excluded ${_e}: ref ${_ref} does not exist")
    endif()
endforeach()

# ===========================================================================
# tools/ergo registry (used by C2 and C6)
# ===========================================================================
set(_registry_file "tools/ergo/src/core/registry.ts")
file(READ "${R}/${_registry_file}" _registry)
_section(_factories "${_registry}" "PLUGIN_FACTORIES" "];")
string(REGEX MATCHALL "import[ \t]+[A-Za-z0-9_]+[ \t]+from[ \t]+\"\\.\\./plugins/[^/\"]+/index\\.js\"" _imports "${_registry}")
set(_plugin_ids "")
foreach(_imp IN LISTS _imports)
    string(REGEX MATCH "import[ \t]+([A-Za-z0-9_]+)[ \t]+from[ \t]+\"\\.\\./plugins/([^/\"]+)/" _x "${_imp}")
    set(_fn "${CMAKE_MATCH_1}")
    set(_id "${CMAKE_MATCH_2}")
    if(NOT _factories MATCHES "[^A-Za-z0-9_]${_fn}[^A-Za-z0-9_]")
        _fail("${_registry_file}" "${_fn} (plugins/${_id}) is imported but not listed in PLUGIN_FACTORIES")
    else()
        list(APPEND _plugin_ids "${_id}")
    endif()
endforeach()
list(LENGTH _plugin_ids _nplug)
if(_nplug EQUAL 0)
    _fail("${_registry_file}" "no plugin factories parsed")
endif()

# ===========================================================================
# C1  include/ergo/<x>/ ↔ catalog
# ===========================================================================
file(GLOB _inc_children LIST_DIRECTORIES true RELATIVE "${R}/include/ergo" "${R}/include/ergo/*")
foreach(_d IN LISTS _inc_children)
    if(NOT IS_DIRECTORY "${R}/include/ergo/${_d}")
        continue()
    endif()
    set(_n "ergo_${_d}")
    if(_n IN_LIST _modules OR _n IN_LIST _shared)
        continue()
    endif()
    if(_n IN_LIST _excluded)
        continue()  # reason / ref are checked in C0
    endif()
    _fail("include/ergo/${_d}/" "not a module_list.yaml module, shared lib or excluded entry")
endforeach()
foreach(_n IN LISTS _modules _shared _excluded)
    string(REGEX REPLACE "^ergo_" "" _x "${_n}")
    if(NOT IS_DIRECTORY "${R}/include/ergo/${_x}")
        _fail("${_yaml}:${_y_${_n}_line}" "${_n}: include/ergo/${_x}/ does not exist")
    endif()
endforeach()

# ===========================================================================
# C2  per-module paths, options, targets, depends, plugins
# ===========================================================================
function(_link_edges out target)
    # ergo_* items of every target_link_libraries(<target> ...) call.
    string(REGEX MATCHALL "target_link_libraries\\([ \t\r\n]*${target}[ \t\r\n]+[^)]*\\)" _calls "${_cmakelists}")
    set(_edges "")
    foreach(_c IN LISTS _calls)
        string(REGEX REPLACE "^target_link_libraries\\([ \t\r\n]*${target}" "" _c "${_c}")
        string(REGEX MATCHALL "ergo_[a-z0-9_]+" _e "${_c}")
        list(APPEND _edges ${_e})
    endforeach()
    list(REMOVE_DUPLICATES _edges)
    list(SORT _edges)
    set(${out} "${_edges}" PARENT_SCOPE)
endfunction()

function(_target_defined out name)
    if(_cmakelists MATCHES "add_(library|executable|custom_target)\\([ \t\r\n]*${name}[ \t\r\n)]")
        set(${out} TRUE PARENT_SCOPE)
    else()
        set(${out} FALSE PARENT_SCOPE)
    endif()
endfunction()

# extra_targets: [{name: <target>, cmake_option: <OPTION or null>}, ...]
set(_extra_re "name:[ ]*([a-z0-9_]+)[ ]*,[ ]*cmake_option:[ ]*([A-Za-z0-9_]+)")
set(_all_targets ${_modules} ${_shared})
foreach(_n IN LISTS _modules _shared)
    string(REGEX MATCHALL "${_extra_re}" _maps "${_y_${_n}_extra_targets}")
    foreach(_mp IN LISTS _maps)
        string(REGEX MATCH "${_extra_re}" _x2 "${_mp}")
        list(APPEND _all_targets "${CMAKE_MATCH_1}")
    endforeach()
    string(REGEX MATCHALL "{" _braces "${_y_${_n}_extra_targets}")
    list(LENGTH _braces _nb)
    list(LENGTH _maps _nmaps)
    if(NOT _nb EQUAL _nmaps)
        _fail("${_yaml}:${_y_${_n}_line}" "${_n}.extra_targets entries must be {name: <target>, cmake_option: <OPTION or null>}")
    endif()
endforeach()

foreach(_n IN LISTS _modules _shared)
    set(_where "${_yaml}:${_y_${_n}_line}")
    string(REGEX REPLACE "^ergo_" "" _x "${_n}")
    set(_expect_header "include/ergo/${_x}/")
    set(_expect_source "src/${_x}/")
    set(_expect_test "tests/${_x}/")
    foreach(_k header_dir source_dir test_dir)
        set(_v "${_y_${_n}_${_k}}")
        if(_k STREQUAL "header_dir")
            set(_e "${_expect_header}")
        elseif(_k STREQUAL "source_dir")
            set(_e "${_expect_source}")
        else()
            set(_e "${_expect_test}")
        endif()
        if(NOT _v STREQUAL _e)
            _fail("${_where}" "${_n}.${_k} is '${_v}', expected '${_e}'")
        elseif(NOT IS_DIRECTORY "${R}/${_v}")
            _fail("${_where}" "${_n}.${_k} ${_v} does not exist")
        endif()
    endforeach()
    set(_spec "${_y_${_n}_spec}")
    if(NOT _spec MATCHES "^spec/(feature/module|module)/[a-z0-9_]+\\.md$")
        _fail("${_where}" "${_n}.spec '${_spec}' is not spec/feature/module/<x>.md or spec/module/<x>.md")
    elseif(NOT EXISTS "${R}/${_spec}")
        _fail("${_where}" "${_n}.spec ${_spec} does not exist")
    endif()

    # extra targets and their switches
    string(REGEX MATCHALL "${_extra_re}" _maps "${_y_${_n}_extra_targets}")
    foreach(_mp IN LISTS _maps)
        string(REGEX MATCH "${_extra_re}" _x2 "${_mp}")
        set(_tn "${CMAKE_MATCH_1}")
        set(_to "${CMAKE_MATCH_2}")
        _target_defined(_def "${_tn}")
        if(NOT _def)
            _fail("${_where}" "${_n}.extra_targets: ${_tn} is not add_library / add_executable / add_custom_target in CMakeLists.txt")
        endif()
        if(NOT _to STREQUAL "null" AND NOT _cmakelists MATCHES "option\\(${_to}[ \t\r\n]")
            _fail("${_where}" "${_n}.extra_targets: option(${_to}) is not declared in CMakeLists.txt")
        endif()
    endforeach()

    if(NOT _n IN_LIST _modules)
        continue()
    endif()

    # name <-> option convention, and the option exists
    string(TOUPPER "${_x}" _X)
    set(_opt "${_y_${_n}_cmake_option}")
    if(NOT _opt STREQUAL "ERGO_BUILD_${_X}")
        _fail("${_where}" "${_n}.cmake_option is '${_opt}', expected ERGO_BUILD_${_X}")
    endif()
    if(NOT _cmakelists MATCHES "option\\(${_opt}[ \t\r\n]")
        _fail("${_where}" "option(${_opt}) is not declared in CMakeLists.txt")
    endif()
    _target_defined(_def "${_n}")
    if(NOT _def)
        _fail("${_where}" "${_n} is not add_library()'d in CMakeLists.txt")
    endif()

    # depends == ergo_* link edges
    _flow_items(_dep "${_y_${_n}_depends}")
    list(SORT _dep)
    _link_edges(_edges "${_n}")
    if(NOT "${_dep}" STREQUAL "${_edges}")
        string(REPLACE ";" ", " _ds "${_dep}")
        string(REPLACE ";" ", " _es "${_edges}")
        _fail("${_where}" "${_n}.depends is [${_ds}] but CMakeLists.txt links [${_es}]")
    endif()
    foreach(_d IN LISTS _dep)
        if(NOT _d IN_LIST _all_targets)
            _fail("${_where}" "${_n}.depends: ${_d} is not a module, shared lib or extra target")
        endif()
    endforeach()

    # tool plugins are registered built-ins
    _flow_items(_tp "${_y_${_n}_tool_plugins}")
    foreach(_p IN LISTS _tp)
        if(NOT IS_DIRECTORY "${R}/tools/ergo/src/plugins/${_p}")
            _fail("${_where}" "${_n}.tool_plugins: tools/ergo/src/plugins/${_p}/ does not exist")
        endif()
        if(NOT _p IN_LIST _plugin_ids)
            _fail("${_where}" "${_n}.tool_plugins: '${_p}' is not registered in ${_registry_file}")
        endif()
    endforeach()

    if(DEFINED _y_${_n}_cxx_standard AND NOT "${_y_${_n}_cxx_standard}" MATCHES "^(17|20)$")
        _fail("${_where}" "${_n}.cxx_standard must be 17 or 20")
    endif()
endforeach()

# ===========================================================================
# C3  module_list.md and README.md tables
# ===========================================================================
file(READ "${R}/module_list.md" _mlmd)
file(READ "${R}/README.md" _readme)
string(REPLACE "\r" "" _mlmd "${_mlmd}")
string(REPLACE "\r" "" _readme "${_readme}")
_section(_readme_modules "${_readme}" "\n## モジュール\n" "\n## ")
if(_readme_modules STREQUAL "")
    _fail("README.md" "no '## モジュール' section")
endif()
foreach(_n IN LISTS _modules _shared)
    string(FIND "${_mlmd}" "\n| `${_n}`" _p)
    if(_p LESS 0)
        _fail("module_list.md" "no table row for ${_n}")
    endif()
    string(FIND "${_readme_modules}" "`${_n}`" _p)
    if(_p LESS 0)
        _fail("README.md" "${_n} is not listed in the '## モジュール' section")
    endif()
endforeach()
string(REGEX MATCHALL "\n\\| `ergo_[a-z0-9_]+`" _rows "${_mlmd}")
foreach(_r IN LISTS _rows)
    string(REGEX MATCH "ergo_[a-z0-9_]+" _r "${_r}")
    if(NOT _r IN_LIST _modules AND NOT _r IN_LIST _shared)
        _line_of(_ln "${R}/module_list.md" "| `${_r}`")
        _fail("module_list.md:${_ln}" "row ${_r} is not in module_list.yaml")
    endif()
endforeach()
string(REGEX MATCHALL "\n\\| (\\*\\*)?`ergo_[a-z0-9_]+`" _rows "${_readme_modules}")
foreach(_r IN LISTS _rows)
    string(REGEX MATCH "ergo_[a-z0-9_]+" _r "${_r}")
    if(NOT _r IN_LIST _modules AND NOT _r IN_LIST _shared)
        _line_of(_ln "${R}/README.md" "`${_r}`")
        _fail("README.md:${_ln}" "row ${_r} is not in module_list.yaml")
    endif()
endforeach()

# ===========================================================================
# C4  spec/domains membership
# ===========================================================================
file(GLOB _domain_files RELATIVE "${R}" "${R}/spec/domains/*.domain.json")
set(_domains "")
foreach(_f IN LISTS _domain_files)
    file(READ "${R}/${_f}" _json)
    string(JSON _dn ERROR_VARIABLE _err GET "${_json}" name)
    if(_err)
        _fail("${_f}" "not readable as a domain definition (${_err})")
        continue()
    endif()
    list(APPEND _domains "${_dn}")
    set(_dom_${_dn}_file "${_f}")
    set(_dom_${_dn}_patterns "")
    string(JSON _cnt ERROR_VARIABLE _err LENGTH "${_json}" membership)
    if(_err OR _cnt EQUAL 0)
        continue()
    endif()
    math(EXPR _last "${_cnt} - 1")
    foreach(_i RANGE ${_last})
        string(JSON _pp ERROR_VARIABLE _err GET "${_json}" membership ${_i} pathPattern)
        if(NOT _err)
            string(APPEND _dom_${_dn}_patterns "\n${_pp}")
        endif()
    endforeach()
endforeach()
foreach(_n IN LISTS _modules _shared)
    set(_where "${_yaml}:${_y_${_n}_line}")
    string(REGEX REPLACE "^ergo_" "" _x "${_n}")
    foreach(_dir "src/${_x}/" "include/ergo/${_x}/" "tests/${_x}/")
        set(_hits "")
        foreach(_dn IN LISTS _domains)
            string(FIND "${_dom_${_dn}_patterns}" ")${_dir}" _p)
            string(FIND "${_dom_${_dn}_patterns}" "\n^${_dir}" _p2)
            if(_p GREATER_EQUAL 0 OR _p2 GREATER_EQUAL 0)
                list(APPEND _hits "${_dn}")
            endif()
        endforeach()
        list(LENGTH _hits _nh)
        if(NOT _nh EQUAL 1)
            string(REPLACE ";" ", " _hs "${_hits}")
            _fail("${_where}" "${_dir} is in ${_nh} spec/domains definitions (${_hs}), expected exactly 1")
        elseif(_n IN_LIST _modules AND NOT _hits STREQUAL "${_y_${_n}_domain}")
            _fail("${_where}" "${_n}.domain is '${_y_${_n}_domain}' but ${_dir} is in ${_hits}")
        endif()
    endforeach()
endforeach()

# ===========================================================================
# C5  ERGO_* cache options <-> spec/setup/config-reference.md
# ===========================================================================
set(_cfgref_file "spec/setup/config-reference.md")
file(READ "${R}/${_cfgref_file}" _cfgref)
file(GLOB _option_files RELATIVE "${R}"
    "${R}/CMakeLists.txt"
    "${R}/third_party/dependencies.cmake"
    "${R}/cmake/*.cmake"
    "${R}/cmake/deps/*.cmake"
    "${R}/cmake/packages/*.cmake"
    "${R}/cmake/packages/*.cmake.in"
    "${R}/examples/CMakeLists.txt")
set(_declared "")
foreach(_f IN LISTS _option_files)
    # Only lines that start with option( / set( (not comments). set() must
    # carry CACHE on the same line.
    file(STRINGS "${R}/${_f}" _decls ENCODING UTF-8 REGEX "^[ \t]*(option|set)\\(ERGO_[A-Z0-9_]+")
    foreach(_l IN LISTS _decls)
        if(_l MATCHES "^[ \t]*set\\(" AND NOT _l MATCHES "[ \t]CACHE[ \t]")
            continue()
        endif()
        string(REGEX MATCH "^[ \t]*(option|set)\\((ERGO_[A-Z0-9_]+)(\\$\\{)?" _x "${_l}")
        set(_o "${CMAKE_MATCH_2}")
        set(_oname "${CMAKE_MATCH_2}")
        if(CMAKE_MATCH_3)
            # ERGO_SOURCE_DIR_${_upper}: documented as ERGO_SOURCE_DIR_<UPPER>
            set(_needle "`${_o}<")
            set(_o "${_o}<...>")
        else()
            set(_needle "`${_o}`")
        endif()
        list(APPEND _declared "${_o}")
        string(FIND "${_cfgref}" "${_needle}" _p)
        if(_p LESS 0)
            _line_of(_ln "${R}/${_f}" "(${_oname}")
            _fail("${_f}:${_ln}" "${_o} is not documented in ${_cfgref_file}")
        endif()
    endforeach()
endforeach()
_section(_cfg_options "${_cfgref}" "\n## CMake オプション" "\n## ")
string(REGEX MATCHALL "\n\\| `ERGO_[A-Z0-9_]+` \\|" _rows "${_cfg_options}")
foreach(_r IN LISTS _rows)
    string(REGEX MATCH "ERGO_[A-Z0-9_]+" _r "${_r}")
    if(NOT _r IN_LIST _declared)
        _line_of(_ln "${R}/${_cfgref_file}" "| `${_r}` |")
        _fail("${_cfgref_file}:${_ln}" "${_r} is documented but no longer declared as an option / cache variable")
    endif()
endforeach()

# ===========================================================================
# C6  tools/ergo plugins
# ===========================================================================
set(_ergo_md_file "spec/feature/tool/ergo.md")
file(READ "${R}/${_ergo_md_file}" _ergo_md)
string(REPLACE "\r" "" _ergo_md "${_ergo_md}")
file(READ "${R}/tools/ergo/README.md" _tools_readme)
_section(_matrix "${_ergo_md}" "<!-- plugin-matrix:begin -->" "<!-- plugin-matrix:end -->")
if(_matrix STREQUAL "")
    _fail("${_ergo_md_file}" "no <!-- plugin-matrix:begin --> ... <!-- plugin-matrix:end --> table")
endif()
foreach(_id IN LISTS _plugin_ids)
    set(_dir "tools/ergo/src/plugins/${_id}")
    if(NOT EXISTS "${R}/${_dir}/index.ts")
        _fail("${_registry_file}" "plugin '${_id}': ${_dir}/index.ts does not exist")
    else()
        file(READ "${R}/${_dir}/index.ts" _idx)
        if(NOT _idx MATCHES "id:[ \t]*\"${_id}\"")
            _fail("${_dir}/index.ts" "does not declare id: \"${_id}\" (the registry imports it as '${_id}')")
        endif()
    endif()
    string(FIND "${_matrix}" "\n| `${_id}` |" _p)
    if(_p LESS 0)
        _fail("${_ergo_md_file}" "plugin '${_id}' has no row in the plugin matrix")
    endif()
    string(FIND "${_ergo_md}" "\n### `${_id}`" _p)
    if(_p LESS 0 AND NOT EXISTS "${R}/spec/feature/tool/${_id}.md")
        _fail("${_ergo_md_file}" "plugin '${_id}' has neither a '### `${_id}`' section nor spec/feature/tool/${_id}.md")
    endif()
    string(FIND "${_tools_readme}" "/${_id}/" _p)
    if(_p LESS 0)
        _fail("tools/ergo/README.md" "plugin '${_id}' (/${_id}/) is not mentioned")
    endif()
endforeach()
string(REGEX MATCHALL "\n\\| `[a-z0-9_-]+` \\|" _rows "${_matrix}")
foreach(_r IN LISTS _rows)
    string(REGEX MATCH "`([a-z0-9_-]+)`" _x "${_r}")
    if(NOT CMAKE_MATCH_1 IN_LIST _plugin_ids)
        _line_of(_ln "${R}/${_ergo_md_file}" "| `${CMAKE_MATCH_1}` |")
        _fail("${_ergo_md_file}:${_ln}" "matrix row '${CMAKE_MATCH_1}' is not registered in ${_registry_file}")
    endif()
endforeach()
file(GLOB _plugin_dirs LIST_DIRECTORIES true RELATIVE "${R}/tools/ergo/src/plugins" "${R}/tools/ergo/src/plugins/*")
foreach(_d IN LISTS _plugin_dirs)
    if(IS_DIRECTORY "${R}/tools/ergo/src/plugins/${_d}" AND NOT _d IN_LIST _plugin_ids)
        _fail("tools/ergo/src/plugins/${_d}/" "not registered in ${_registry_file}")
    endif()
endforeach()

# ===========================================================================
# C7  links
# ===========================================================================
file(GLOB _md_files RELATIVE "${R}"
    "${R}/README.md" "${R}/module_list.md" "${R}/tools/ergo/README.md"
    "${R}/spec/setup/*.md" "${R}/spec/module/*.md" "${R}/spec/domains/*.md"
    "${R}/spec/architecture/*.md" "${R}/spec/data/*.md")
file(GLOB_RECURSE _md_feature RELATIVE "${R}" "${R}/spec/feature/*.md")
list(APPEND _md_files ${_md_feature})
list(SORT _md_files)
foreach(_f IN LISTS _md_files)
    get_filename_component(_fdir "${R}/${_f}" DIRECTORY)
    _read_lines("${R}/${_f}" _lines)
    set(_in_fence FALSE)
    set(_ln 0)
    foreach(_l IN LISTS _lines)
        math(EXPR _ln "${_ln} + 1")
        if(_l MATCHES "^[ \t]*(```|~~~)")
            if(_in_fence)
                set(_in_fence FALSE)
            else()
                set(_in_fence TRUE)
            endif()
            continue()
        endif()
        if(_in_fence OR NOT _l MATCHES "<RB>\\(")
            continue()
        endif()
        string(REGEX REPLACE "`[^`]*`" "" _l "${_l}")
        string(REGEX MATCHALL "<RB>\\([^) \t]+\\)" _links "${_l}")
        foreach(_k IN LISTS _links)
            string(REGEX REPLACE "^<RB>\\((.*)\\)$" "\\1" _t "${_k}")
            _unescape(_t "${_t}")
            if(_t MATCHES "^[A-Za-z][A-Za-z0-9+.-]*:" OR _t MATCHES "^#")
                continue()
            endif()
            string(REGEX REPLACE "#.*$" "" _path "${_t}")
            if(_path STREQUAL "")
                continue()
            endif()
            get_filename_component(_abs "${_fdir}/${_path}" ABSOLUTE)
            file(RELATIVE_PATH _rel "${R}" "${_abs}")
            if(_rel MATCHES "^\\.\\./" OR _rel STREQUAL "..")
                continue()  # outside the repository (e.g. sibling checkouts)
            endif()
            if(NOT EXISTS "${_abs}")
                _fail("${_f}:${_ln}" "broken link (${_t})")
            endif()
        endforeach()
    endforeach()
endforeach()

file(GLOB_RECURSE _code_files RELATIVE "${R}"
    "${R}/include/*.h" "${R}/include/*.hpp"
    "${R}/src/*.h" "${R}/src/*.hpp" "${R}/src/*.cpp"
    "${R}/examples/*.cpp" "${R}/examples/*.txt" "${R}/examples/*.cmake"
    "${R}/tools/package_cli/*.cpp" "${R}/tools/package_cli/*.h"
    "${R}/tools/package_viz/*.cpp" "${R}/tools/package_viz/*.h"
    "${R}/tools/render_viz/*.cpp"
    "${R}/tools/shuriken_migrator/*.cpp"
    "${R}/tools/ergo/src/*.ts" "${R}/tools/ergo/src/*.js"
    "${R}/tools/ergo/public/*.js" "${R}/tools/ergo/electron/*.cjs")
foreach(_f IN LISTS _code_files)
    file(READ "${R}/${_f}" _c)
    string(REGEX MATCHALL "spec/(module|feature/module|feature/tool|tool)/[a-z0-9_-]+\\.md" _refs "${_c}")
    list(REMOVE_DUPLICATES _refs)
    foreach(_ref IN LISTS _refs)
        if(NOT EXISTS "${R}/${_ref}")
            _line_of(_ln "${R}/${_f}" "${_ref}")
            _fail("${_f}:${_ln}" "names ${_ref}, which does not exist")
        endif()
    endforeach()
endforeach()

# ===========================================================================
list(LENGTH _modules _nm)
list(LENGTH _plugin_ids _np)
list(LENGTH _declared _no)
list(LENGTH _md_files _nf)
if(_problem_count GREATER 0)
    message(FATAL_ERROR "test_module_catalog: ${_problem_count} problem(s) in ${R}:${_problems}\n"
        "The catalogs (module_list.yaml / module_list.md / README.md / "
        "spec/setup/config-reference.md / spec/feature/tool/ergo.md / tools/ergo/README.md / "
        "spec/domains) must describe the code.")
endif()
message(STATUS "test_module_catalog: OK (${_nm} modules, ${_np} plugins, ${_no} options, ${_nf} markdown files)")
