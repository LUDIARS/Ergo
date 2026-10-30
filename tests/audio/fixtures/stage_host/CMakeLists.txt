# Host for tests/audio/test_audio_stage_runtime.cmake. It calls
# ergo_audio_stage_runtime() from its own scope, where Ergo's directory
# variables are not visible. mygame does not link ergo_audio: the fake FMOD
# SDK's library is an empty file and cannot be linked.
cmake_minimum_required(VERSION 3.20)
project(StageHost LANGUAGES CXX)

add_subdirectory("${ERGO_SOURCE_DIR}" ergo EXCLUDE_FROM_ALL)

add_executable(mygame main.cpp)
ergo_audio_stage_runtime(mygame)

file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/mygame_dir_$<CONFIG>.txt"
    CONTENT "$<TARGET_FILE_DIR:mygame>")
