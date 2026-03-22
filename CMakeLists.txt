cmake_minimum_required(VERSION 3.14)
project(ergo_network VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# オプション
option(ERGO_NETWORK_BUILD_TESTS "テストをビルドする" ON)
option(ERGO_NETWORK_BUILD_DUMMY "ダミープラグをビルドする" ON)

# ソースファイル
set(NETWORK_SOURCES
    src/types.cpp
    src/socket.cpp
    src/dns_resolver.cpp
    src/connection_pool.cpp
    src/http_client.cpp
    src/stream_channel.cpp
    src/network_system.cpp
)

# 静的ライブラリ
add_library(ergo_network STATIC ${NETWORK_SOURCES})

target_include_directories(ergo_network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ergo_network PUBLIC cxx_std_17)

# プラットフォーム別リンク
if(WIN32)
    target_link_libraries(ergo_network PUBLIC ws2_32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(ergo_network PUBLIC pthread)
elseif(APPLE)
    target_link_libraries(ergo_network PUBLIC pthread)
endif()

# ダミープラグ
if(ERGO_NETWORK_BUILD_DUMMY)
    add_library(ergo_network_dummy STATIC src/network_dummy.cpp)
    target_include_directories(ergo_network_dummy PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endif()

# テスト
if(ERGO_NETWORK_BUILD_TESTS)
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
        tests/test_socket.cpp
        tests/test_dns_resolver.cpp
        tests/test_connection_pool.cpp
        tests/test_http_client.cpp
        tests/test_stream_channel.cpp
        tests/test_network_system.cpp
    )

    add_executable(network_tests ${TEST_SOURCES})
    target_link_libraries(network_tests PRIVATE ergo_network GTest::gtest_main)
    target_include_directories(network_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    include(GoogleTest)
    gtest_discover_tests(network_tests)
endif()
