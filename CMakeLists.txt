cmake_minimum_required(VERSION 3.16)
project(ErgoBind LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ERGO_BIND_BUILD_TESTS "Build bind tests" ON)
option(ERGO_BIND_BUILD_DUMMY "Build dummy plug implementations" ON)

set(ERGO_BIND_SOURCES
    src/bind/types.cpp
    src/bind/bind_engine.cpp
    src/bind/json_min.cpp
    src/bind/ws_client.cpp
)

add_library(ergo_bind ${ERGO_BIND_SOURCES})
target_include_directories(ergo_bind PUBLIC include)
target_compile_definitions(ergo_bind PUBLIC ERGO_BIND_ENABLED=1)

find_package(Threads REQUIRED)
target_link_libraries(ergo_bind PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(ergo_bind PUBLIC ws2_32)
endif()

if(ERGO_BIND_BUILD_DUMMY)
    add_library(ergo_bind_dummy
        src/bind/bind_dummy.cpp
    )
    target_include_directories(ergo_bind_dummy PUBLIC include)
    target_link_libraries(ergo_bind_dummy PUBLIC Threads::Threads)
endif()

if(ERGO_BIND_BUILD_TESTS)
    enable_testing()

    add_library(gtest_main STATIC tests/gtest_main.cpp)
    target_include_directories(gtest_main PUBLIC third_party)

    function(ergo_add_test NAME)
        add_executable(${NAME} tests/bind/${NAME}.cpp)
        target_link_libraries(${NAME} PRIVATE ergo_bind gtest_main)
        add_test(NAME ${NAME} COMMAND ${NAME})
    endfunction()

    ergo_add_test(test_types)
    ergo_add_test(test_json_min)
    ergo_add_test(test_bind_engine)
endif()
