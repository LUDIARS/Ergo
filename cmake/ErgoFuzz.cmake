# ---------------------------------------------------------------------------
# cmake/ErgoFuzz.cmake — libFuzzer entry points of Ergo's input parsers
#
# ergo_add_fuzz_entry(<name> SOURCE <file> LIBS <targets>... [CORPUS <dir>])
#   - With ERGO_BUILD_TESTS: fuzz_regress_<name>, the entry built with any
#     compiler against tests/fuzz_main.cpp and run once per seed in <dir> (and
#     a few built-in edge inputs) as the ctest test fuzz_regress_<name>.
#   - With ERGO_BUILD_FUZZERS (Clang only, default OFF): fuzz_<name>, a
#     libFuzzer binary (-fsanitize=fuzzer; the LIBS get -fsanitize=fuzzer-no-link
#     for coverage). The build must be an ASan + UBSan build as a whole
#     (CMAKE_CXX_FLAGS / CMAKE_EXE_LINKER_FLAGS with -fsanitize=address,undefined,
#     as the ci-asan preset), so every executable linking an instrumented
#     library also links the sanitizer runtime. Run it by hand:
#       <build>/fuzz_<name> -max_total_time=30 <corpus copy>
# See spec/setup/hardening.md.
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

option(ERGO_BUILD_FUZZERS "Build libFuzzer entry points of Ergo's parsers (Clang only, test use)" OFF)
if(ERGO_BUILD_FUZZERS AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "ERGO_BUILD_FUZZERS=ON needs Clang (libFuzzer); this is ${CMAKE_CXX_COMPILER_ID}. No silent fallback.")
endif()
if(ERGO_BUILD_FUZZERS AND NOT (CMAKE_CXX_FLAGS MATCHES "-fsanitize=[^ ]*address" AND CMAKE_EXE_LINKER_FLAGS MATCHES "-fsanitize=[^ ]*address"))
    message(FATAL_ERROR "ERGO_BUILD_FUZZERS=ON needs an ASan build: set CMAKE_CXX_FLAGS and CMAKE_EXE_LINKER_FLAGS "
        "to include -fsanitize=address,undefined (see spec/setup/hardening.md).")
endif()

function(_ergo_fuzz_instrument target)
    get_property(_done TARGET ${target} PROPERTY ERGO_FUZZ_INSTRUMENTED)
    if(NOT _done)
        target_compile_options(${target} PRIVATE -fsanitize=fuzzer-no-link)
        set_property(TARGET ${target} PROPERTY ERGO_FUZZ_INSTRUMENTED TRUE)
    endif()
endfunction()

function(ergo_add_fuzz_entry name)
    cmake_parse_arguments(F "" "SOURCE;CORPUS" "LIBS" ${ARGN})
    if(NOT F_SOURCE OR NOT F_LIBS)
        message(FATAL_ERROR "ergo_add_fuzz_entry(${name}): SOURCE and LIBS are required")
    endif()
    if(ERGO_BUILD_TESTS)
        add_executable(fuzz_regress_${name} tests/fuzz_main.cpp ${F_SOURCE})
        target_link_libraries(fuzz_regress_${name} PRIVATE ${F_LIBS})
        if(MSVC)
            target_compile_options(fuzz_regress_${name} PRIVATE /utf-8)
        endif()
        set(_corpus "")
        if(F_CORPUS)
            set(_corpus "${CMAKE_CURRENT_SOURCE_DIR}/${F_CORPUS}")
        endif()
        add_test(NAME fuzz_regress_${name} COMMAND fuzz_regress_${name} ${_corpus})
    endif()
    if(ERGO_BUILD_FUZZERS)
        add_executable(fuzz_${name} ${F_SOURCE})
        target_link_libraries(fuzz_${name} PRIVATE ${F_LIBS})
        target_compile_options(fuzz_${name} PRIVATE -fsanitize=fuzzer)
        target_link_options(fuzz_${name} PRIVATE -fsanitize=fuzzer)
        foreach(_lib IN LISTS F_LIBS)
            _ergo_fuzz_instrument(${_lib})
        endforeach()
    endif()
endfunction()
