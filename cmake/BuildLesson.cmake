# Excubitor build entry. Compilation only: no service or game process is started.
cmake_minimum_required(VERSION 3.20)
get_filename_component(lesson_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(lesson_build "${lesson_root}/build/lesson")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${lesson_root}" -B "${lesson_build}"
    -G "Visual Studio 18 2026" -A x64
    -DERGO_BUILD_TESTS=OFF -DERGO_BUILD_EXAMPLES=ON -DERGO_BUILD_STUDIO=OFF
    -DERGO_BUILD_RENDER=OFF -DERGO_BUILD_UI=OFF -DERGO_WITH_CURL=OFF
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON
    RESULT_VARIABLE configured)
if(NOT configured EQUAL 0)
    message(FATAL_ERROR "Lesson configure failed: ${configured}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${lesson_build}" --config Release
    --target ergo_example_lesson_action_feel --parallel 4 RESULT_VARIABLE built)
if(NOT built EQUAL 0)
    message(FATAL_ERROR "Lesson build failed: ${built}")
endif()
