cmake_minimum_required(VERSION 3.14)
project(ergo_compose VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# オプション
option(ERGO_COMPOSE_BUILD_TESTS "テストをビルドする" ON)
option(ERGO_COMPOSE_BUILD_DUMMY "ダミープラグをビルドする" ON)

# ソースファイル
set(COMPOSE_SOURCES
    src/transpiler.cpp
    src/actor_composer.cpp
    src/scene_builder.cpp
    src/quic_commander.cpp
    src/hot_reloader.cpp
    src/compose_system.cpp
)

# 静的ライブラリ
add_library(ergo_compose STATIC ${COMPOSE_SOURCES})

target_include_directories(ergo_compose PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ergo_compose PUBLIC cxx_std_17)

# プラットフォーム別リンク
if(UNIX AND NOT APPLE)
    target_link_libraries(ergo_compose PUBLIC pthread stdc++fs)
elseif(APPLE)
    target_link_libraries(ergo_compose PUBLIC pthread)
endif()

# ダミープラグ
if(ERGO_COMPOSE_BUILD_DUMMY)
    add_library(ergo_compose_dummy STATIC src/compose_dummy.cpp)
    target_include_directories(ergo_compose_dummy PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(ergo_compose_dummy PUBLIC ergo_compose)
endif()

# テスト
if(ERGO_COMPOSE_BUILD_TESTS)
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
        tests/test_transpiler.cpp
        tests/test_actor_composer.cpp
        tests/test_scene_builder.cpp
        tests/test_quic_commander.cpp
        tests/test_hot_reloader.cpp
        tests/test_compose_system.cpp
    )

    add_executable(compose_tests ${TEST_SOURCES})
    target_link_libraries(compose_tests PRIVATE ergo_compose GTest::gtest_main)
    target_include_directories(compose_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    include(GoogleTest)
    gtest_discover_tests(compose_tests)
endif()
