cmake_minimum_required(VERSION 3.16)
project(Ergo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Top-level options. Hosts can flip per-module knobs as needed.
# ---------------------------------------------------------------------------
option(ERGO_BUILD_TESTS  "Build module tests"             ON)
option(ERGO_BUILD_DUMMY  "Build dummy plug libraries"     ON)

option(ERGO_BUILD_INPUT        "Build ergo_input"        ON)
option(ERGO_BUILD_INSPECTOR    "Build ergo_inspector"    ON)
option(ERGO_BUILD_PARTICLE     "Build ergo_particle"     ON)
option(ERGO_BUILD_GPU_PARTICLE "Build ergo_gpu_particle" ON)
option(ERGO_BUILD_BIND         "Build ergo_bind"         ON)
option(ERGO_BUILD_SOUND        "Build ergo_sound"        ON)
option(ERGO_BUILD_FRAME        "Build ergo_frame"        ON)
option(ERGO_BUILD_LOG          "Build ergo_log"          ON)
option(ERGO_BUILD_IO           "Build ergo_io"           ON)

# Per-module knobs (passed through to module sections below)
option(ERGO_INSPECTOR_BUILD_SERVER       "ergo_inspector: build POSIX WebSocket server" ON)
option(ERGO_PARTICLE_HAS_RENDERER        "ergo_particle: build Vulkan billboard renderer (requires pictor + Vulkan)" OFF)
option(ERGO_GPU_PARTICLE_COMPILE_SHADERS "ergo_gpu_particle: bake GLSL compute shaders to SPIR-V via glslc" ON)

find_package(Threads REQUIRED)

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
# ergo_inspector — in-process WS server for live var editing (legacy/optional)
# ---------------------------------------------------------------------------
if(ERGO_BUILD_INSPECTOR)
    set(_INSP_SRC
        src/inspector/tweakable.cpp
        src/inspector/command_queue.cpp
        src/inspector/inspector.cpp
        src/inspector/json_min.cpp
        src/inspector/ws_handshake.cpp
    )
    if(ERGO_INSPECTOR_BUILD_SERVER AND NOT WIN32)
        list(APPEND _INSP_SRC src/inspector/server_posix.cpp)
    else()
        list(APPEND _INSP_SRC src/inspector/server_dummy.cpp)
    endif()
    add_library(ergo_inspector ${_INSP_SRC})
    target_include_directories(ergo_inspector PUBLIC include)
    target_compile_definitions(ergo_inspector PUBLIC ERGO_INSPECTOR_ENABLED=1)
    target_link_libraries(ergo_inspector PUBLIC Threads::Threads)

    if(ERGO_BUILD_DUMMY)
        add_library(ergo_inspector_dummy src/inspector/inspector_dummy.cpp)
        target_include_directories(ergo_inspector_dummy PUBLIC include)
        target_link_libraries(ergo_inspector_dummy PUBLIC Threads::Threads)
    endif()

    if(ERGO_BUILD_TESTS)
        foreach(NAME
                test_tweakable test_command_queue test_inspector
                test_ws_handshake test_json_min)
            add_executable(${NAME} tests/inspector/${NAME}.cpp)
            target_link_libraries(${NAME} PRIVATE ergo_inspector ergo_gtest_main)
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
        src/bind/json_min.cpp
        src/bind/ws_client.cpp
    )
    target_include_directories(ergo_bind PUBLIC include)
    target_compile_definitions(ergo_bind PUBLIC ERGO_BIND_ENABLED=1)
    target_link_libraries(ergo_bind PUBLIC Threads::Threads)
    if(WIN32)
        target_link_libraries(ergo_bind PUBLIC ws2_32)
    endif()

    if(ERGO_BUILD_DUMMY)
        add_library(ergo_bind_dummy src/bind/bind_dummy.cpp)
        target_include_directories(ergo_bind_dummy PUBLIC include)
        target_link_libraries(ergo_bind_dummy PUBLIC Threads::Threads)
    endif()

    if(ERGO_BUILD_TESTS)
        foreach(NAME test_types test_json_min test_bind_engine)
            # test_json_min name collision with inspector — disambiguate.
            if("${NAME}" STREQUAL "test_json_min")
                set(TGT bind_test_json_min)
            else()
                set(TGT ${NAME})
            endif()
            add_executable(${TGT} tests/bind/${NAME}.cpp)
            target_link_libraries(${TGT} PRIVATE ergo_bind ergo_gtest_main)
            add_test(NAME ${TGT} COMMAND ${TGT})
        endforeach()
    endif()
endif()

# ---------------------------------------------------------------------------
# ergo_sound — WAV/OGG decode, streaming, mixing engine, waveform, quantizer
# ---------------------------------------------------------------------------
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
