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

# lesson_action_feel — the Odeum lesson's sample game (spec/feature/tool/lesson-action-game.md):
# core/ = the pure arena game (hit stop through ergo_world_time, HP through ergo_health, score / combo through
# ergo_score / ergo_combo_counter), terminal/ = the playable terminal front-end, the rest = the executable
# (headless --auto with the Studio link through ergo_bind, --play, --check).
set(_lesson_feel_needs ergo_bind ergo_world_time ergo_health ergo_score ergo_combo_counter)
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
    set(_lf lesson_action_feel)
    add_library(ergo_example_lesson_action_feel_core STATIC
        ${_lf}/core/action_game.cpp
        ${_lf}/core/autopilot.cpp
        ${_lf}/core/boss_blur.cpp
        ${_lf}/core/boss_brain.cpp
        ${_lf}/core/combat_rules.cpp
        ${_lf}/core/combat_settings.cpp
        ${_lf}/core/effect_observations.cpp
        ${_lf}/core/enemy_brain.cpp
        ${_lf}/core/feel_effects.cpp
        ${_lf}/core/motion_blur.cpp
        ${_lf}/core/presentation.cpp
        ${_lf}/core/session.cpp
        ${_lf}/core/wave_plan.cpp)
    target_include_directories(ergo_example_lesson_action_feel_core PUBLIC ${_lf}/core)
    target_link_libraries(ergo_example_lesson_action_feel_core
        PUBLIC ergo_world_time ergo_health ergo_score ergo_combo_counter)

    if(WIN32)
        # The playable terminal front-end is POSIX only: --play stops with a message on Windows.
        message(STATUS "example lesson_action_feel: --play (terminal front-end) is not available on Windows")
        set(_lf_console ${_lf}/terminal/raw_terminal_unsupported.cpp)
    else()
        set(_lf_console ${_lf}/terminal/raw_terminal_posix.cpp)
    endif()
    add_library(ergo_example_lesson_action_feel_terminal STATIC
        ${_lf}/terminal/ansi_frame.cpp
        ${_lf}/terminal/key_bindings.cpp
        ${_lf}/terminal/key_decoder.cpp
        ${_lf}/terminal/move_latch.cpp
        ${_lf}/terminal/screen.cpp
        ${_lf}/terminal/screen_effects.cpp
        ${_lf_console})
    target_include_directories(ergo_example_lesson_action_feel_terminal PUBLIC ${_lf})
    target_link_libraries(ergo_example_lesson_action_feel_terminal PUBLIC ergo_example_lesson_action_feel_core)

    add_library(ergo_example_lesson_action_feel_app STATIC
        ${_lf}/check_mode.cpp
        ${_lf}/frame_pacer.cpp
        ${_lf}/headless_mode.cpp
        ${_lf}/lesson_bindings.cpp
        ${_lf}/options.cpp
        ${_lf}/play_mode.cpp
        ${_lf}/settings_watch.cpp)
    target_include_directories(ergo_example_lesson_action_feel_app PUBLIC ${_lf})
    target_link_libraries(ergo_example_lesson_action_feel_app
        PUBLIC ergo_example_lesson_action_feel_core ergo_example_lesson_action_feel_terminal ergo_bind)

    add_executable(ergo_example_lesson_action_feel ${_lf}/main.cpp)
    target_link_libraries(ergo_example_lesson_action_feel PRIVATE ergo_example_lesson_action_feel_app)
    if(MSVC)
        foreach(_t ergo_example_lesson_action_feel_core ergo_example_lesson_action_feel_terminal
                   ergo_example_lesson_action_feel_app ergo_example_lesson_action_feel)
            target_compile_options(${_t} PRIVATE /utf-8)
        endforeach()
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_lesson_action_feel
            ${_lf}/tests/test_action_game.cpp
            ${_lf}/tests/test_boss.cpp
            ${_lf}/tests/test_combat_rules.cpp
            ${_lf}/tests/test_presentation.cpp
            ${_lf}/tests/test_wave_plan.cpp)
        target_link_libraries(test_lesson_action_feel PRIVATE ergo_example_lesson_action_feel_core ergo_gtest_main)
        add_test(NAME test_lesson_action_feel COMMAND test_lesson_action_feel)

        add_executable(test_lesson_action_feel_frontend
            ${_lf}/tests/test_options.cpp
            ${_lf}/tests/test_terminal_effects.cpp
            ${_lf}/tests/test_terminal_input.cpp
            ${_lf}/tests/test_terminal_screen.cpp)
        target_link_libraries(test_lesson_action_feel_frontend PRIVATE ergo_example_lesson_action_feel_app ergo_gtest_main)
        add_test(NAME test_lesson_action_feel_frontend COMMAND test_lesson_action_feel_frontend)

        add_test(NAME example_lesson_action_feel COMMAND ergo_example_lesson_action_feel --check)
    endif()
endif()
