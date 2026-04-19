cmake_minimum_required(VERSION 3.16)
project(ErgoInspector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ERGO_INSPECTOR_BUILD_TESTS  "Build inspector tests" ON)
option(ERGO_INSPECTOR_BUILD_DUMMY  "Build dummy plug implementations" ON)
option(ERGO_INSPECTOR_BUILD_SERVER "Build the WebSocket server (POSIX-only; Windows uses dummy)" ON)

# ---------------------------------------------------------------------------
# Core library
# ---------------------------------------------------------------------------
set(ERGO_INSPECTOR_SOURCES
    src/inspector/tweakable.cpp
    src/inspector/command_queue.cpp
    src/inspector/inspector.cpp
    # Platform-neutral helpers: always compile (also useful for tests on any host).
    src/inspector/json_min.cpp
    src/inspector/ws_handshake.cpp
)

if(ERGO_INSPECTOR_BUILD_SERVER AND NOT WIN32)
    list(APPEND ERGO_INSPECTOR_SOURCES src/inspector/server_posix.cpp)
else()
    list(APPEND ERGO_INSPECTOR_SOURCES src/inspector/server_dummy.cpp)
    if(ERGO_INSPECTOR_BUILD_SERVER AND WIN32)
        message(STATUS "ergo_inspector: WebSocket server is POSIX-only; using dummy on Windows")
    endif()
endif()

add_library(ergo_inspector ${ERGO_INSPECTOR_SOURCES})
target_include_directories(ergo_inspector PUBLIC include)
target_compile_definitions(ergo_inspector PUBLIC ERGO_INSPECTOR_ENABLED=1)

find_package(Threads REQUIRED)
target_link_libraries(ergo_inspector PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
# Dummy plug — satisfies all ergo_inspector symbols with no-op implementations.
# Use when ERGO_INSPECTOR_ENABLED is left defined but actual inspection isn't
# wanted (e.g. headless CI that exercises host-side hooks).
# ---------------------------------------------------------------------------
if(ERGO_INSPECTOR_BUILD_DUMMY)
    add_library(ergo_inspector_dummy
        src/inspector/inspector_dummy.cpp
    )
    target_include_directories(ergo_inspector_dummy PUBLIC include)
    target_link_libraries(ergo_inspector_dummy PUBLIC Threads::Threads)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(ERGO_INSPECTOR_BUILD_TESTS)
    enable_testing()

    add_library(gtest_main STATIC tests/gtest_main.cpp)
    target_include_directories(gtest_main PUBLIC third_party)

    function(ergo_add_test NAME)
        add_executable(${NAME} tests/inspector/${NAME}.cpp)
        target_link_libraries(${NAME} PRIVATE ergo_inspector gtest_main)
        add_test(NAME ${NAME} COMMAND ${NAME})
    endfunction()

    ergo_add_test(test_tweakable)
    ergo_add_test(test_command_queue)
    ergo_add_test(test_inspector)
    ergo_add_test(test_ws_handshake)
    ergo_add_test(test_json_min)
endif()
