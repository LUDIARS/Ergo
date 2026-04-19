cmake_minimum_required(VERSION 3.16)
project(ErgoInspector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ERGO_INSPECTOR_BUILD_TESTS "Build inspector tests" ON)
option(ERGO_INSPECTOR_BUILD_DUMMY "Build dummy plug implementations" ON)
option(ERGO_INSPECTOR_BUILD_SERVER "Build the WebSocket server (off = registry-only)" OFF)

# ---------------------------------------------------------------------------
# Real implementation
# ---------------------------------------------------------------------------
set(ERGO_INSPECTOR_SOURCES
    src/inspector/tweakable.cpp
    src/inspector/command_queue.cpp
    src/inspector/inspector.cpp
)

if(ERGO_INSPECTOR_BUILD_SERVER)
    list(APPEND ERGO_INSPECTOR_SOURCES
        src/inspector/json_min.cpp
        src/inspector/ws_handshake.cpp
    )
    if(WIN32)
        list(APPEND ERGO_INSPECTOR_SOURCES src/inspector/server_win32.cpp)
    else()
        list(APPEND ERGO_INSPECTOR_SOURCES src/inspector/server_posix.cpp)
    endif()
else()
    list(APPEND ERGO_INSPECTOR_SOURCES src/inspector/server_dummy.cpp)
endif()

add_library(ergo_inspector ${ERGO_INSPECTOR_SOURCES})
target_include_directories(ergo_inspector PUBLIC include)
target_compile_definitions(ergo_inspector PUBLIC ERGO_INSPECTOR_ENABLED=1)

find_package(Threads REQUIRED)
target_link_libraries(ergo_inspector PUBLIC Threads::Threads)

if(WIN32 AND ERGO_INSPECTOR_BUILD_SERVER)
    target_link_libraries(ergo_inspector PUBLIC ws2_32)
endif()

# ---------------------------------------------------------------------------
# Dummy plug — satisfies all symbols with no-op implementations.
# Use when ERGO_INSPECTOR_ENABLED is *not* defined by the host but you still
# want unconditional compilation (e.g. for editor tooling).
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
    if(ERGO_INSPECTOR_BUILD_SERVER)
        ergo_add_test(test_ws_handshake)
        ergo_add_test(test_json_min)
    endif()
endif()
