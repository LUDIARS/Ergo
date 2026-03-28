cmake_minimum_required(VERSION 3.14)
project(ergo_event VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# オプション
option(ERGO_EVENT_BUILD_TESTS "テストをビルドする" ON)
option(ERGO_EVENT_BUILD_DUMMY "ダミープラグをビルドする" ON)

# ソースファイル
set(EVENT_SOURCES
    src/event_queue.cpp
    src/event_sequencer.cpp
)

# 静的ライブラリ
add_library(ergo_event STATIC ${EVENT_SOURCES})

target_include_directories(ergo_event PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ergo_event PUBLIC cxx_std_17)

# プラットフォーム別リンク
if(UNIX AND NOT APPLE)
    target_link_libraries(ergo_event PUBLIC pthread)
elseif(APPLE)
    target_link_libraries(ergo_event PUBLIC pthread)
endif()

# ダミープラグ
if(ERGO_EVENT_BUILD_DUMMY)
    add_library(ergo_event_dummy STATIC src/event_dummy.cpp)
    target_include_directories(ergo_event_dummy PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(ergo_event_dummy PUBLIC ergo_event)
endif()

# テスト
if(ERGO_EVENT_BUILD_TESTS)
    enable_testing()

    # GoogleTest
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    FetchContent_MakeAvailable(googletest)

    # テストファイル
    set(TEST_SOURCES
        tests/test_event_queue.cpp
        tests/test_event_sequencer.cpp
    )

    add_executable(event_tests ${TEST_SOURCES})
    target_link_libraries(event_tests PRIVATE ergo_event GTest::gtest_main)
    target_include_directories(event_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    include(GoogleTest)
    gtest_discover_tests(event_tests)
endif()
