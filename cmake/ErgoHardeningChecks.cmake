# ---------------------------------------------------------------------------
# cmake/ErgoHardeningChecks.cmake — ctest checks that Lorica is really on the
# build (spec/setup/hardening.md "Lorica が掛かっているかのテスト")
#
# ergo_hardening_verify() (cmake/ErgoHardening.cmake) fails configuration when a
# target lacks the Lorica properties. These tests look at the build itself, so a
# dropped flag, a lost forced include or a detached Lorica shows up as a red ctest:
#
#   test_lorica_applied            a unit built like every Ergo target checks
#   test_lorica_applied_extended   from the inside: forced <lorica/unsafe_api.hpp>,
#                                  standard library checks, _FORTIFY_SOURCE,
#                                  EXTENDED marks, Lorica::boundary at run time
#   test_lorica_compile_commands   every compile command of Ergo's own sources
#                                  (tests/hardening/test_lorica_compile_commands.cmake)
#   test_lorica_verify_binary_<t>  Lorica's tools/verify_binary.py on the main
#                                  executables (only when python3 is found)
#
# Where Lorica's build profiles do not apply (macOS, CMake < 3.24, ...) or the
# generator writes no compile_commands.json (Visual Studio), the tests say so and
# ctest counts a skip, not a pass. The probe still runs its Lorica::boundary
# checks first there (a wrong header fails it).
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

# The probe executables. Call before ergo_harden_targets(), which then hardens
# them like any other Ergo target (they are not given Lorica here).
function(ergo_hardening_add_probe_tests)
    get_property(_root GLOBAL PROPERTY ERGO_HARDENING_SOURCE_ROOT)
    get_property(_active GLOBAL PROPERTY ERGO_HARDENING_ACTIVE)
    get_property(_unsafe GLOBAL PROPERTY ERGO_HARDENING_UNSAFE_API)
    get_property(_extended GLOBAL PROPERTY ERGO_HARDENING_EXTENDED)
    foreach(_v IN ITEMS baseline extended)
        set(_t test_lorica_applied)
        if(_v STREQUAL "extended")
            set(_t test_lorica_applied_extended)
        endif()
        add_executable(${_t} "${_root}/tests/hardening/test_lorica_applied.cpp")
        target_include_directories(${_t} PRIVATE "${_root}/third_party")  # mini gtest
        set(_a 0)
        set(_u 0)
        set(_x 0)
        set(_na 0)
        if(_active)
            set(_a 1)
            if(_unsafe)
                set(_u 1)
            endif()
            if(_v STREQUAL "extended")
                if(_extended)
                    set(_x 1)
                else()
                    set(_na 1)
                endif()
            endif()
        endif()
        target_compile_definitions(${_t} PRIVATE
            ERGO_LORICA_ACTIVE=${_a}
            ERGO_LORICA_EXPECT_UNSAFE_API=${_u}
            ERGO_LORICA_EXPECT_EXTENDED=${_x}
            ERGO_LORICA_EXTENDED_UNAVAILABLE=${_na}
            # The configurations Lorica gives _FORTIFY_SOURCE (_lorica_linux_capabilities).
            "ERGO_LORICA_OPTIMIZED_CONFIG=$<IF:$<CONFIG:Release,RelWithDebInfo,MinSizeRel>,1,0>")
        if(_v STREQUAL "extended")
            ergo_hardening_mark_network(${_t})
        endif()
        add_test(NAME ${_t} COMMAND ${_t})
        set_tests_properties(${_t} PROPERTIES SKIP_RETURN_CODE 77 LABELS hardening)
    endforeach()
endfunction()

# The tests that read the finished build. Call after ergo_harden_targets()
# (they read the profile each target ended up with). top_level: Ergo is the
# top-level project, where it turns on CMAKE_EXPORT_COMPILE_COMMANDS.
function(ergo_hardening_add_build_checks top_level)
    get_property(_root GLOBAL PROPERTY ERGO_HARDENING_SOURCE_ROOT)
    get_property(_active GLOBAL PROPERTY ERGO_HARDENING_ACTIVE)
    get_property(_reason GLOBAL PROPERTY ERGO_HARDENING_REASON)
    get_property(_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

    set(_inc "")
    set(_header "")
    set(_lorica_root "")
    set(_extended_targets "")
    set(_skipped "")
    if(_active)
        get_property(_h GLOBAL PROPERTY LORICA_UNSAFE_API_HEADER)
        cmake_path(GET _h PARENT_PATH _inc)       # <lorica>/include/lorica
        cmake_path(GET _inc PARENT_PATH _inc)     # <lorica>/include
        cmake_path(GET _inc PARENT_PATH _lorica_root)
        get_property(_unsafe GLOBAL PROPERTY ERGO_HARDENING_UNSAFE_API)
        if(_unsafe)
            set(_header "${_h}")
        endif()
        _ergo_hardening_compiled_targets(_targets)
        # Comma-separated below: a ';' would split the add_test argument.
        foreach(_t IN LISTS _targets)
            get_target_property(_profile ${_t} LORICA_HARDENING_PROFILE)
            if(_profile STREQUAL "extended")
                list(APPEND _extended_targets ${_t})
            endif()
            get_target_property(_skip ${_t} ERGO_UNSAFE_API_SKIPPED)
            if(_skip)
                list(APPEND _skipped ${_t})
            endif()
        endforeach()
    endif()

    if(top_level)
        string(REPLACE ";" "," _skipped_csv "${_skipped}")
        string(REPLACE ";" "," _extended_csv "${_extended_targets}")
        set(_exports OFF)
        set(_why "${CMAKE_GENERATOR}")
        if(MSVC)
            # cl command lines (/GS /sdl /guard:cf /we4996) are another syntax.
            set(_why "${CMAKE_GENERATOR} (MSVC)")
        elseif(CMAKE_GENERATOR MATCHES "Ninja|Makefiles" AND NOT _multi)
            set(_exports ON)
        endif()
        # Lorica's FORTIFY level per language: 3 from compiler version 12, else 2,
        # in the optimized configurations only.
        set(_fortify_cxx 0)
        set(_fortify_c 0)
        if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
            foreach(_lang CXX C)
                if(CMAKE_${_lang}_COMPILER_LOADED)
                    set(_level 2)
                    if(CMAKE_${_lang}_COMPILER_VERSION VERSION_GREATER_EQUAL 12)
                        set(_level 3)
                    endif()
                    string(TOLOWER "${_lang}" _l)
                    set(_fortify_${_l} ${_level})
                endif()
            endforeach()
        endif()
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
            set(_arch -mbranch-protection=standard)
        else()
            set(_arch -fcf-protection=full)
        endif()
        add_test(NAME test_lorica_compile_commands
            COMMAND ${CMAKE_COMMAND}
                "-DERGO_SOURCE_DIR=${_root}"
                "-DERGO_BINARY_DIR=${CMAKE_BINARY_DIR}"
                "-DERGO_LORICA_ACTIVE=${_active}"
                "-DERGO_LORICA_INACTIVE_REASON=${_reason}"
                "-DERGO_EXPORTS_COMPILE_COMMANDS=${_exports}"
                "-DERGO_GENERATOR=${_why}"
                "-DERGO_LORICA_INCLUDE_DIR=${_inc}"
                "-DERGO_LORICA_UNSAFE_HEADER=${_header}"
                "-DERGO_LORICA_UNSAFE_SKIPPED=${_skipped_csv}"
                "-DERGO_LORICA_EXTENDED_TARGETS=${_extended_csv}"
                "-DERGO_LORICA_ARCH_FLAGS=${_arch}"
                "-DERGO_LORICA_FORTIFY_CXX=${_fortify_cxx}"
                "-DERGO_LORICA_FORTIFY_C=${_fortify_c}"
                -P "${_root}/tests/hardening/test_lorica_compile_commands.cmake")
        set_tests_properties(test_lorica_compile_commands PROPERTIES
            SKIP_REGULAR_EXPRESSION "SKIPPED:" LABELS hardening)
    endif()

    # Lorica's binary inspector (Python standard library only): when python3 is there.
    if(NOT _active)
        return()
    endif()
    set(_tool "${_lorica_root}/tools/verify_binary.py")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    set(_extra "")
    if(NOT Python3_Interpreter_FOUND)
        message(STATUS "ergo: test_lorica_verify_binary_* not registered: python3 not found")
        return()
    elseif(NOT EXISTS "${_tool}")
        message(STATUS "ergo: test_lorica_verify_binary_* not registered: ${_tool} not found (this Lorica has no tools/)")
        return()
    elseif(WIN32 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(STATUS "ergo: test_lorica_verify_binary_* not registered: verify_binary.py reads x64 PE only")
        return()
    elseif(NOT WIN32)
        if(NOT CMAKE_READELF)
            message(STATUS "ergo: test_lorica_verify_binary_* not registered: readelf not found")
            return()
        endif()
        set(_extra --readelf "${CMAKE_READELF}")
    endif()
    set(_n 0)
    foreach(_t IN ITEMS ergo_studio ergo_pkg_cli test_lorica_applied)
        if(TARGET ${_t})
            add_test(NAME test_lorica_verify_binary_${_t}
                COMMAND "${Python3_EXECUTABLE}" -I "${_tool}" ${_extra} "$<TARGET_FILE:${_t}>")
            set_tests_properties(test_lorica_verify_binary_${_t} PROPERTIES LABELS hardening)
            math(EXPR _n "${_n} + 1")
        endif()
    endforeach()
    message(STATUS "ergo: Lorica's verify_binary.py registered on ${_n} executable(s)")
endfunction()
