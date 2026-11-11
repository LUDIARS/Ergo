# tests/hardening/test_lorica_compile_commands.cmake — does every unit of Ergo's
# own sources compile with Lorica's flags? (cmake -P, no Python.)
#
# Reads <build>/compile_commands.json and, for each entry whose file is in
# Ergo's source tree (not the build tree, which holds the fetched sources),
# checks the command line for what lorica_harden_target /
# lorica_deprecate_unsafe_apis add with this toolchain (cmake/LoricaHardening.cmake
# of the pinned Lorica) and for flags that cancel them. A missing flag fails
# with the file names. Registered by cmake/ErgoHardeningChecks.cmake.
#
# -D inputs:
#   ERGO_SOURCE_DIR ERGO_BINARY_DIR   the checkout and its build directory
#   ERGO_LORICA_ACTIVE                ON / OFF (OFF: skip, with the reason)
#   ERGO_LORICA_INACTIVE_REASON       why Lorica does not apply
#   ERGO_EXPORTS_COMPILE_COMMANDS     ON when the generator writes the file
#   ERGO_GENERATOR                    for the skip message
#   ERGO_LORICA_INCLUDE_DIR           Lorica's include directory
#   ERGO_LORICA_UNSAFE_HEADER         the force-included header ("" if not applied)
#   ERGO_LORICA_UNSAFE_SKIPPED        targets that do not take the rejection (C sources), comma-separated
#   ERGO_LORICA_EXTENDED_TARGETS      targets that must carry the EXTENDED profile, comma-separated
#   ERGO_LORICA_ARCH_FLAGS            -fcf-protection=full or -mbranch-protection=standard
#   ERGO_LORICA_FORTIFY_C / _CXX      expected _FORTIFY_SOURCE level, 0 = not added
#
# Skips (ctest SKIP_REGULAR_EXPRESSION "^SKIPPED:") instead of passing when the
# build has no Lorica or the generator writes no compile_commands.json.
cmake_minimum_required(VERSION 3.20)

# Target lists arrive comma-separated (a ';' would split the add_test argument).
string(REPLACE "," ";" ERGO_LORICA_UNSAFE_SKIPPED "${ERGO_LORICA_UNSAFE_SKIPPED}")
string(REPLACE "," ";" ERGO_LORICA_EXTENDED_TARGETS "${ERGO_LORICA_EXTENDED_TARGETS}")

if(NOT ERGO_LORICA_ACTIVE)
    message("SKIPPED: Ergo is built without Lorica here: ${ERGO_LORICA_INACTIVE_REASON}")
    return()
endif()
if(NOT ERGO_EXPORTS_COMPILE_COMMANDS)
    message("SKIPPED: the ${ERGO_GENERATOR} generator writes no compile_commands.json, so the command lines cannot be checked")
    return()
endif()

set(_db "${ERGO_BINARY_DIR}/compile_commands.json")
if(NOT EXISTS "${_db}")
    message(FATAL_ERROR "${_db} does not exist (CMAKE_EXPORT_COMPILE_COMMANDS is set by Ergo's top-level CMakeLists.txt)")
endif()
file(READ "${_db}" _json)
string(JSON _count ERROR_VARIABLE _err LENGTH "${_json}")
if(_err OR _count EQUAL 0)
    message(FATAL_ERROR "${_db}: no entries (${_err})")
endif()

cmake_path(SET _src NORMALIZE "${ERGO_SOURCE_DIR}/")
cmake_path(SET _bin NORMALIZE "${ERGO_BINARY_DIR}/")
cmake_path(SET _lorica_inc NORMALIZE "${ERGO_LORICA_INCLUDE_DIR}")
string(REGEX REPLACE "/$" "" _lorica_inc "${_lorica_inc}")
if(ERGO_LORICA_UNSAFE_HEADER)
    cmake_path(SET _unsafe_header NORMALIZE "${ERGO_LORICA_UNSAFE_HEADER}")
endif()

# Flags every C / C++ unit gets, and the ones that would undo them.
set(_common_required -fstack-protector-strong -fstack-clash-protection ${ERGO_LORICA_ARCH_FLAGS})
set(_forbidden
    -fno-stack-protector -fstack-protector -fno-stack-clash-protection
    -fcf-protection=none -fcf-protection=branch -fcf-protection=return -mbranch-protection=none
    -D_FORTIFY_SOURCE=0 -D_FORTIFY_SOURCE=1 -U_GLIBCXX_ASSERTIONS
    -Wno-deprecated-declarations -Wno-error=deprecated-declarations -Wno-error
    -fno-PIE -fno-pie -fno-PIC -fno-pic -ftrivial-auto-var-init=uninitialized)

set(_failures "")
set(_checked 0)
set(_seen_extended "")
math(EXPR _last "${_count} - 1")
foreach(_i RANGE ${_last})
    string(JSON _file GET "${_json}" ${_i} file)
    string(JSON _dir GET "${_json}" ${_i} directory)
    if(NOT IS_ABSOLUTE "${_file}")
        set(_file "${_dir}/${_file}")
    endif()
    cmake_path(SET _file NORMALIZE "${_file}")
    cmake_path(IS_PREFIX _src "${_file}" _inside)
    set(_in_build FALSE)
    if(NOT _bin STREQUAL _src)
        cmake_path(IS_PREFIX _bin "${_file}" _in_build)
    endif()
    if(NOT _inside OR _in_build)
        continue()  # fetched / generated sources (curl, Pictor, ...) are not Ergo's
    endif()
    string(JSON _cmd ERROR_VARIABLE _no_cmd GET "${_json}" ${_i} command)
    if(_no_cmd)
        # "arguments" form: a JSON array of strings.
        string(JSON _n LENGTH "${_json}" ${_i} arguments)
        set(_args "")
        math(EXPR _n_last "${_n} - 1")
        foreach(_k RANGE ${_n_last})
            string(JSON _a GET "${_json}" ${_i} arguments ${_k})
            list(APPEND _args "${_a}")
        endforeach()
    else()
        separate_arguments(_args UNIX_COMMAND "${_cmd}")
    endif()

    # The target, from the object path (CMakeFiles/<target>.dir/...).
    set(_target "")
    list(FIND _args "-o" _o)
    if(_o GREATER_EQUAL 0)
        math(EXPR _o "${_o} + 1")
        list(GET _args ${_o} _obj)
        if(_obj MATCHES "CMakeFiles/([^/]+)\\.dir/")
            set(_target "${CMAKE_MATCH_1}")
        endif()
    endif()

    set(_is_c FALSE)
    if(_file MATCHES "\\.c$")
        set(_is_c TRUE)
    endif()
    set(_required ${_common_required})
    set(_fortify ${ERGO_LORICA_FORTIFY_CXX})
    if(_is_c)
        set(_fortify ${ERGO_LORICA_FORTIFY_C})
    else()
        list(APPEND _required -D_GLIBCXX_ASSERTIONS)
        set(_mode _LIBCPP_HARDENING_MODE_FAST)
        if(_target AND _target IN_LIST ERGO_LORICA_EXTENDED_TARGETS)
            set(_mode _LIBCPP_HARDENING_MODE_EXTENSIVE)
        endif()
        list(APPEND _required -D_LIBCPP_HARDENING_MODE=${_mode})
        if(_unsafe_header AND NOT _target IN_LIST ERGO_LORICA_UNSAFE_SKIPPED)
            list(APPEND _required -Werror=deprecated-declarations)
        endif()
    endif()
    if(_fortify)
        list(APPEND _required -D_FORTIFY_SOURCE=${_fortify})
    endif()
    if(_target AND _target IN_LIST ERGO_LORICA_EXTENDED_TARGETS)
        list(APPEND _required -ftrivial-auto-var-init=zero)
        list(APPEND _seen_extended "${_target}")
    endif()

    set(_missing "")
    foreach(_f IN LISTS _required)
        if(NOT _f IN_LIST _args)
            list(APPEND _missing "${_f}")
        endif()
    endforeach()
    # Position independent code: -fPIE (executables) or -fPIC (libraries).
    if(NOT "-fPIE" IN_LIST _args AND NOT "-fPIC" IN_LIST _args AND NOT "-fpie" IN_LIST _args AND NOT "-fpic" IN_LIST _args)
        list(APPEND _missing "-fPIE/-fPIC")
    endif()
    # The last _FORTIFY_SOURCE definition is the one that counts.
    if(_fortify)
        set(_last_fortify "")
        foreach(_a IN LISTS _args)
            if(_a MATCHES "^-[DU]_FORTIFY_SOURCE")
                set(_last_fortify "${_a}")
            endif()
        endforeach()
        if(NOT _last_fortify STREQUAL "-D_FORTIFY_SOURCE=${_fortify}")
            list(APPEND _missing "last _FORTIFY_SOURCE is '${_last_fortify}'")
        endif()
    endif()
    # Lorica's include directory (-I / -isystem, joined or separate).
    set(_has_inc FALSE)
    set(_prev "")
    foreach(_a IN LISTS _args)
        set(_path "")
        # One MATCHES per branch: a failed MATCHES clears CMAKE_MATCH_1.
        if(_a MATCHES "^-isystem(.+)$")
            set(_path "${CMAKE_MATCH_1}")
        elseif(_a MATCHES "^-I(.+)$")
            set(_path "${CMAKE_MATCH_1}")
        elseif(_prev STREQUAL "-I" OR _prev STREQUAL "-isystem")
            set(_path "${_a}")
        endif()
        if(_path)
            cmake_path(SET _path NORMALIZE "${_path}")
            string(REGEX REPLACE "/$" "" _path "${_path}")
            if(_path STREQUAL _lorica_inc)
                set(_has_inc TRUE)
            endif()
        endif()
        set(_prev "${_a}")
    endforeach()
    if(NOT _has_inc)
        list(APPEND _missing "-I${_lorica_inc}")
    endif()
    # The force-included <lorica/unsafe_api.hpp> (C++ only).
    if(NOT _is_c AND _unsafe_header AND NOT _target IN_LIST ERGO_LORICA_UNSAFE_SKIPPED)
        set(_has_header FALSE)
        set(_prev "")
        foreach(_a IN LISTS _args)
            if(_prev STREQUAL "-include")
                cmake_path(SET _h NORMALIZE "${_a}")
                if(_h STREQUAL _unsafe_header)
                    set(_has_header TRUE)
                endif()
            endif()
            set(_prev "${_a}")
        endforeach()
        if(NOT _has_header)
            list(APPEND _missing "-include ${_unsafe_header}")
        endif()
    endif()

    set(_cancel "")
    foreach(_f IN LISTS _forbidden)
        if(_f IN_LIST _args)
            list(APPEND _cancel "${_f}")
        endif()
    endforeach()

    file(RELATIVE_PATH _rel "${ERGO_SOURCE_DIR}" "${_file}")
    if(_missing)
        list(JOIN _missing " " _m)
        list(APPEND _failures "${_rel} [${_target}]: missing ${_m}")
    endif()
    if(_cancel)
        list(JOIN _cancel " " _c)
        list(APPEND _failures "${_rel} [${_target}]: cancelling ${_c}")
    endif()
    math(EXPR _checked "${_checked} + 1")
endforeach()

if(_checked EQUAL 0)
    message(FATAL_ERROR "${_db}: no entry for a source of ${ERGO_SOURCE_DIR}")
endif()
foreach(_t IN LISTS ERGO_LORICA_EXTENDED_TARGETS)
    if(NOT _t IN_LIST _seen_extended)
        list(APPEND _failures "EXTENDED target ${_t}: no compile command found")
    endif()
endforeach()

if(_failures)
    list(LENGTH _failures _nf)
    list(JOIN _failures "\n  " _list)
    message(FATAL_ERROR "Lorica is not on ${_nf} of ${_checked} unit(s):\n  ${_list}")
endif()
list(LENGTH ERGO_LORICA_EXTENDED_TARGETS _ne)
message("Lorica flags present on all ${_checked} unit(s) of Ergo's sources (${_ne} EXTENDED target(s))")
