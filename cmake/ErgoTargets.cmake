# ---------------------------------------------------------------------------
# cmake/ErgoTargets.cmake — what every Ergo library promises its consumers
#
# ergo_finalize_targets() is called once, at the end of the top-level
# CMakeLists.txt, and walks every ergo_* library defined in that directory
# (third_party/dependencies.cmake is include()d, so ergo_thirdparty counts too).
# New modules get all of this without editing this file.
#
#   - Ergo::<name> ALIAS (ergo_log -> Ergo::log, ergo_thirdparty ->
#     Ergo::thirdparty; ergo::thirdparty stays). A typo or a module that is
#     OFF is then a configure error naming the target, not "-lergo_<x>" at
#     link time.
#   - The C++ standard as a usage requirement: cxx_std_17, and cxx_std_20 for
#     ergo_math (C++20 concepts in its public headers; ergo_physics2d and any
#     other consumer inherit it through the link). A host left at C++14, MSVC's
#     default, is raised automatically. CMAKE_CXX_STANDARD in CMakeLists.txt
#     only covers Ergo's own directory.
#   - MSVC: /utf-8 as a usage requirement (public headers carry UTF-8
#     Japanese comments; MSVC reads BOM-less files as the ANSI code page).
#
# ergo_gtest_main (test-only) is skipped; executables are not libraries.
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

function(ergo_finalize_targets)
    get_property(_targets DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_t IN LISTS _targets)
        if(_t STREQUAL "ergo_gtest_main" OR NOT _t MATCHES "^ergo_(.+)$")
            continue()
        endif()
        set(_name "${CMAKE_MATCH_1}")
        get_target_property(_type ${_t} TYPE)
        if(_type STREQUAL "INTERFACE_LIBRARY")
            set(_scope INTERFACE)
        elseif(_type MATCHES "^(STATIC|SHARED|OBJECT)_LIBRARY$")
            set(_scope PUBLIC)
        else()
            continue()
        endif()

        add_library(Ergo::${_name} ALIAS ${_t})

        set(_std 17)
        if(_t STREQUAL "ergo_math")
            set(_std 20)
        endif()
        target_compile_features(${_t} ${_scope} cxx_std_${_std})
        if(MSVC)
            target_compile_options(${_t} ${_scope} /utf-8)
        endif()
    endforeach()
endfunction()
