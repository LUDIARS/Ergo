# ---------------------------------------------------------------------------
# examples/ — headless composition examples. They are consumers of the public
# ergo_* targets (like a host game), not modules, and need no Pictor / Vulkan.
# Included from the top-level CMakeLists.txt after every module target, so the
# if(TARGET ...) checks below see the final module set. A missing module skips
# the example with a STATUS line (never silently).
# ---------------------------------------------------------------------------

# compose_loop — one per-frame loop wiring input / character / actor / cast /
# world_time / health / combo_counter / score / timing_judge / blackboard /
# log / frame / profile / bind / audio (spec/setup/integrate-modules.md).
set(_compose_loop_needs
    ergo_input ergo_character ergo_actor ergo_bind ergo_cast ergo_world_time
    ergo_health ergo_combo_counter ergo_score ergo_timing_judge ergo_blackboard
    ergo_log ergo_frame ergo_profile ergo_audio)
set(_compose_loop_missing "")
foreach(_tgt IN LISTS _compose_loop_needs)
    if(NOT TARGET ${_tgt})
        list(APPEND _compose_loop_missing ${_tgt})
    endif()
endforeach()

if(_compose_loop_missing)
    list(JOIN _compose_loop_missing ", " _compose_loop_missing_str)
    message(STATUS "skip example compose_loop: needs ${_compose_loop_missing_str}")
else()
    add_executable(ergo_example_compose_loop compose_loop/main.cpp)
    target_link_libraries(ergo_example_compose_loop PRIVATE ${_compose_loop_needs})
    # Several module headers contain non-ASCII comments; MSVC reads BOM-less
    # UTF-8 as the system code page without /utf-8 (spec/setup/build-cpp.md).
    if(MSVC)
        target_compile_options(ergo_example_compose_loop PRIVATE /utf-8)
    endif()

    if(ERGO_BUILD_TESTS)
        add_test(NAME example_compose_loop COMMAND ergo_example_compose_loop --check)
        # The trace run writes compose_loop.trace.json; the follow-up test
        # parses it (string(JSON), no Python) so CI proves it is valid JSON
        # with a traceEvents array the tools/ergo profile plugin can load.
        set(_compose_loop_trace ${CMAKE_CURRENT_BINARY_DIR}/compose_loop.trace.json)
        add_test(NAME example_compose_loop_trace
                 COMMAND ergo_example_compose_loop --check --trace ${_compose_loop_trace})
        set_tests_properties(example_compose_loop_trace PROPERTIES
                             FIXTURES_SETUP ergo_example_compose_loop_trace)
        add_test(NAME example_compose_loop_trace_json
                 COMMAND ${CMAKE_COMMAND} -DTRACE=${_compose_loop_trace}
                         -P ${CMAKE_CURRENT_SOURCE_DIR}/check_trace.cmake)
        set_tests_properties(example_compose_loop_trace_json PROPERTIES
                             FIXTURES_REQUIRED ergo_example_compose_loop_trace)
    endif()
endif()
