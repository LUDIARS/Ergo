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

# lesson_action_feel — headless sample for the Odeum lesson link (spec/feature/tool/lesson.md, "サンプル"):
# a simulated player hitting an enemy, publishing feel.* through ergo_bind; hit stop through ergo_world_time.
set(_lesson_feel_needs ergo_bind ergo_world_time)
set(_lesson_feel_missing "")
foreach(_tgt IN LISTS _lesson_feel_needs)
    if(NOT TARGET ${_tgt})
        list(APPEND _lesson_feel_missing ${_tgt})
    endif()
endforeach()

if(_lesson_feel_missing)
    list(JOIN _lesson_feel_missing ", " _lesson_feel_missing_str)
    message(STATUS "skip example lesson_action_feel: needs ${_lesson_feel_missing_str}")
else()
    add_library(ergo_example_lesson_action_feel_sim STATIC lesson_action_feel/action_feel_sim.cpp)
    target_include_directories(ergo_example_lesson_action_feel_sim PUBLIC lesson_action_feel)
    target_link_libraries(ergo_example_lesson_action_feel_sim PUBLIC ergo_world_time)

    add_executable(ergo_example_lesson_action_feel lesson_action_feel/main.cpp)
    target_link_libraries(ergo_example_lesson_action_feel PRIVATE ergo_example_lesson_action_feel_sim ergo_bind)
    if(MSVC)
        target_compile_options(ergo_example_lesson_action_feel_sim PRIVATE /utf-8)
        target_compile_options(ergo_example_lesson_action_feel PRIVATE /utf-8)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_lesson_action_feel lesson_action_feel/test_action_feel_sim.cpp)
        target_link_libraries(test_lesson_action_feel PRIVATE ergo_example_lesson_action_feel_sim ergo_gtest_main)
        add_test(NAME test_lesson_action_feel COMMAND test_lesson_action_feel)
        add_test(NAME example_lesson_action_feel COMMAND ergo_example_lesson_action_feel --check)
    endif()
endif()
