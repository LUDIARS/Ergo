cmake_minimum_required(VERSION 3.16)
project(Ergo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Top-level options. Hosts can flip per-module knobs as needed.
# ---------------------------------------------------------------------------
option(ERGO_BUILD_TESTS       "Build module tests"             ON)
option(ERGO_BUILD_DUMMY       "Build dummy plug libraries"     ON)
option(ERGO_BUILD_BENCHMARKS  "Build micro-benchmark executables (opt-in)" OFF)

option(ERGO_BUILD_INPUT        "Build ergo_input"        ON)
option(ERGO_BUILD_PARTICLE     "Build ergo_particle"     ON)
option(ERGO_BUILD_GPU_PARTICLE "Build ergo_gpu_particle" ON)
option(ERGO_BUILD_BIND         "Build ergo_bind"         ON)
option(ERGO_BUILD_ACTOR        "Build ergo_actor"        ON)
option(ERGO_BUILD_SOUND        "Build ergo_sound"        ON)
option(ERGO_BUILD_FRAME        "Build ergo_frame"        ON)
option(ERGO_BUILD_PROFILE      "Build ergo_profile (performance timeline)" ON)
option(ERGO_BUILD_LOG          "Build ergo_log"          ON)
option(ERGO_BUILD_IO           "Build ergo_io"           ON)
option(ERGO_BUILD_AUDIO        "Build ergo_audio"        ON)
option(ERGO_BUILD_WORLD_TIME   "Build ergo_world_time"   ON)
option(ERGO_BUILD_BLACKBOARD   "Build ergo_blackboard"   ON)
option(ERGO_BUILD_UI           "Build ergo_ui (SVG raster + 9-slice)" ON)
option(ERGO_BUILD_CUSTOS       "Build ergo_custos (Custos remote-test bridge)" ON)
option(ERGO_BUILD_HEALTH       "Build ergo_health"        ON)
option(ERGO_BUILD_SCORE        "Build ergo_score"         ON)
option(ERGO_BUILD_COMBO_COUNTER "Build ergo_combo_counter" ON)
option(ERGO_BUILD_TIMING_JUDGE "Build ergo_timing_judge"  ON)
option(ERGO_BUILD_UI_KIT       "Build ergo_ui_kit (retained-mode UI framework)" ON)
option(ERGO_BUILD_UI_LAYOUT    "Build ergo_ui_layout (data-driven UI layout runtime)" ON)
option(ERGO_BUILD_SHURIKEN_MIGRATOR "Build ergo_shuriken_migrator (Unity Shuriken → ergo_particle migrator + CLI)" ON)
option(ERGO_BUILD_RENDER       "Build ergo_render (Pictor 横断オーケストレーション層)" ON)
option(ERGO_BUILD_VECTOR       "Build ergo_vector (SVG path mesh + extrude + animation)" ON)
option(ERGO_BUILD_CAST         "Build ergo_cast (Actor bundling Scene; render-independent)" ON)
option(ERGO_BUILD_SCENE        "Build ergo_scene (serializable look-dev Scene document)" ON)
option(ERGO_BUILD_CHARACTER    "Build ergo_character (TCC-style character controller)" ON)
option(ERGO_BUILD_MATH         "Build ergo_math (C++20 generic math: vec/mat/batch/pool)" ON)
option(ERGO_BUILD_PHYSICS2D    "Build ergo_physics2d (2D rigid body, Box2D-like)" ON)

# Per-module knobs (passed through to module sections below)
option(ERGO_PARTICLE_HAS_RENDERER        "ergo_particle: build Vulkan billboard renderer (requires pictor + Vulkan)" OFF)
option(ERGO_GPU_PARTICLE_COMPILE_SHADERS "ergo_gpu_particle: bake GLSL compute shaders to SPIR-V via glslc" ON)

# ergo_audio backend selector. "auto" probes for FMOD via the bundled
# FindFMOD.cmake and falls back to dummy; "dummy" forces no-op; "fmod"
# hard-requires the SDK and errors if not found.
set(ERGO_AUDIO_BACKEND "auto" CACHE STRING "ergo_audio backend: auto / fmod / dummy")
set_property(CACHE ERGO_AUDIO_BACKEND PROPERTY STRINGS auto fmod dummy)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# ergo_common — small utility lib shared across modules. Currently just the
# minimal JSON codec (used by ergo_bind).
# ---------------------------------------------------------------------------
add_library(ergo_common STATIC src/common/json_min.cpp)
target_include_directories(ergo_common PUBLIC include)

# ---------------------------------------------------------------------------
# Shared mini-gtest (header-only), bound into one static lib used by all tests
# ---------------------------------------------------------------------------
if(ERGO_BUILD_TESTS)
    enable_testing()
    add_library(ergo_gtest_main STATIC tests/gtest_main.cpp)
    target_include_directories(ergo_gtest_main PUBLIC third_party)
endif()

# ---------------------------------------------------------------------------
# ergo_input — unified input (mouse/keyboard/gamepad/USB HID)
# ---------------------------------------------------------------------------
if(ERGO_BUILD_INPUT)
    add_library(ergo_input
        src/input/input_buffer.cpp
        src/input/observer.cpp
        src/input/polling_thread.cpp
        src/input/mouse_device.cpp
        src/input/keyboard_device.cpp
        src/input/gamepad_device.cpp
        src/input/usb_device.cpp
        src/input/input_system.cpp
    )
    target_include_directories(ergo_input PUBLIC include)
    target_link_libraries(ergo_input PUBLIC Threads::Threads)
    if(ERGO_BUILD_LOG)
        # keyboard_device.cpp で ERGO_LOG_INFO を使うため依存を立てる。
        target_link_libraries(ergo_input PUBLIC ergo_log)
    endif()

    if(ERGO_BUILD_DUMMY)
        add_library(ergo_input_dummy
            src/input/mouse_device_dummy.cpp
            src/input/keyboard_device_dummy.cpp
            src/input/gamepad_device_dummy.cpp
            src/input/usb_device_dummy.cpp
            src/input/input_system_dummy.cpp
            src/input/input_buffer_dummy.cpp
            src/input/double_buffer_dummy.cpp
            src/input/observer_dummy.cpp
            src/input/polling_thread_dummy.cpp
        )
        target_include_directories(ergo_input_dummy PUBLIC include)
        target_link_libraries(ergo_input_dummy PUBLIC Threads::Threads)
    endif()

    if(ERGO_BUILD_TESTS)
        foreach(NAME
                test_double_buffer test_input_buffer test_observer test_polling_thread
                test_mouse_device test_keyboard_device test_gamepad_device test_usb_device
                test_input_system)
            add_executable(${NAME} tests/input/${NAME}.cpp)
            target_link_libraries(${NAME} PRIVATE ergo_input ergo_gtest_main)
            add_test(NAME ${NAME} COMMAND ${NAME})
        endforeach()
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_particle — CPU sim + (optional) Pictor Vulkan billboard renderer
# ---------------------------------------------------------------------------
if(ERGO_BUILD_PARTICLE)
    set(_PART_SRC
        src/particle/effect_config.cpp
        src/particle/particle_system.cpp
    )
    add_library(ergo_particle ${_PART_SRC})
    target_include_directories(ergo_particle PUBLIC include)
    target_link_libraries(ergo_particle PUBLIC Threads::Threads)

    if(TARGET pictor AND ERGO_PARTICLE_HAS_RENDERER)
        find_package(Vulkan QUIET)
        if(Vulkan_FOUND)
            target_sources(ergo_particle PRIVATE src/particle/renderer.cpp)
            target_link_libraries(ergo_particle PUBLIC pictor Vulkan::Vulkan)
            target_compile_definitions(ergo_particle PUBLIC ERGO_PARTICLE_HAS_RENDERER=1)
            if(Vulkan_GLSLC_EXECUTABLE)
                set(_PART_SHADERS
                    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.vert
                    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.frag
                )
                set(_PART_SPIRV)
                foreach(SHADER ${_PART_SHADERS})
                    get_filename_component(N ${SHADER} NAME)
                    set(OUT ${CMAKE_BINARY_DIR}/shaders/${N}.spv)
                    add_custom_command(
                        OUTPUT ${OUT}
                        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
                        COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${OUT}
                        DEPENDS ${SHADER}
                        COMMENT "Compiling ${N} -> SPIR-V")
                    list(APPEND _PART_SPIRV ${OUT})
                endforeach()
                add_custom_target(ergo_particle_shaders ALL DEPENDS ${_PART_SPIRV})
                add_dependencies(ergo_particle ergo_particle_shaders)
            endif()
            message(STATUS "ergo_particle: renderer enabled (Pictor + Vulkan)")
        else()
            message(STATUS "ergo_particle: renderer requested but Vulkan not found")
        endif()
    endif()

    if(ERGO_BUILD_DUMMY)
        add_library(ergo_particle_dummy src/particle/particle_dummy.cpp)
        target_include_directories(ergo_particle_dummy PUBLIC include)
        target_link_libraries(ergo_particle_dummy PUBLIC Threads::Threads)
    endif()

    if(ERGO_BUILD_TESTS)
        foreach(NAME test_effect_config test_particle_system)
            add_executable(${NAME} tests/particle/${NAME}.cpp)
            target_link_libraries(${NAME} PRIVATE ergo_particle ergo_gtest_main)
            add_test(NAME ${NAME} COMMAND ${NAME})
        endforeach()
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_bind — outbound WS client publishing host vars to the unified
# ergo tool's `variable` plugin (tools/ergo/)
# ---------------------------------------------------------------------------
if(ERGO_BUILD_BIND)
    add_library(ergo_bind
        src/bind/types.cpp
        src/bind/bind_engine.cpp
        src/bind/ws_client.cpp
    )
    target_include_directories(ergo_bind PUBLIC include)
    target_compile_definitions(ergo_bind PUBLIC ERGO_BIND_ENABLED=1)
    target_link_libraries(ergo_bind PUBLIC ergo_common Threads::Threads)
    if(WIN32)
        target_link_libraries(ergo_bind PUBLIC ws2_32)
    endif()
    # VarKind is a closed wire-protocol enum (no runtime converter registry by
    # design). Guard against it drifting: any switch over an enum that omits a
    # case with no default label is a hard error, so adding a new VarKind fails
    # to compile until every codec switch (value_to_json / json_to_value /
    # Value::equals / to_string) handles it.
    if(MSVC)
        target_compile_options(ergo_bind PRIVATE /we4062)
    else()
        target_compile_options(ergo_bind PRIVATE -Werror=switch)
    endif()

    if(ERGO_BUILD_DUMMY)
        add_library(ergo_bind_dummy src/bind/bind_dummy.cpp)
        target_include_directories(ergo_bind_dummy PUBLIC include)
        target_link_libraries(ergo_bind_dummy PUBLIC Threads::Threads)
    endif()

    if(ERGO_BUILD_TESTS)
        foreach(NAME test_types test_json_min test_bind_engine)
            add_executable(${NAME} tests/bind/${NAME}.cpp)
            target_link_libraries(${NAME} PRIVATE ergo_bind ergo_gtest_main)
            add_test(NAME ${NAME} COMMAND ${NAME})
        endforeach()
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_actor — scene-graph binding base class (depends on ergo_bind)
# ---------------------------------------------------------------------------
if(ERGO_BUILD_ACTOR AND ERGO_BUILD_BIND)
    add_library(ergo_actor src/actor/actor.cpp)
    target_include_directories(ergo_actor PUBLIC include)
    target_link_libraries(ergo_actor PUBLIC ergo_bind)

    if(ERGO_BUILD_DUMMY)
        add_library(ergo_actor_dummy src/actor/actor_dummy.cpp)
        target_include_directories(ergo_actor_dummy PUBLIC include)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_actor tests/actor/test_actor.cpp)
        target_link_libraries(test_actor PRIVATE ergo_actor ergo_gtest_main)
        add_test(NAME test_actor COMMAND test_actor)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_cast - bundles Actors into a named Scene for batch lifecycle mgmt.
#   Non-owning (holds raw Actor*, never deletes) and render-independent
#   (includes neither Pictor nor ergo_render). Provides bulk + per-actor
#   activate/deactivate and snapshot/restore only. Sits on top of ergo_actor
#   (Actor is a foundational base type, so this is not a layering violation;
#   ergo_scene likewise builds on Actor).
# ---------------------------------------------------------------------------
if(ERGO_BUILD_CAST AND ERGO_BUILD_ACTOR AND ERGO_BUILD_BIND)
    add_library(ergo_cast src/cast/scene.cpp)
    target_include_directories(ergo_cast PUBLIC include)
    target_link_libraries(ergo_cast PUBLIC ergo_actor)

    if(ERGO_BUILD_TESTS)
        add_executable(test_scene_cast tests/cast/test_scene.cpp)
        target_link_libraries(test_scene_cast PRIVATE ergo_cast ergo_gtest_main)
        add_test(NAME test_scene_cast COMMAND test_scene_cast)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_sound — WAV/OGG decode, streaming, mixing engine, waveform, quantizer
# ---------------------------------------------------------------------------
# ergo_scene - serializable look-dev scene document.
if(ERGO_BUILD_SCENE)
    add_library(ergo_scene src/scene/scene.cpp)
    target_include_directories(ergo_scene PUBLIC include)
    target_link_libraries(ergo_scene PUBLIC ergo_common)

    if(ERGO_BUILD_TESTS)
        add_executable(test_scene tests/scene/test_scene.cpp)
        target_link_libraries(test_scene PRIVATE ergo_scene ergo_gtest_main)
        add_test(NAME test_scene COMMAND test_scene)
    endif()
endif()

if(ERGO_BUILD_SOUND)
    add_library(ergo_sound
        src/sound/audio_decoder.cpp
        src/sound/audio_stream.cpp
        src/sound/audio_engine.cpp
        src/sound/waveform_processor.cpp
        src/sound/quantizer.cpp
    )
    target_include_directories(ergo_sound PUBLIC include)
    target_link_libraries(ergo_sound PUBLIC Threads::Threads)
    # MSVC: enable M_PI et al from <cmath>. Propagated to dependents
    # (notably the sound tests that sample sine waves).
    target_compile_definitions(ergo_sound PUBLIC _USE_MATH_DEFINES)

    if(ERGO_BUILD_DUMMY)
        add_library(ergo_sound_dummy src/sound/sound_dummy.cpp)
        target_include_directories(ergo_sound_dummy PUBLIC include)
        target_link_libraries(ergo_sound_dummy PUBLIC Threads::Threads)
    endif()

    if(ERGO_BUILD_TESTS)
        foreach(NAME
                test_audio_decoder test_audio_stream test_audio_engine
                test_waveform_processor test_quantizer)
            add_executable(${NAME} tests/sound/${NAME}.cpp)
            target_link_libraries(${NAME} PRIVATE ergo_sound ergo_gtest_main)
            add_test(NAME ${NAME} COMMAND ${NAME})
        endforeach()
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_gpu_particle — Shuriken-style GPU compute particle module.
# Pure C++ module; GPU backend is an abstract interface (`IGpuBackend`)
# supplied by host integration layers (Vulkan / WebGPU). Compute shaders
# are baked to SPIR-V when glslc is available.
# ---------------------------------------------------------------------------
if(ERGO_BUILD_GPU_PARTICLE)
    add_library(ergo_gpu_particle
        src/gpu_particle/curve.cpp
        src/gpu_particle/emitter_descriptor.cpp
        src/gpu_particle/particle_system.cpp
    )
    target_include_directories(ergo_gpu_particle PUBLIC include)

    if(ERGO_BUILD_DUMMY)
        add_library(ergo_gpu_particle_dummy
            src/gpu_particle/curve_dummy.cpp
            src/gpu_particle/emitter_descriptor_dummy.cpp
            src/gpu_particle/particle_system_dummy.cpp
        )
        target_include_directories(ergo_gpu_particle_dummy PUBLIC include)
    endif()

    if(ERGO_GPU_PARTICLE_COMPILE_SHADERS)
        find_package(Vulkan QUIET COMPONENTS glslc)
        if(Vulkan_GLSLC_EXECUTABLE)
            set(_GPU_PART_SHADERS
                ${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle_spawn.comp
                ${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle_update.comp
            )
            set(_GPU_PART_SPV)
            foreach(SRC ${_GPU_PART_SHADERS})
                get_filename_component(N ${SRC} NAME)
                set(OUT ${CMAKE_BINARY_DIR}/shaders/${N}.spv)
                add_custom_command(
                    OUTPUT  ${OUT}
                    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
                    COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SRC} -o ${OUT}
                    DEPENDS ${SRC}
                    COMMENT "ergo_gpu_particle: compiling ${N} -> SPIR-V")
                list(APPEND _GPU_PART_SPV ${OUT})
            endforeach()
            add_custom_target(ergo_gpu_particle_shaders ALL DEPENDS ${_GPU_PART_SPV})
            add_dependencies(ergo_gpu_particle ergo_gpu_particle_shaders)
        else()
            message(STATUS "ergo_gpu_particle: glslc not found — SPIR-V bake skipped")
        endif()
    endif()

    if(ERGO_BUILD_TESTS)
        foreach(NAME test_curve test_emitter_descriptor test_particle_system)
            # test_particle_system name collision with ergo_particle — disambiguate.
            if("${NAME}" STREQUAL "test_particle_system")
                set(TGT gpu_test_particle_system)
            else()
                set(TGT ${NAME})
            endif()
            add_executable(${TGT} tests/gpu_particle/${NAME}.cpp)
            target_link_libraries(${TGT} PRIVATE ergo_gpu_particle ergo_gtest_main)
            add_test(NAME ${TGT} COMMAND ${TGT})
        endforeach()
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_frame — per-frame tick counter + rolling FPS
# ---------------------------------------------------------------------------
if(ERGO_BUILD_FRAME)
    add_library(ergo_frame src/frame/counter.cpp)
    target_include_directories(ergo_frame PUBLIC include)

    if(ERGO_BUILD_DUMMY)
        add_library(ergo_frame_dummy src/frame/frame_dummy.cpp)
        target_include_directories(ergo_frame_dummy PUBLIC include)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_counter tests/frame/test_counter.cpp)
        target_link_libraries(test_counter PRIVATE ergo_frame ergo_gtest_main)
        add_test(NAME test_counter COMMAND test_counter)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_profile — performance timeline via AOP-style marker injection
# ---------------------------------------------------------------------------
if(ERGO_BUILD_PROFILE)
    add_library(ergo_profile STATIC src/profile/profile.cpp)
    target_include_directories(ergo_profile PUBLIC include)
    target_compile_definitions(ergo_profile PUBLIC ERGO_PROFILE_ENABLED=1)
    target_link_libraries(ergo_profile PUBLIC Threads::Threads)
    if(WIN32)
        # GetProcessMemoryInfo (RSS sampling) lives in Psapi.
        target_link_libraries(ergo_profile PRIVATE psapi)
    endif()

    if(ERGO_BUILD_DUMMY)
        add_library(ergo_profile_dummy src/profile/profile_dummy.cpp)
        target_include_directories(ergo_profile_dummy PUBLIC include)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_profile tests/profile/test_profile.cpp)
        target_link_libraries(test_profile PRIVATE ergo_profile ergo_gtest_main)
        target_link_libraries(test_profile PRIVATE Threads::Threads)
        add_test(NAME test_profile COMMAND test_profile)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_log — 4-level logger with frame-number prefix (injectable source)
# ---------------------------------------------------------------------------
if(ERGO_BUILD_LOG)
    add_library(ergo_log src/log/log.cpp)
    target_include_directories(ergo_log PUBLIC include)
    target_link_libraries(ergo_log PUBLIC Threads::Threads)

    if(ERGO_BUILD_DUMMY)
        add_library(ergo_log_dummy src/log/log_dummy.cpp)
        target_include_directories(ergo_log_dummy PUBLIC include)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_log tests/log/test_log.cpp)
        target_link_libraries(test_log PRIVATE ergo_log ergo_gtest_main)
        target_link_libraries(test_log PRIVATE Threads::Threads)
        add_test(NAME test_log COMMAND test_log)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_io — minimal file / path I/O wrapper (thin over <filesystem>)
# ---------------------------------------------------------------------------
if(ERGO_BUILD_IO)
    add_library(ergo_io src/io/file.cpp)
    target_include_directories(ergo_io PUBLIC include)
    # GCC < 9 splits <filesystem> into its own library; MSVC / Clang / newer
    # GCC fold it into libstdc++. Link unconditionally on GCC to stay safe.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(ergo_io PUBLIC stdc++fs)
    endif()

    if(ERGO_BUILD_DUMMY)
        add_library(ergo_io_dummy src/io/file_dummy.cpp)
        target_include_directories(ergo_io_dummy PUBLIC include)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_file tests/io/test_file.cpp)
        target_link_libraries(test_file PRIVATE ergo_io ergo_gtest_main)
        add_test(NAME test_file COMMAND test_file)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_audio — game audio wrapper. FMOD Core by default (auto-detected);
# dummy backend when the SDK is missing so hosts link unconditionally.
# ---------------------------------------------------------------------------
if(ERGO_BUILD_AUDIO)
    set(_ERGO_AUDIO_USE_FMOD OFF)
    if(NOT ERGO_AUDIO_BACKEND STREQUAL "dummy")
        if(ERGO_AUDIO_BACKEND STREQUAL "fmod")
            find_package(FMOD REQUIRED)
        else()
            find_package(FMOD QUIET)
        endif()
        if(FMOD_FOUND)
            set(_ERGO_AUDIO_USE_FMOD ON)
        endif()
    endif()

    if(_ERGO_AUDIO_USE_FMOD)
        add_library(ergo_audio src/audio/audio_engine_fmod.cpp)
        target_link_libraries(ergo_audio PUBLIC FMOD::FMOD Threads::Threads)
        target_compile_definitions(ergo_audio PUBLIC ERGO_AUDIO_BACKEND_FMOD=1)
        message(STATUS "ergo_audio: backend = FMOD (${FMOD_LIBRARY})")
    else()
        add_library(ergo_audio src/audio/audio_engine_dummy.cpp)
        target_compile_definitions(ergo_audio PUBLIC ERGO_AUDIO_BACKEND_DUMMY=1)
        if(ERGO_AUDIO_BACKEND STREQUAL "fmod")
            message(FATAL_ERROR "ergo_audio: ERGO_AUDIO_BACKEND=fmod but FMOD SDK not found. Set FMOD_SDK_DIR.")
        endif()
        message(STATUS "ergo_audio: backend = Dummy (set FMOD_SDK_DIR + reconfigure for FMOD)")
    endif()
    target_include_directories(ergo_audio PUBLIC include)

    if(ERGO_BUILD_TESTS)
        # Name-disambiguated from ergo_sound's existing test_audio_engine.
        add_executable(test_ergo_audio tests/audio/test_ergo_audio.cpp)
        target_link_libraries(test_ergo_audio PRIVATE ergo_audio ergo_gtest_main)
        add_test(NAME test_ergo_audio COMMAND test_ergo_audio)
    endif()

    # Host helper: stage the FMOD runtime DLL next to an executable so it
    # can load at launch time without PATH tweaks. No-op on the Dummy
    # backend (DLL path is unset) or when FindFMOD did not resolve the
    # runtime binary.
    #
    # Usage (from host CMakeLists):
    #     add_executable(mygame ...)
    #     target_link_libraries(mygame PRIVATE ergo_audio)
    #     ergo_audio_stage_runtime(mygame)
    function(ergo_audio_stage_runtime TARGET)
        if(_ERGO_AUDIO_USE_FMOD AND FMOD_RUNTIME AND EXISTS "${FMOD_RUNTIME}")
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${FMOD_RUNTIME}"
                    "$<TARGET_FILE_DIR:${TARGET}>/"
                COMMENT "Staging FMOD runtime next to ${TARGET}: ${FMOD_RUNTIME}")
        endif()
    endfunction()
endif()

# ---------------------------------------------------------------------------
# ergo_world_time — global time-scale composer for hit-stop / hit-slow
# ---------------------------------------------------------------------------
if(ERGO_BUILD_WORLD_TIME)
    add_library(ergo_world_time src/world_time/world_time.cpp)
    target_include_directories(ergo_world_time PUBLIC include)

    if(ERGO_BUILD_DUMMY)
        add_library(ergo_world_time_dummy src/world_time/world_time_dummy.cpp)
        target_include_directories(ergo_world_time_dummy PUBLIC include)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_world_time tests/world_time/test_world_time.cpp)
        target_link_libraries(test_world_time PRIVATE ergo_world_time ergo_gtest_main)
        add_test(NAME test_world_time COMMAND test_world_time)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_blackboard — global named-property registry with category lifecycle
# ---------------------------------------------------------------------------
if(ERGO_BUILD_BLACKBOARD)
    add_library(ergo_blackboard src/blackboard/blackboard.cpp)
    target_include_directories(ergo_blackboard PUBLIC include)

    if(ERGO_BUILD_DUMMY)
        add_library(ergo_blackboard_dummy src/blackboard/blackboard_dummy.cpp)
        target_include_directories(ergo_blackboard_dummy PUBLIC include)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_blackboard tests/blackboard/test_blackboard.cpp)
        target_link_libraries(test_blackboard PRIVATE ergo_blackboard ergo_gtest_main)
        add_test(NAME test_blackboard COMMAND test_blackboard)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_ui — headless SVG raster + 9-slice frame composer for UI
# ---------------------------------------------------------------------------
if(ERGO_BUILD_UI)
    add_library(ergo_ui STATIC
        src/ui/svg_raster.cpp
        src/ui/nine_slice.cpp
        src/ui/gradient.cpp
        src/ui/line_glow.cpp
    )
    target_include_directories(ergo_ui PUBLIC include)
    # nanoSVG is header-only and used only by svg_raster.cpp. PRIVATE so
    # downstream targets don't accidentally rely on the vendored path.
    target_include_directories(ergo_ui PRIVATE third_party)

    if(ERGO_BUILD_TESTS)
        foreach(NAME test_nine_slice test_svg_raster test_gradient test_line_glow)
            add_executable(${NAME} tests/ui/${NAME}.cpp)
            target_link_libraries(${NAME} PRIVATE ergo_ui ergo_gtest_main)
            add_test(NAME ${NAME} COMMAND ${NAME})
        endforeach()
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_custos — Custos remote-test runner と話す in-process HTTP ブリッジ
# ---------------------------------------------------------------------------
if(ERGO_BUILD_CUSTOS)
    add_library(ergo_custos STATIC
        src/custos/custos_module.cpp
        src/custos/http_server.cpp
        src/custos/png_writer.cpp
    )
    target_include_directories(ergo_custos PUBLIC include)
    target_include_directories(ergo_custos PRIVATE third_party)
    target_link_libraries(ergo_custos PUBLIC Threads::Threads)
    if(WIN32)
        target_link_libraries(ergo_custos PRIVATE ws2_32)
    endif()
    # MSVC は BOM なし UTF-8 を CodePage 932 (Shift-JIS) として読むので、
    # 日本語コメントを含む source は /utf-8 を渡す必要がある。
    if(MSVC)
        target_compile_options(ergo_custos PRIVATE /utf-8)
    endif()

    if(ERGO_BUILD_TESTS)
        foreach(NAME test_http_server test_custos_module)
            add_executable(${NAME} tests/custos/${NAME}.cpp)
            target_link_libraries(${NAME} PRIVATE ergo_custos ergo_gtest_main)
            if(MSVC)
                target_compile_options(${NAME} PRIVATE /utf-8)
            endif()
            add_test(NAME ${NAME} COMMAND ${NAME})
        endforeach()
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_math — C++20 generic math (vec / mat / batch / pool)
# ---------------------------------------------------------------------------
if(ERGO_BUILD_MATH)
    add_library(ergo_math STATIC src/math/math.cpp)
    target_include_directories(ergo_math PUBLIC include)
    # ergo_math は C++20 concepts を使う。ターゲットだけ標準を上書きする。
    set_target_properties(ergo_math PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    # MSVC は BOM なし UTF-8 を CodePage 932 として読むので /utf-8 を渡す。
    if(MSVC)
        target_compile_options(ergo_math PUBLIC /utf-8)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_math tests/math/test_math.cpp)
        target_link_libraries(test_math PRIVATE ergo_math ergo_gtest_main)
        set_target_properties(test_math PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
        if(MSVC)
            target_compile_options(test_math PRIVATE /utf-8)
        endif()
        add_test(NAME test_math COMMAND test_math)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_physics2d — 2D rigid body physics engine (Box2D-like)
#   Shape(Circle/Polygon), Body, World, ContactEvent
#   Sequential impulse solver + SAT narrowphase + N² AABB broadphase
#   Uses ergo_math ObjectPool/Vec2 for DoD body storage
# ---------------------------------------------------------------------------
if(ERGO_BUILD_PHYSICS2D)
    file(GLOB PHYSICS2D_SRC src/physics2d/*.cpp)
    add_library(ergo_physics2d STATIC ${PHYSICS2D_SRC})
    target_include_directories(ergo_physics2d PUBLIC include)
    target_link_libraries(ergo_physics2d PUBLIC ergo_math)
    set_target_properties(ergo_physics2d PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
    if(MSVC)
        target_compile_options(ergo_physics2d PRIVATE /utf-8)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_physics2d tests/physics2d/test_physics2d.cpp)
        target_link_libraries(test_physics2d PRIVATE ergo_physics2d ergo_gtest_main)
        set_target_properties(test_physics2d PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
        )
        if(MSVC)
            target_compile_options(test_physics2d PRIVATE /utf-8)
        endif()
        add_test(NAME test_physics2d COMMAND test_physics2d)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_health — minimal HP component (game-lexicon: health-system)
# ---------------------------------------------------------------------------
if(ERGO_BUILD_HEALTH)
    add_library(ergo_health STATIC src/health/health.cpp)
    target_include_directories(ergo_health PUBLIC include)
    # MSVC は BOM なし UTF-8 を CodePage 932 として読むので /utf-8 を渡す。
    if(MSVC)
        target_compile_options(ergo_health PUBLIC /utf-8)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_health tests/health/test_health.cpp)
        target_link_libraries(test_health PRIVATE ergo_health ergo_gtest_main)
        add_test(NAME test_health COMMAND test_health)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_score — score counter + combo multiplier (game-lexicon: score-system)
# ---------------------------------------------------------------------------
if(ERGO_BUILD_SCORE)
    add_library(ergo_score STATIC src/score/score.cpp)
    target_include_directories(ergo_score PUBLIC include)
    if(MSVC)
        target_compile_options(ergo_score PUBLIC /utf-8)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_score tests/score/test_score.cpp)
        target_link_libraries(test_score PRIVATE ergo_score ergo_gtest_main)
        add_test(NAME test_score COMMAND test_score)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_combo_counter — successive-success counter (game-lexicon: combo-counter)
# ---------------------------------------------------------------------------
if(ERGO_BUILD_COMBO_COUNTER)
    add_library(ergo_combo_counter STATIC src/combo_counter/combo_counter.cpp)
    target_include_directories(ergo_combo_counter PUBLIC include)
    if(MSVC)
        target_compile_options(ergo_combo_counter PUBLIC /utf-8)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_combo_counter tests/combo_counter/test_combo_counter.cpp)
        target_link_libraries(test_combo_counter PRIVATE ergo_combo_counter ergo_gtest_main)
        add_test(NAME test_combo_counter COMMAND test_combo_counter)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_timing_judge — rhythm timing window classifier (game-lexicon: timing-judge)
# ---------------------------------------------------------------------------
if(ERGO_BUILD_TIMING_JUDGE)
    add_library(ergo_timing_judge STATIC src/timing_judge/timing_judge.cpp)
    target_include_directories(ergo_timing_judge PUBLIC include)
    if(MSVC)
        target_compile_options(ergo_timing_judge PUBLIC /utf-8)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_timing_judge tests/timing_judge/test_timing_judge.cpp)
        target_link_libraries(test_timing_judge PRIVATE ergo_timing_judge ergo_gtest_main)
        add_test(NAME test_timing_judge COMMAND test_timing_judge)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_character — TCC-style character controller (Brain/Check/Control/Effect)
# 思想の出典: unity3d-jp/Project_TCC。エンジン非依存 (最下層、依存なし)。
# ---------------------------------------------------------------------------
if(ERGO_BUILD_CHARACTER)
    add_library(ergo_character STATIC
        src/character/brain.cpp
        src/character/transform_brain.cpp
        src/character/move_manager.cpp
        src/character/turn_manager.cpp
        src/character/effect_manager.cpp
        src/character/update_component_manager.cpp
        src/character/move_control.cpp
        src/character/gravity.cpp
        src/character/extra_force.cpp
    )
    target_include_directories(ergo_character PUBLIC include)
    if(MSVC)
        target_compile_options(ergo_character PUBLIC /utf-8)
    endif()

    if(ERGO_BUILD_TESTS)
        foreach(NAME test_character_brain test_character_components)
            add_executable(${NAME} tests/character/${NAME}.cpp)
            target_link_libraries(${NAME} PRIVATE ergo_character ergo_gtest_main)
            if(MSVC)
                target_compile_options(${NAME} PRIVATE /utf-8)
            endif()
            add_test(NAME ${NAME} COMMAND ${NAME})
        endforeach()
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_ui_kit — retained-mode UI component framework (XD-style)
# コンポーネントツリー + auto-layout/constraints + 描画リスト生成。
# 描画は Pictor UIRenderer、 入力/アニメは ergo_ui_kit (P3/P4)。
# ---------------------------------------------------------------------------
if(ERGO_BUILD_UI_KIT)
    add_library(ergo_ui_kit STATIC
        src/ui_kit/ui_kit.cpp
    )
    target_include_directories(ergo_ui_kit PUBLIC include)
    target_link_libraries(ergo_ui_kit PUBLIC ergo_common ergo_io)
    if(MSVC)
        target_compile_options(ergo_ui_kit PRIVATE /utf-8)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_ui_kit tests/ui_kit/test_ui_kit.cpp)
        target_link_libraries(test_ui_kit PRIVATE ergo_ui_kit ergo_gtest_main)
        if(MSVC)
            target_compile_options(test_ui_kit PRIVATE /utf-8)
        endif()
        add_test(NAME test_ui_kit COMMAND test_ui_kit)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_shuriken_migrator — Unity ParticleSystem (Shuriken) → ergo_particle
#   * 依存ゼロの最小 YAML スキャナで .prefab/.unity から !u!198 ブロックを抽出
#   * MinMaxCurve / 主要 Module を ParticleEffectConfig へ写像
#   * CLI 実行ファイル shuriken_migrator も同時 build
# ---------------------------------------------------------------------------
if(ERGO_BUILD_SHURIKEN_MIGRATOR AND ERGO_BUILD_PARTICLE AND ERGO_BUILD_GPU_PARTICLE)
    add_library(ergo_shuriken_migrator STATIC
        src/shuriken_migrator/migrator.cpp
        src/shuriken_migrator/gpu_target.cpp
    )
    target_include_directories(ergo_shuriken_migrator PUBLIC include)
    target_link_libraries(ergo_shuriken_migrator PUBLIC ergo_particle ergo_gpu_particle)
    if(MSVC)
        target_compile_options(ergo_shuriken_migrator PUBLIC /utf-8)
    endif()

    add_executable(shuriken_migrator tools/shuriken_migrator/main.cpp)
    target_link_libraries(shuriken_migrator PRIVATE ergo_shuriken_migrator)
    if(MSVC)
        target_compile_options(shuriken_migrator PRIVATE /utf-8)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_shuriken_migrator tests/shuriken_migrator/test_shuriken_migrator.cpp)
        target_link_libraries(test_shuriken_migrator PRIVATE ergo_shuriken_migrator ergo_gtest_main)
        add_test(NAME test_shuriken_migrator COMMAND test_shuriken_migrator)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_render — Pictor の上・ゲームの下に入る横断オーケストレーション層。
#   * RenderContext  — Pictor ハンドル束
#   * IRenderLayer   — サブレンダラ統一インターフェース
#   * FrameContext   — 毎フレーム可変データ + カメラ math (look_at / perspective)
#   * FrameComposer  — パス構成 + フレームループ (acquire→update→record→submit→present)
#   * ScreenshotBridge — HTTP↔レンダースレッドのスクリーンショット同期
#   * asset_paths    — シェーダ/アセットルートの上方探索ユーティリティ
#   * StageRenderer  — 色キューブ描画の共通レイヤー (KS から移植)
#
# Vulkan 非依存部分 (カメラ math / asset path / ScreenshotBridge /
# FrameComposer のパス列管理) は常にビルドする。 Vulkan 依存の実描画経路は
# `pictor` ターゲットが存在し Vulkan SDK が見つかったときだけ有効化する
# (ergo_particle の renderer と同じパターン)。
# ---------------------------------------------------------------------------
if(ERGO_BUILD_RENDER)
    add_library(ergo_render STATIC
        src/render/frame_context.cpp
        src/render/frame_composer.cpp
        src/render/screenshot_bridge.cpp
        src/render/asset_paths.cpp
        src/render/stage_renderer.cpp
    )
    target_include_directories(ergo_render PUBLIC include)
    target_link_libraries(ergo_render PUBLIC Threads::Threads)
    # MSVC は BOM なし UTF-8 を CodePage 932 として読むので /utf-8 を渡す。
    if(MSVC)
        target_compile_options(ergo_render PUBLIC /utf-8)
    endif()

    # Pictor + Vulkan が揃っていれば実描画経路 (FrameComposer の run_frame、
    # StageRenderer の pipeline 構築) を有効化する。 揃わない場合でも上記
    # 5 ソースは Vulkan 非依存部分としてビルドできる (ヘッダの互換型で縮退)。
    if(TARGET pictor)
        find_package(Vulkan QUIET)
        if(Vulkan_FOUND)
            target_link_libraries(ergo_render PUBLIC pictor Vulkan::Vulkan)
            target_compile_definitions(ergo_render PUBLIC ERGO_RENDER_HAS_VULKAN=1)
            # StageRenderer 用 SPIR-V を bake (glslc がある環境のみ)。
            if(Vulkan_GLSLC_EXECUTABLE)
                set(_RENDER_SHADERS
                    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/stage.vert
                    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/stage.frag
                )
                set(_RENDER_SPIRV)
                foreach(SHADER ${_RENDER_SHADERS})
                    get_filename_component(N ${SHADER} NAME)
                    set(OUT ${CMAKE_BINARY_DIR}/shaders/${N}.spv)
                    add_custom_command(
                        OUTPUT ${OUT}
                        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
                        COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${OUT}
                        DEPENDS ${SHADER}
                        COMMENT "ergo_render: compiling ${N} -> SPIR-V")
                    list(APPEND _RENDER_SPIRV ${OUT})
                endforeach()
                add_custom_target(ergo_render_shaders ALL DEPENDS ${_RENDER_SPIRV})
                add_dependencies(ergo_render ergo_render_shaders)
            endif()
            message(STATUS "ergo_render: real render path enabled (Pictor + Vulkan)")
        else()
            message(STATUS "ergo_render: Vulkan not found — Vulkan-free build only")
        endif()
    else()
        message(STATUS "ergo_render: pictor target absent — Vulkan-free build only")
    endif()

    if(ERGO_BUILD_TESTS)
        foreach(NAME
                test_frame_context test_asset_paths
                test_screenshot_bridge test_frame_composer)
            add_executable(${NAME} tests/render/${NAME}.cpp)
            target_link_libraries(${NAME} PRIVATE ergo_render ergo_gtest_main)
            if(MSVC)
                target_compile_options(${NAME} PRIVATE /utf-8)
            endif()
            add_test(NAME ${NAME} COMMAND ${NAME})
        endforeach()
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_ui_layout — data-driven UI layout runtime with bind/update/emit APIs.
# Pictor-agnostic at module layer via RenderAdapter abstraction.
# ---------------------------------------------------------------------------
if(ERGO_BUILD_UI_LAYOUT)
    add_library(ergo_ui_layout STATIC
        src/ui_layout/document.cpp
        src/ui_layout/json_codec.cpp
        src/ui_layout/layout_solver.cpp
        src/ui_layout/binding.cpp
        src/ui_layout/render_emitter.cpp
        src/ui_layout/hittest.cpp
    )
    target_include_directories(ergo_ui_layout PUBLIC include)
    target_link_libraries(ergo_ui_layout PUBLIC ergo_common ergo_io)
    if(MSVC)
        target_compile_options(ergo_ui_layout PRIVATE /utf-8)
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_ui_layout tests/ui_layout/test_ui_layout.cpp)
        target_link_libraries(test_ui_layout PRIVATE ergo_ui_layout ergo_gtest_main)
        if(MSVC)
            target_compile_options(test_ui_layout PRIVATE /utf-8)
        endif()
        # Registered like every other module test. NOTE: with a multi-config
        # generator (VS) the test is config-scoped, so it is only discoverable
        # via `ctest -C <Release|Debug|...>`. A bare `ctest -R ui_layout`
        # reports "Not Run"/"No tests found" for the empty config — that is the
        # documented invocation (README: `ctest --test-dir build -C Release`),
        # not a registration bug.
        add_test(NAME test_ui_layout COMMAND test_ui_layout)
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_vector - SVG path parser + tessellation + extruded mesh + scene animation
# ---------------------------------------------------------------------------
if(ERGO_BUILD_VECTOR)
    add_library(ergo_vector STATIC
        src/vector/path.cpp
        src/vector/svg_parser.cpp
        src/vector/flatten.cpp
        src/vector/tessellator.cpp
        src/vector/extruder.cpp
        src/vector/vector_mesh.cpp
        src/vector/vector_scene.cpp
        src/vector/morph.cpp
        src/vector/bind.cpp
    )
    target_include_directories(ergo_vector PUBLIC include)
    target_include_directories(ergo_vector PRIVATE third_party)
    if(MSVC)
        target_compile_options(ergo_vector PUBLIC /utf-8)
    endif()

    if(ERGO_BUILD_TESTS)
        foreach(NAME test_svg_parser test_mesh_build test_extrude test_morph test_scene_collect)
            add_executable(${NAME} tests/vector/${NAME}.cpp)
            target_link_libraries(${NAME} PRIVATE ergo_vector ergo_gtest_main)
            add_test(NAME ${NAME} COMMAND ${NAME})
        endforeach()
    endif()
endif()

# ---------------------------------------------------------------------------
# benchmarks/ — micro-benchmarks, opt-in via ERGO_BUILD_BENCHMARKS.
#
# Each benchmark is a standalone executable that uses the tiny harness in
# `benchmarks/bench_common.h` (no external benchmarking framework — we
# value zero-dep builds over richer reporting). Run manually; no CTest hook
# on purpose — benchmarks measure runtime variance, not correctness.
# ---------------------------------------------------------------------------
if(ERGO_BUILD_BENCHMARKS)
    if(ERGO_BUILD_GPU_PARTICLE)
        add_executable(bench_particle_cpu benchmarks/bench_particle_cpu.cpp)
        target_include_directories(bench_particle_cpu PRIVATE benchmarks)
        target_link_libraries(bench_particle_cpu PRIVATE ergo_gpu_particle)

        add_executable(bench_curve benchmarks/bench_curve.cpp)
        target_include_directories(bench_curve PRIVATE benchmarks)
        target_link_libraries(bench_curve PRIVATE ergo_gpu_particle)
    endif()
endif()

