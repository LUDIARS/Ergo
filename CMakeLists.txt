cmake_minimum_required(VERSION 3.14)
project(ergo_sound VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# オプション
option(ERGO_SOUND_BUILD_TESTS "テストをビルドする" ON)
option(ERGO_SOUND_BUILD_DUMMY "ダミープラグをビルドする" ON)

# ソースファイル
set(SOUND_SOURCES
    src/audio_decoder.cpp
    src/audio_stream.cpp
    src/audio_engine.cpp
    src/waveform_processor.cpp
    src/quantizer.cpp
)

# 静的ライブラリ
add_library(ergo_sound STATIC ${SOUND_SOURCES})

target_include_directories(ergo_sound PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ergo_sound PUBLIC cxx_std_17)

# プラットフォーム別リンク
if(UNIX AND NOT APPLE)
    target_link_libraries(ergo_sound PUBLIC pthread stdc++fs)
elseif(APPLE)
    target_link_libraries(ergo_sound PUBLIC pthread)
endif()

# ダミープラグ
if(ERGO_SOUND_BUILD_DUMMY)
    add_library(ergo_sound_dummy STATIC src/sound_dummy.cpp)
    target_include_directories(ergo_sound_dummy PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(ergo_sound_dummy PUBLIC ergo_sound)
endif()

# テスト
if(ERGO_SOUND_BUILD_TESTS)
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
        tests/test_audio_decoder.cpp
        tests/test_audio_stream.cpp
        tests/test_audio_engine.cpp
        tests/test_waveform_processor.cpp
        tests/test_quantizer.cpp
    )

    add_executable(sound_tests ${TEST_SOURCES})
    target_link_libraries(sound_tests PRIVATE ergo_sound GTest::gtest_main)
    target_include_directories(sound_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    include(GoogleTest)
    gtest_discover_tests(sound_tests)
endif()
