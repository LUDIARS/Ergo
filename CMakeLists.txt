cmake_minimum_required(VERSION 3.16)
project(ErgoInput LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ERGO_INPUT_BUILD_TESTS "Build input system tests" ON)
option(ERGO_INPUT_BUILD_DUMMY "Build dummy plug implementations" ON)

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

find_package(Threads REQUIRED)
target_link_libraries(ergo_input PUBLIC Threads::Threads)

if(ERGO_INPUT_BUILD_DUMMY)
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

if(ERGO_INPUT_BUILD_TESTS)
    enable_testing()

    add_library(gtest_main STATIC tests/gtest_main.cpp)
    target_include_directories(gtest_main PUBLIC third_party)

    function(ergo_add_test NAME)
        add_executable(${NAME} tests/input/${NAME}.cpp)
        target_link_libraries(${NAME} PRIVATE ergo_input gtest_main)
        add_test(NAME ${NAME} COMMAND ${NAME})
    endfunction()

    ergo_add_test(test_double_buffer)
    ergo_add_test(test_input_buffer)
    ergo_add_test(test_observer)
    ergo_add_test(test_polling_thread)
    ergo_add_test(test_mouse_device)
    ergo_add_test(test_keyboard_device)
    ergo_add_test(test_gamepad_device)
    ergo_add_test(test_usb_device)
    ergo_add_test(test_input_system)
endif()
