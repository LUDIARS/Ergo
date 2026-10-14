# ---------------------------------------------------------------------------
# compare_m0_build.cmake — acceptance C-5 on the real tree (manual run)
#
# Configures two Ergo source trees with the same cache arguments and compares
# the target set and per-target compile definitions taken from the CMake File
# API (codemodel v2). Use it to prove that a change keeps M0 (ERGO_PACKAGES
# unset) byte-for-byte equivalent for hosts:
#
#   cmake -DBASE_SOURCE=<baseline ergo checkout> -DNEW_SOURCE=<this checkout>
#         -DWORK_DIR=<scratch dir> [-DGENERATOR=<gen>] [-DPLATFORM=<arch>]
#         [-DCONFIGURE_ARGS=<-Dk=v;...>] -P tests/packages/compare_m0_build.cmake
#
# Output: <WORK_DIR>/{base,new}.targets.txt and FATAL_ERROR with the first
# differing lines when they differ. Not registered in ctest: it configures the
# whole tree twice (compiler detection included).
# ---------------------------------------------------------------------------
foreach(_req BASE_SOURCE NEW_SOURCE WORK_DIR)
    if(NOT DEFINED ${_req})
        message(FATAL_ERROR "compare_m0_build: -D${_req}=... is required")
    endif()
endforeach()

# Configure <source> into <build> with a private File API query, then write
# one "<target>|<type>|<config>|<sorted defines>" line per target to <out>.
function(_m0_configure_and_dump source build out)
    file(REMOVE_RECURSE "${build}")
    file(WRITE "${build}/.cmake/api/v1/query/client-ergo-compare/query.json"
        "{ \"requests\": [ { \"kind\": \"codemodel\", \"version\": 2 } ] }\n")
    set(_args -S "${source}" -B "${build}")
    if(GENERATOR)
        list(APPEND _args -G "${GENERATOR}")
    endif()
    if(PLATFORM)
        list(APPEND _args -A "${PLATFORM}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" ${_args} ${CONFIGURE_ARGS}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _log ERROR_VARIABLE _log)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "compare_m0_build: configure of ${source} failed\n${_log}")
    endif()

    set(_reply "${build}/.cmake/api/v1/reply")
    file(GLOB _index "${_reply}/index-*.json")
    list(SORT _index)
    list(GET _index -1 _index)
    file(READ "${_index}" _ij)
    string(JSON _cm_file GET "${_ij}" reply client-ergo-compare query.json responses 0 jsonFile)
    file(READ "${_reply}/${_cm_file}" _cm)

    set(_lines "")
    string(JSON _nconf LENGTH "${_cm}" configurations)
    math(EXPR _clast "${_nconf} - 1")
    foreach(_c RANGE ${_clast})
        string(JSON _cname GET "${_cm}" configurations ${_c} name)
        string(JSON _nt LENGTH "${_cm}" configurations ${_c} targets)
        if(_nt EQUAL 0)
            continue()
        endif()
        math(EXPR _tlast "${_nt} - 1")
        foreach(_t RANGE ${_tlast})
            string(JSON _tfile GET "${_cm}" configurations ${_c} targets ${_t} jsonFile)
            file(READ "${_reply}/${_tfile}" _tj)
            string(JSON _name GET "${_tj}" name)
            string(JSON _type GET "${_tj}" type)
            set(_defs "")
            string(JSON _ngroups ERROR_VARIABLE _e LENGTH "${_tj}" compileGroups)
            if(NOT _e AND _ngroups GREATER 0)
                math(EXPR _glast "${_ngroups} - 1")
                foreach(_g RANGE ${_glast})
                    string(JSON _nd ERROR_VARIABLE _e2 LENGTH "${_tj}" compileGroups ${_g} defines)
                    if(NOT _e2 AND _nd GREATER 0)
                        math(EXPR _dlast "${_nd} - 1")
                        foreach(_d RANGE ${_dlast})
                            string(JSON _def GET "${_tj}" compileGroups ${_g} defines ${_d} define)
                            list(APPEND _defs "${_def}")
                        endforeach()
                    endif()
                endforeach()
            endif()
            list(REMOVE_DUPLICATES _defs)
            list(SORT _defs)
            string(REPLACE ";" " " _defs "${_defs}")
            list(APPEND _lines "${_name}|${_type}|${_cname}|${_defs}")
        endforeach()
    endforeach()
    list(SORT _lines)
    string(REPLACE ";" "\n" _text "${_lines}")
    file(WRITE "${out}" "${_text}\n")
endfunction()

_m0_configure_and_dump("${BASE_SOURCE}" "${WORK_DIR}/base" "${WORK_DIR}/base.targets.txt")
_m0_configure_and_dump("${NEW_SOURCE}" "${WORK_DIR}/new" "${WORK_DIR}/new.targets.txt")

file(STRINGS "${WORK_DIR}/base.targets.txt" _base)
file(STRINGS "${WORK_DIR}/new.targets.txt" _new)
set(_only_base ${_base})
set(_only_new ${_new})
list(REMOVE_ITEM _only_base ${_new})
list(REMOVE_ITEM _only_new ${_base})
list(LENGTH _base _count)
if(_only_base OR _only_new)
    string(REPLACE ";" "\n  - " _ob "${_only_base}")
    string(REPLACE ";" "\n  + " _on "${_only_new}")
    message(FATAL_ERROR "C-5: target set / compile definitions differ\n  - ${_ob}\n  + ${_on}")
endif()
message(STATUS "C-5: ${_count} target/config lines identical (base vs new)")
