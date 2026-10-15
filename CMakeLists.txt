cmake_minimum_required(VERSION 3.20)
project(Ergo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Top-level options. Hosts can flip per-module knobs as needed.
# ---------------------------------------------------------------------------
option(ERGO_BUILD_TESTS       "Build module tests"             ON)
option(ERGO_BUILD_DUMMY       "Build dummy plug libraries"     OFF)
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
# 実描画経路 (Pictor + Vulkan) を確保できない構成を構成エラーにする。
# Android / iOS は ErgoRenderBackend.cmake が常に必須扱いにするため、
# このオプションはデスクトップ構成を厳格化するためのもの。
option(ERGO_RENDER_REQUIRE_REAL "ergo_render: 実描画経路を確保できない構成を構成エラーにする" OFF)
option(ERGO_BUILD_VECTOR       "Build ergo_vector (SVG path mesh + extrude + animation)" ON)
option(ERGO_BUILD_TELA_GRAPH   "Build ergo_tela_graph (Ergo 可視化の共通出口: Tela 関係図)" ON)
option(ERGO_BUILD_RENDER_VIZ   "Build ergo_render_viz (Pictor 描画パイプラインの Tela 可視化)" ON)
option(ERGO_BUILD_GRAPH_LAYOUT  "Build ergo_graph_layout (抽象グラフの層別配置 → Tela 関係図)" ON)
option(ERGO_BUILD_PACKAGE_MODEL "Build ergo_package_model (resolved.json / CMake File API リーダ)" ON)
option(ERGO_BUILD_PACKAGE_SCAN  "Build ergo_package_scan (観測対象のチェックアウト → 根拠付きの使用関係)" ON)
option(ERGO_BUILD_PACKAGE_VIZ   "Build ergo_package_viz (パッケージ版一覧 / ターゲットのリンク図 / 使用関係図 + CLI)" ON)
option(ERGO_BUILD_PACKAGE_MANAGER "Build ergo_package_manager (ホスト宣言の読み取り・検査・漏れ検査)" ON)
option(ERGO_BUILD_PACKAGE_CLI   "Build the ergo_pkg CLI (init / list / doctor / fetch、 ergo_package_manager が要る)" ON)
option(ERGO_BUILD_PACKAGE_WATCH "Build ergo_package_watch (更新の検知 + ergo_pkg outdated / watch)" ON)
option(ERGO_BUILD_CAST         "Build ergo_cast (Actor bundling Scene; render-independent)" ON)
option(ERGO_BUILD_SCENE        "Build ergo_scene (serializable look-dev Scene document)" ON)
option(ERGO_BUILD_CHARACTER    "Build ergo_character (TCC-style character controller)" ON)
option(ERGO_BUILD_MATH         "Build ergo_math (C++20 generic math: vec/mat/batch/pool)" ON)
option(ERGO_BUILD_PHYSICS2D    "Build ergo_physics2d (2D rigid body, Box2D-like)" ON)
option(ERGO_BUILD_RESOURCE     "Build ergo_resource (tag-driven resource selection & loading)" ON)
# ergo_http needs the managed libcurl, which is a fetched/opt-in dependency, so
# this module is OFF by default (keeps the plain build offline + light).
option(ERGO_BUILD_HTTP         "Build ergo_http (libcurl-backed HTTP client; forces ERGO_WITH_CURL)" OFF)
if(ERGO_BUILD_HTTP)
    set(ERGO_WITH_CURL ON)  # ergo_http consumes CURL::libcurl (see third_party/dependencies.cmake)
endif()
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
# Managed third-party libraries (fetched + compiled from pinned upstreams:
# curl / ...). The registry declares every managed dependency and
# opt-in builds the ones whose ERGO_WITH_<NAME> switch is ON. Engine lives in
# cmake/ErgoDependencies.cmake; per-lib wiring in cmake/deps/<name>.cmake.
# Header-only vendored libs (earcut/nanosvg/gtest) stay under third_party/ and
# are not managed here. See spec/setup/third-party-deps.md.
# ---------------------------------------------------------------------------
include("${CMAKE_CURRENT_LIST_DIR}/third_party/dependencies.cmake")

# LUDIARS package hub (Pictor / Tela ...). Must run before the module sections
# (they branch on if(TARGET pictor)). ERGO_PACKAGES unset = compatibility mode
# M0: nothing is acquired, existing targets are only recorded into
# <build>/ergo-packages.resolved.json. See spec/setup/packages.md.
include("${CMAKE_CURRENT_LIST_DIR}/cmake/packages/ErgoPackages.cmake")

# ---------------------------------------------------------------------------
# ergo_http — libcurl-backed synchronous HTTP client. The public header hides
# curl entirely (consumers link ergo_http, never <curl/curl.h>); curl is a
# PRIVATE link. Opt-in via -DERGO_BUILD_HTTP=ON (forces ERGO_WITH_CURL above).
# ---------------------------------------------------------------------------
if(ERGO_BUILD_HTTP)
    if(NOT TARGET CURL::libcurl)
        message(FATAL_ERROR
            "ERGO_BUILD_HTTP=ON but CURL::libcurl is unavailable. Ensure ERGO_WITH_CURL=ON "
            "and ERGO_FETCH_DEPENDENCIES=ON (or provide ERGO_SOURCE_DIR_CURL). No silent fallback.")
    endif()
    add_library(ergo_http
        src/http/curl_client.cpp
        src/http/curl_global.cpp)
    target_include_directories(ergo_http PUBLIC include)
    target_link_libraries(ergo_http PRIVATE CURL::libcurl)

    if(ERGO_BUILD_TESTS)
        add_executable(test_http_client tests/http/test_http_client.cpp)
        target_link_libraries(test_http_client PRIVATE ergo_http ergo_gtest_main)
        add_test(NAME test_http_client COMMAND test_http_client)
    endif()
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
# ergo_resource — tag-driven resource selection & loading (Clio の C++ 移植)
# ---------------------------------------------------------------------------
if(ERGO_BUILD_RESOURCE)
    # catalog.cpp / loader.cpp read manifests and resource bytes through
    # ergo_io (see their #include "ergo/io/file.h"); there is no IO-less
    # code path. Fail fast at configure time instead of a cryptic LNK1181
    # from linking a target that ERGO_BUILD_IO=OFF never created.
    if(NOT ERGO_BUILD_IO)
        message(FATAL_ERROR
            "ERGO_BUILD_RESOURCE=ON but ERGO_BUILD_IO=OFF. ergo_resource reads "
            "manifests/resources via ergo_io and has no non-IO fallback. Set "
            "ERGO_BUILD_IO=ON (or ERGO_BUILD_RESOURCE=OFF). No silent fallback.")
    endif()
    add_library(ergo_resource STATIC
        src/resource/catalog.cpp
        src/resource/loader.cpp
        src/resource/resolver.cpp
        src/resource/tag_scorer.cpp)
    target_include_directories(ergo_resource PUBLIC include)
    target_link_libraries(ergo_resource PUBLIC ergo_common ergo_io)
    if(MSVC)
        target_compile_options(ergo_resource PUBLIC /utf-8)
    endif()

    if(ERGO_BUILD_TESTS)
        foreach(NAME tag_scorer catalog resolver loader)
            add_executable(test_resource_${NAME} tests/resource/test_${NAME}.cpp)
            target_link_libraries(test_resource_${NAME} PRIVATE ergo_resource ergo_gtest_main)
            add_test(NAME test_resource_${NAME} COMMAND test_resource_${NAME})
        endforeach()
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
#   * StageRenderer  — 色キューブ描画の共通レイヤー (PrivateGame から移植)
#   * RenderBackend  — 実描画バックエンドのビルド時契約 (desktop/Android/iOS)
#   * RenderRequirements — RenderContext が実描画の前提を満たすかの型付き検査
#
# Vulkan 非依存部分 (カメラ math / asset path / ScreenshotBridge /
# FrameComposer のパス列管理) は常にビルドする。 Vulkan 依存の実描画経路は
# `pictor` ターゲットが公開する runtime Vulkan contract (PICTOR_HAS_VULKAN) を
# 正本に、 desktop / Android / iOS それぞれの出所で有効化する
# (判定は cmake/ErgoRenderBackend.cmake)。 実描画を確保できない構成は
# STATUS で黙って通さず、 警告または構成エラーにする。
# ---------------------------------------------------------------------------
if(ERGO_BUILD_RENDER)
    add_library(ergo_render STATIC
        src/render/frame_context.cpp
        src/render/frame_composer.cpp
        src/render/screenshot_bridge.cpp
        src/render/asset_paths.cpp
        src/render/stage_renderer.cpp
        src/render/render_backend.cpp
        src/render/render_requirements.cpp
    )
    target_include_directories(ergo_render PUBLIC include)
    target_link_libraries(ergo_render PUBLIC Threads::Threads)
    # MSVC は BOM なし UTF-8 を CodePage 932 として読むので /utf-8 を渡す。
    if(MSVC)
        target_compile_options(ergo_render PUBLIC /utf-8)
    endif()

    # 実描画経路 (FrameComposer の run_frame、 StageRenderer の pipeline 構築)
    # の可否は ErgoRenderBackend.cmake がプラットフォーム中立に判定する。
    # デスクトップは Vulkan SDK、 Android は NDK 同梱 Vulkan、 iOS は MoltenVK
    # で、 いずれも pictor が公開する PICTOR_HAS_VULKAN が正本。
    # `Vulkan::Vulkan` の不在だけでモバイルの描画を無効化しない (KD-MOB-001)。
    include(ErgoRenderBackend)
    ergo_render_resolve_backend(ERGO_RENDER_BACKEND)
    ergo_render_enforce_backend(ERGO_RENDER_BACKEND)

    if(ERGO_RENDER_BACKEND_ENABLED)
        target_link_libraries(ergo_render PUBLIC pictor ${ERGO_RENDER_BACKEND_LIBRARIES})
        target_compile_definitions(ergo_render PUBLIC ${ERGO_RENDER_BACKEND_DEFINITIONS})
        # StageRenderer 用 SPIR-V を bake (glslc がある環境のみ)。
        # モバイルは bake 済み SPIR-V をホストのパッケージが配る前提なので、
        # glslc 不在は実描画可否の判定に影響しない。
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
    endif()

    if(ERGO_BUILD_TESTS)
        foreach(NAME
                test_frame_context test_asset_paths
                test_screenshot_bridge test_frame_composer
                test_render_backend)
            add_executable(${NAME} tests/render/${NAME}.cpp)
            target_link_libraries(${NAME} PRIVATE ergo_render ergo_gtest_main)
            if(MSVC)
                target_compile_options(${NAME} PRIVATE /utf-8)
            endif()
            add_test(NAME ${NAME} COMMAND ${NAME})
        endforeach()

        # 出所名定義の作り方は実描画の有無に依存させずに検査する
        # (実描画が無効な構成ではこの定義の経路がビルドされないため)。
        add_executable(test_render_vulkan_source_define
            tests/render/test_render_vulkan_source_define.cpp)
        target_link_libraries(test_render_vulkan_source_define PRIVATE ergo_gtest_main)
        ergo_render_vulkan_source_definition(_test_source_definition "Vulkan SDK")
        target_compile_definitions(test_render_vulkan_source_define
            PRIVATE "${_test_source_definition}")
        if(MSVC)
            target_compile_options(test_render_vulkan_source_define PRIVATE /utf-8)
        endif()
        add_test(NAME test_render_vulkan_source_define
            COMMAND test_render_vulkan_source_define)
    endif()
endif()


# ---------------------------------------------------------------------------
# ergo_tela_graph — Ergo の可視化の共通出口。 配置済みの関係図を Tela へ渡す。
#   * ergo_tela_graph      — 関係図の値型 + TELA_GRAPH 1 の検査・書き出し。
#                            Tela に依存しない (常時ビルド・テスト可)。
#   * ergo_tela_graph_tela — 関係図 → tela::Graph (プロセス内描画)。
#                            `Tela::Graph` ターゲットがあるときだけ (C++20)。
# 表示は Tela の tela_view / tela_overlay (--graph <file>)。
# ---------------------------------------------------------------------------
if(ERGO_BUILD_TELA_GRAPH)
    add_library(ergo_tela_graph STATIC src/tela_graph/writer.cpp)
    target_include_directories(ergo_tela_graph PUBLIC include)
    if(MSVC)
        target_compile_options(ergo_tela_graph PUBLIC /utf-8)
    endif()

    if(NOT TARGET Tela::Graph)
        find_package(Tela CONFIG QUIET)
    endif()
    if(TARGET Tela::Graph)
        add_library(ergo_tela_graph_tela STATIC src/tela_graph/tela_adapter.cpp)
        target_link_libraries(ergo_tela_graph_tela PUBLIC ergo_tela_graph Tela::Graph)
        target_compile_features(ergo_tela_graph_tela PUBLIC cxx_std_20)
        target_compile_definitions(ergo_tela_graph_tela PUBLIC ERGO_TELA_GRAPH_HAS_TELA=1)
        message(STATUS "ergo_tela_graph: Tela adapter enabled")
    else()
        message(STATUS "ergo_tela_graph: Tela::Graph absent — in-process Tela adapter skipped")
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_tela_graph_writer tests/tela_graph/test_tela_graph_writer.cpp)
        target_link_libraries(test_tela_graph_writer PRIVATE ergo_tela_graph ergo_gtest_main)
        if(MSVC)
            target_compile_options(test_tela_graph_writer PRIVATE /utf-8)
        endif()
        add_test(NAME test_tela_graph_writer COMMAND test_tela_graph_writer)
        if(TARGET ergo_tela_graph_tela)
            add_executable(test_tela_graph_adapter tests/tela_graph/test_tela_graph_adapter.cpp)
            target_link_libraries(test_tela_graph_adapter PRIVATE ergo_tela_graph_tela ergo_gtest_main)
            if(MSVC)
                target_compile_options(test_tela_graph_adapter PRIVATE /utf-8)
            endif()
            add_test(NAME test_tela_graph_adapter COMMAND test_tela_graph_adapter)
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_render_viz — Pictor の描画パイプラインを関係図にする (出口は ergo_tela_graph)。
#   * ergo_render_viz        — スナップショット + 配置。 Pictor に依存しない。
#   * ergo_render_viz_pictor — PipelineProfileDef / FrameStats → スナップショット。
#                              `pictor` ターゲットがあるときだけ。
#   * ergo_render_viz (CLI)  — プロファイル JSON / プリセット → .tela ファイル。
#                              pictor があるときだけ。
# ergo_render とは独立 (モジュール間の cross-import をしない)。
# ---------------------------------------------------------------------------
if(ERGO_BUILD_RENDER_VIZ AND TARGET ergo_tela_graph)
    add_library(ergo_render_viz STATIC
        src/render_viz/pipeline_labels.cpp
        src/render_viz/pipeline_layout.cpp
    )
    target_include_directories(ergo_render_viz PUBLIC include)
    target_link_libraries(ergo_render_viz PUBLIC ergo_tela_graph)
    if(MSVC)
        target_compile_options(ergo_render_viz PUBLIC /utf-8)
    endif()

    if(TARGET pictor)
        add_library(ergo_render_viz_pictor STATIC src/render_viz/pictor_snapshot.cpp)
        target_link_libraries(ergo_render_viz_pictor PUBLIC ergo_render_viz pictor)
        target_compile_features(ergo_render_viz_pictor PUBLIC cxx_std_20)
        target_compile_definitions(ergo_render_viz_pictor PUBLIC ERGO_RENDER_VIZ_HAS_PICTOR=1)

        add_executable(ergo_render_viz_cli tools/render_viz/main.cpp)
        set_target_properties(ergo_render_viz_cli PROPERTIES OUTPUT_NAME ergo_render_viz)
        target_link_libraries(ergo_render_viz_cli PRIVATE ergo_render_viz_pictor)
        message(STATUS "ergo_render_viz: Pictor adapter + CLI enabled")
    else()
        message(STATUS "ergo_render_viz: pictor target absent — Pictor adapter / CLI skipped")
    endif()

    if(ERGO_BUILD_TESTS)
        add_executable(test_render_viz_layout tests/render_viz/test_render_viz_layout.cpp)
        target_link_libraries(test_render_viz_layout PRIVATE ergo_render_viz ergo_gtest_main)
        if(MSVC)
            target_compile_options(test_render_viz_layout PRIVATE /utf-8)
        endif()
        add_test(NAME test_render_viz_layout COMMAND test_render_viz_layout)
    endif()
elseif(ERGO_BUILD_RENDER_VIZ)
    message(STATUS "ergo_render_viz: requires ERGO_BUILD_TELA_GRAPH — skipped")
endif()

# ---------------------------------------------------------------------------
# Package hub visualisation (spec/feature/package-hub.md §5, stage S3).
#   * ergo_graph_layout  — 抽象グラフ → 配置済み tela_graph::Graph (層別配置)。
#   * ergo_package_model — resolved.json / File API codemodel v2 → PackageModel。
#   * ergo_package_scan  — observe の兄弟チェックアウト → 根拠付きの使用関係 (UsageModel)。
#   * ergo_package_viz   — PackageModel → P1 packages / P2 targets、 UsageModel → P3 usage
#                          (簡約・畳み・分割)。
#   * ergo_package_viz (CLI、 ターゲット ergo_package_viz_cli) と
#     ergo_package_graphs (build/graphs/ に図を書くカスタムターゲット)。
# 依存は package_viz → package_model / package_scan / graph_layout → tela_graph の一方向
# (package_scan と package_model は互いに import しない)。
# Pictor / Tela に依存せず常時ビルド・テスト可。 ergo_render_viz とは互いに import しない。
# ---------------------------------------------------------------------------
if(ERGO_BUILD_GRAPH_LAYOUT AND TARGET ergo_tela_graph)
    add_library(ergo_graph_layout STATIC
        src/graph_layout/layering.cpp
        src/graph_layout/ordering.cpp
        src/graph_layout/routing.cpp
        src/graph_layout/layered_layout.cpp
    )
    target_include_directories(ergo_graph_layout PUBLIC include)
    target_link_libraries(ergo_graph_layout PUBLIC ergo_tela_graph)
    if(MSVC)
        target_compile_options(ergo_graph_layout PUBLIC /utf-8)
    endif()
    if(ERGO_BUILD_TESTS)
        add_executable(test_graph_layout tests/graph_layout/test_graph_layout.cpp)
        target_link_libraries(test_graph_layout PRIVATE ergo_graph_layout ergo_gtest_main)
        add_test(NAME test_graph_layout COMMAND test_graph_layout)
    endif()
elseif(ERGO_BUILD_GRAPH_LAYOUT)
    message(STATUS "ergo_graph_layout: requires ERGO_BUILD_TELA_GRAPH — skipped")
endif()

if(ERGO_BUILD_PACKAGE_MODEL)
    add_library(ergo_package_model STATIC
        src/package_model/json_access.cpp
        src/package_model/package_model.cpp
        src/package_model/resolved_reader.cpp
        src/package_model/file_api_reader.cpp
        src/package_model/layers.cpp
        src/package_model/attribution.cpp
        src/package_model/declaration_check.cpp
        src/package_model/version_label.cpp
        src/package_model/loader.cpp
        src/package_model/watch_report_reader.cpp
        src/package_model/host_status_reader.cpp
        src/package_model/host_state_loader.cpp
    )
    target_include_directories(ergo_package_model PUBLIC include)
    target_link_libraries(ergo_package_model PUBLIC ergo_common)
    if(MSVC)
        target_compile_options(ergo_package_model PUBLIC /utf-8)
    endif()
    if(ERGO_BUILD_TESTS)
        add_executable(test_package_model tests/package_model/test_package_model.cpp
            tests/package_model/test_host_state.cpp)
        target_link_libraries(test_package_model PRIVATE ergo_package_model ergo_gtest_main)
        target_compile_definitions(test_package_model PRIVATE
            ERGO_PACKAGE_MODEL_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/package_model/fixtures")
        add_test(NAME test_package_model COMMAND test_package_model)
    endif()
endif()

if(ERGO_BUILD_PACKAGE_SCAN)
    add_library(ergo_package_scan STATIC
        src/package_scan/usage_model.cpp
        src/package_scan/json_lines.cpp
        src/package_scan/gitmodules_reader.cpp
        src/package_scan/git_index_reader.cpp
        src/package_scan/package_json_reader.cpp
        src/package_scan/manifest_reader.cpp
        src/package_scan/scope.cpp
        src/package_scan/source.cpp
        src/package_scan/resolver.cpp
        src/package_scan/scan.cpp
    )
    target_include_directories(ergo_package_scan PUBLIC include)
    target_link_libraries(ergo_package_scan PUBLIC ergo_common)
    if(MSVC)
        target_compile_options(ergo_package_scan PUBLIC /utf-8)
    endif()
    if(ERGO_BUILD_TESTS)
        add_executable(test_package_scan tests/package_scan/test_package_scan.cpp)
        target_link_libraries(test_package_scan PRIVATE ergo_package_scan ergo_gtest_main)
        target_compile_definitions(test_package_scan PRIVATE
            ERGO_PACKAGE_SCAN_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/package_scan/fixtures")
        add_test(NAME test_package_scan COMMAND test_package_scan)
    endif()
endif()

if(ERGO_BUILD_PACKAGE_VIZ AND TARGET ergo_package_model AND TARGET ergo_package_scan
   AND TARGET ergo_graph_layout)
    add_library(ergo_package_viz STATIC
        src/package_viz/groups.cpp
        src/package_viz/labels.cpp
        src/package_viz/package_view.cpp
        src/package_viz/target_view.cpp
        src/package_viz/reduction.cpp
        src/package_viz/collapse_focus.cpp
        src/package_viz/split.cpp
        src/package_viz/fit.cpp
        src/package_viz/usage_view.cpp
        src/package_viz/host_labels.cpp
        src/package_viz/host_view.cpp
    )
    target_include_directories(ergo_package_viz PUBLIC include)
    target_link_libraries(ergo_package_viz PUBLIC ergo_package_model ergo_package_scan ergo_graph_layout)

    add_executable(ergo_package_viz_cli
        tools/package_viz/main.cpp
        tools/package_viz/cli_options.cpp
        tools/package_viz/version_probe.cpp
        tools/package_viz/usage_command.cpp
        tools/package_viz/host_command.cpp
    )
    set_target_properties(ergo_package_viz_cli PROPERTIES OUTPUT_NAME ergo_package_viz)
    target_link_libraries(ergo_package_viz_cli PRIVATE ergo_package_viz)

    # cmake --build <build> --target ergo_package_graphs → <build>/graphs/{packages,targets,usage,host}.tela
    # (P1 / P2 はトップの構成の resolved.json と File API の返答、 P3 はマニフェストの observe が指す
    # ERGO_PACKAGES_SIBLING_ROOT の兄弟チェックアウト。 未設定なら観測対象は未観測として描く。
    # P4 はトップのソース (ホスト) の .ergo/watch-report.json と <build>/ergo-packages.status.json
    # (ergo_pkg watch / ergo_pkg status が書く。 無ければ札は "—")。
    set(_ergo_graphs_dir "${CMAKE_BINARY_DIR}/graphs")
    set(_ergo_usage_root "")
    if(ERGO_PACKAGES_SIBLING_ROOT)
        set(_ergo_usage_root --root "${ERGO_PACKAGES_SIBLING_ROOT}")
    endif()
    add_custom_target(ergo_package_graphs
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_ergo_graphs_dir}"
        COMMAND ergo_package_viz_cli "${CMAKE_BINARY_DIR}" --view packages
                --out "${_ergo_graphs_dir}/packages.tela"
        COMMAND ergo_package_viz_cli "${CMAKE_BINARY_DIR}" --view targets
                --out "${_ergo_graphs_dir}/targets.tela"
        COMMAND ergo_package_viz_cli "${CMAKE_BINARY_DIR}" --view usage ${_ergo_usage_root}
                --manifest "${CMAKE_CURRENT_SOURCE_DIR}/ergo-packages.json"
                --out "${_ergo_graphs_dir}/usage.tela"
        COMMAND ergo_package_viz_cli "${CMAKE_BINARY_DIR}" --view host --host "${CMAKE_SOURCE_DIR}"
                --out "${_ergo_graphs_dir}/host.tela"
        DEPENDS ergo_package_viz_cli
        COMMENT "ergo_package_viz: writing package graphs to ${_ergo_graphs_dir}"
        VERBATIM)
    unset(_ergo_graphs_dir)
    unset(_ergo_usage_root)

    if(ERGO_BUILD_TESTS)
        add_executable(test_package_viz tests/package_viz/test_package_viz.cpp
            tests/package_viz/test_usage_view.cpp
            tests/package_viz/test_host_view.cpp)
        target_link_libraries(test_package_viz PRIVATE ergo_package_viz ergo_gtest_main)
        target_compile_definitions(test_package_viz PRIVATE
            ERGO_PACKAGE_MODEL_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/package_model/fixtures"
            ERGO_PACKAGE_SCAN_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/package_scan/fixtures")
        add_test(NAME test_package_viz COMMAND test_package_viz)
        if(TARGET ergo_tela_graph_tela)
            # Tela がある構成だけ: 書いた図を Tela 本体の load_graph が受理すること。
            add_executable(test_package_viz_tela tests/package_viz/test_package_viz_tela.cpp)
            target_link_libraries(test_package_viz_tela PRIVATE
                ergo_package_viz ergo_tela_graph_tela ergo_gtest_main)
            target_compile_definitions(test_package_viz_tela PRIVATE
                ERGO_PACKAGE_MODEL_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/package_model/fixtures")
            add_test(NAME test_package_viz_tela COMMAND test_package_viz_tela)
        endif()
    endif()
elseif(ERGO_BUILD_PACKAGE_VIZ)
    message(STATUS "ergo_package_viz: requires ergo_package_model, ergo_package_scan and ergo_graph_layout — skipped")
endif()

# ---------------------------------------------------------------------------
# Package manager (spec/feature/package-manager.md §2, stages W1 / W2).
#   * ergo_package_manager — ホストの宣言 (ergo-packages.json の host 節) の読み取りと検査、
#     漏れ検査 (§8)、 ブートストラップの生成と古さの判定、 init の計画、 一覧。
#     W2: 書式を保つ書き換え (add / remove / pin)、 持ち主の判定、 版の解決 (IRevResolver)。
#   * ergo_pkg (CLI、 ターゲット ergo_pkg_cli) — init / list / doctor / fetch、 W2 の add / remove / update (W3 の outdated / watch は下の package watch)。
# 依存は tools/package_cli → package_manager → ergo_common の一方向。 Pictor / Tela に依存せず
# 常時ビルド・テスト可。 git / cmake の起動は CLI 側だけ (ライブラリはプロセスを起こさない)。
# ---------------------------------------------------------------------------
if(ERGO_BUILD_PACKAGE_MANAGER)
    add_library(ergo_package_manager STATIC
        src/package_manager/finding.cpp
        src/package_manager/json_fields.cpp
        src/package_manager/manifest_packages.cpp
        src/package_manager/ergo_manifest.cpp
        src/package_manager/host_declaration.cpp
        src/package_manager/declaration_check.cpp
        src/package_manager/leak_check.cpp
        src/package_manager/bootstrap.cpp
        src/package_manager/init_plan.cpp
        src/package_manager/upm_declaration.cpp
        src/package_manager/package_list.cpp
        src/package_manager/json_scan.cpp
        src/package_manager/json_edit.cpp
        src/package_manager/package_owner.cpp
        src/package_manager/rev_resolution.cpp
        src/package_manager/declaration_edit.cpp
        src/package_manager/declaration_add.cpp
        src/package_manager/declaration_remove.cpp
        src/package_manager/test_results.cpp
        src/package_manager/host_status.cpp
    )
    target_include_directories(ergo_package_manager PUBLIC include)
    target_link_libraries(ergo_package_manager PUBLIC ergo_common)
    if(MSVC)
        target_compile_options(ergo_package_manager PUBLIC /utf-8)
    endif()
    if(ERGO_BUILD_TESTS)
        add_executable(test_package_manager
            tests/package_manager/test_declaration.cpp
            tests/package_manager/test_leak_check.cpp
            tests/package_manager/test_bootstrap_init.cpp
            tests/package_manager/test_package_list.cpp
            tests/package_manager/test_json_edit.cpp
            tests/package_manager/test_declaration_edit.cpp
            tests/package_manager/test_rev_resolution.cpp
            tests/package_manager/test_host_status.cpp)
        target_link_libraries(test_package_manager PRIVATE ergo_package_manager ergo_gtest_main)
        target_compile_definitions(test_package_manager PRIVATE
            ERGO_PACKAGE_MANAGER_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/package_manager/fixtures"
            ERGO_PACKAGE_MANAGER_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME test_package_manager COMMAND test_package_manager)
    endif()
endif()

# ---------------------------------------------------------------------------
# Package watch (spec/feature/package-manager.md §2.3 / §3, stage W3).
#   * ergo_package_watch — 依存の列挙、 IRefSource / IHistorySource (取得元の interface)、
#     観測・判定 (tag / track / off-main)、 前回状態と重複の抑止、 報告と出来事の書き手 (漏れ検査つき)。
#   * W4: IReviewSource / IRunSource (Revisor の local PR と Concordia の委託 run)、 run / review / merged と
#     由来 (pf / ai / human / unknown)、 loopback の URL の検査。
#   * W7: UPM ホスト (Unity プロジェクトの Packages/manifest.json の git 依存) の列挙。
# 向きは ergo_package_watch → ergo_package_manager → ergo_common。 git プロセスと HTTP は CLI のアダプタ
# (tools/package_cli/git_*_source.cpp、 revisor_review_source.cpp / concordia_run_source.cpp) にだけあり、
# ライブラリはプロセスを起こさずネットワークにも出ない。
# ---------------------------------------------------------------------------
if(ERGO_BUILD_PACKAGE_WATCH AND TARGET ergo_package_manager)
    add_library(ergo_package_watch STATIC
        src/package_watch/watch_dependency.cpp
        src/package_watch/ref_listing.cpp
        src/package_watch/semver.cpp
        src/package_watch/observation.cpp
        src/package_watch/detect.cpp
        src/package_watch/time_text.cpp
        src/package_watch/watch_state.cpp
        src/package_watch/watch_diff.cpp
        src/package_watch/watch_report.cpp
        src/package_watch/loopback_url.cpp
        src/package_watch/provenance_json.cpp
        src/package_watch/provenance.cpp
        src/package_watch/upm_dependency.cpp
    )
    target_include_directories(ergo_package_watch PUBLIC include)
    target_link_libraries(ergo_package_watch PUBLIC ergo_package_manager)
    if(ERGO_BUILD_TESTS)
        add_executable(test_package_watch
            tests/package_watch/test_detect.cpp
            tests/package_watch/test_diff.cpp
            tests/package_watch/test_report.cpp
            tests/package_watch/test_provenance.cpp
            tests/package_watch/test_upm.cpp)
        target_link_libraries(test_package_watch PRIVATE ergo_package_watch ergo_gtest_main)
        target_compile_definitions(test_package_watch PRIVATE
            ERGO_PACKAGE_WATCH_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/package_watch/fixtures")
        add_test(NAME test_package_watch COMMAND test_package_watch)
    endif()
elseif(ERGO_BUILD_PACKAGE_WATCH)
    message(STATUS "ergo_package_watch: requires ERGO_BUILD_PACKAGE_MANAGER — skipped")
endif()

if(ERGO_BUILD_TESTS)
    # W7: Unity パッケージ (unity/com.ludiars.ergo.packages) の構造と Fixtures のバイト一致。 Unity も dotnet も要らない。
    add_test(NAME test_unity_package_structure
        COMMAND ${CMAKE_COMMAND}
            -DERGO_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/packages/test_unity_package.cmake)
endif()

if(ERGO_BUILD_TESTS AND ERGO_BUILD_PACKAGE_MANAGER)
    # track キー (W3) は構成時の読み手の出力を変えない。
    add_test(NAME test_packages_track_key
        COMMAND ${CMAKE_COMMAND}
            -DERGO_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DERGO_TEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/packages/track_key
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/packages/test_track_key.cmake)
endif()

if(ERGO_BUILD_PACKAGE_CLI AND TARGET ergo_package_manager)
    add_executable(ergo_pkg_cli
        tools/package_cli/main.cpp
        tools/package_cli/cli_args.cpp
        tools/package_cli/file_io.cpp
        tools/package_cli/process.cpp
        tools/package_cli/host_context.cpp
        tools/package_cli/upm_host.cpp
        tools/package_cli/cmd_init.cpp
        tools/package_cli/cmd_list.cpp
        tools/package_cli/cmd_doctor.cpp
        tools/package_cli/cmd_fetch.cpp
        tools/package_cli/git_rev_resolver.cpp
        tools/package_cli/edit_support.cpp
        tools/package_cli/cmd_add.cpp
        tools/package_cli/cmd_remove.cpp
        tools/package_cli/cmd_update.cpp
        tools/package_cli/status_inputs.cpp
        tools/package_cli/cmd_status.cpp
    )
    set_target_properties(ergo_pkg_cli PROPERTIES OUTPUT_NAME ergo_pkg)
    target_link_libraries(ergo_pkg_cli PRIVATE ergo_package_manager)
    # 既定の Ergo / cmake はこのビルドのもの (--ergo / --cmake で上書き)。 バイナリにだけ入り、 リポには書かない。
    target_compile_definitions(ergo_pkg_cli PRIVATE
        ERGO_PKG_CLI_DEFAULT_ERGO_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        ERGO_PKG_CLI_DEFAULT_CMAKE="${CMAKE_COMMAND}")
    if(ERGO_BUILD_TESTS)
        find_package(Git QUIET)
        if(GIT_EXECUTABLE)
            add_test(NAME test_package_cli
                COMMAND ${CMAKE_COMMAND}
                    -DERGO_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                    -DERGO_TEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/packages/package_cli
                    -DERGO_PKG_CLI=$<TARGET_FILE:ergo_pkg_cli>
                    -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/packages/test_package_cli.cmake)
            add_test(NAME test_package_cli_edit
                COMMAND ${CMAKE_COMMAND}
                    -DERGO_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                    -DERGO_TEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/packages/package_cli_edit
                    -DERGO_PKG_CLI=$<TARGET_FILE:ergo_pkg_cli>
                    -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/packages/test_package_cli_edit.cmake)
        endif()
        if(TARGET ergo_package_viz_cli)
            # W6: ergo_pkg status → ergo_package_viz --view host (JUnit 見本・欠けた入力・決定性)。
            add_test(NAME test_package_host_view
                COMMAND ${CMAKE_COMMAND}
                    -DERGO_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                    -DERGO_TEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/packages/package_host_view
                    -DERGO_PKG_CLI=$<TARGET_FILE:ergo_pkg_cli>
                    -DERGO_PACKAGE_VIZ_CLI=$<TARGET_FILE:ergo_package_viz_cli>
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/packages/test_package_host_view.cmake)
        endif()
    endif()
    # W3: outdated / watch (ergo_package_watch があるときだけ)。
    if(TARGET ergo_package_watch)
        target_sources(ergo_pkg_cli PRIVATE
            tools/package_cli/git_history.cpp
            tools/package_cli/git_remote_source.cpp
            tools/package_cli/git_sibling_source.cpp
            tools/package_cli/ref_source_router.cpp
            tools/package_cli/watch_paths.cpp
            tools/package_cli/watch_host.cpp
            tools/package_cli/watch_observe.cpp
            tools/package_cli/cmd_outdated.cpp
            tools/package_cli/cmd_watch.cpp
            tools/package_cli/provenance_endpoints.cpp
            tools/package_cli/provenance_sources.cpp)
        target_link_libraries(ergo_pkg_cli PRIVATE ergo_package_watch)
        target_compile_definitions(ergo_pkg_cli PRIVATE ERGO_PKG_CLI_WITH_WATCH=1)
        # W4: Revisor / Concordia の読み手は ergo_http (ERGO_BUILD_HTTP=ON) があるときだけ。 無ければ
        # --revisor-url / --concordia-url を渡しても sources.* は "unavailable" になる。
        if(TARGET ergo_http)
            target_sources(ergo_pkg_cli PRIVATE
                tools/package_cli/http_json_get.cpp
                tools/package_cli/revisor_review_source.cpp
                tools/package_cli/concordia_run_source.cpp)
            target_link_libraries(ergo_pkg_cli PRIVATE ergo_http)
            target_compile_definitions(ergo_pkg_cli PRIVATE ERGO_PKG_CLI_WITH_HTTP=1)
        endif()
        if(ERGO_BUILD_TESTS AND GIT_EXECUTABLE)
            add_test(NAME test_package_watch_cli
                COMMAND ${CMAKE_COMMAND}
                    -DERGO_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                    -DERGO_TEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/packages/package_watch_cli
                    -DERGO_PKG_CLI=$<TARGET_FILE:ergo_pkg_cli>
                    -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/packages/test_package_watch_cli.cmake)
        endif()
    endif()
elseif(ERGO_BUILD_PACKAGE_CLI)
    message(STATUS "ergo_pkg: requires ERGO_BUILD_PACKAGE_MANAGER — skipped")
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

# ---------------------------------------------------------------------------
# tests/thirdparty/ — smoke test for the managed dependency manager. Proves the
# fetched libraries (curl) link and run. Opt-in via
# -DERGO_BUILD_THIRDPARTY_SMOKE=ON (which forces ERGO_WITH_CURL on; see
# third_party/dependencies.cmake).
# ---------------------------------------------------------------------------
if(ERGO_BUILD_TESTS AND ERGO_BUILD_THIRDPARTY_SMOKE)
    add_executable(test_thirdparty_deps tests/thirdparty/deps_smoke.cpp)
    target_link_libraries(test_thirdparty_deps PRIVATE ergo_thirdparty ergo_gtest_main)
    add_test(NAME test_thirdparty_deps COMMAND test_thirdparty_deps)
endif()

# ---------------------------------------------------------------------------
# tests/packages/ — package hub rule tests (cmake -P, no compiler needed), the
# M0 invariance configure test (C-5) and the S2 route tests on fake packages
# (git repositories created by the test). See spec/data/ergo-packages.md.
# ---------------------------------------------------------------------------
if(ERGO_BUILD_TESTS)
    foreach(NAME manifest rules cycles features report acquire host_declaration host_defaults
                 watch_notice)
        add_test(NAME test_packages_${NAME}
            COMMAND ${CMAKE_COMMAND}
                -DERGO_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DERGO_TEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/packages/${NAME}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/packages/test_${NAME}.cmake)
    endforeach()
    add_test(NAME test_packages_m0_invariance
        COMMAND ${CMAKE_COMMAND}
            -DERGO_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DERGO_TEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/packages/m0_invariance
            -DERGO_TEST_GENERATOR=${CMAKE_GENERATOR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/packages/test_m0_invariance.cmake)
    find_package(Git QUIET)
    if(GIT_EXECUTABLE)
        add_test(NAME test_packages_hub_routes
            COMMAND ${CMAKE_COMMAND}
                -DERGO_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DERGO_TEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/packages/hub_routes
                -DERGO_TEST_GENERATOR=${CMAKE_GENERATOR}
                -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/packages/test_hub_routes.cmake)
        # W1 (spec/feature/package-manager.md §7): host declarations on a
        # configure, the bootstrap's search order, and prefetch -> offline
        # route 5. Fake repositories are created by the tests (no network).
        # host_use: the declaration's use / optional / features as configure
        # defaults (Ludellus-Native L1-L3 on a fake host and a fake Ergo).
        foreach(NAME host_configure bootstrap prefetch host_use)
            add_test(NAME test_packages_${NAME}
                COMMAND ${CMAKE_COMMAND}
                    -DERGO_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                    -DERGO_TEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/packages/${NAME}
                    -DERGO_TEST_GENERATOR=${CMAKE_GENERATOR}
                    -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/packages/test_${NAME}.cmake)
        endforeach()
    endif()
endif()
