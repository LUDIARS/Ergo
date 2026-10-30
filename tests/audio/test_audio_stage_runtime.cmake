cmake_minimum_required(VERSION 3.20)
# ergo_audio_stage_runtime() called from a host's scope
# (spec/feature/module/audio.md「ホストへの組み込み」). A fake FMOD SDK
# (api/core/inc/fmod.hpp + the per-OS library / runtime as empty files) is laid
# out in the work dir and the host (fixtures/stage_host) add_subdirectory()s
# Ergo with ERGO_AUDIO_BACKEND=fmod:
#   fmod   -> building mygame copies the runtime next to the executable
#   dummy  -> the call returns quietly (no staging, no warning)
#   AUDIO=OFF -> the call is still defined and stops the configure, naming it
#
#   cmake -DERGO_SOURCE_DIR=<ergo> -DERGO_TEST_WORK_DIR=<dir>
#         [-DERGO_TEST_GENERATOR=<gen>] [-DERGO_TEST_PLATFORM=<platform>]
#         [-DERGO_TEST_CXX_COMPILER=<cxx>] [-DERGO_TEST_CXX_LAUNCHER=<launcher>]
#         -P tests/audio/test_audio_stage_runtime.cmake
include("${CMAKE_CURRENT_LIST_DIR}/../packages/host_test_support.cmake")

set(_fixture "${CMAKE_CURRENT_LIST_DIR}/fixtures/stage_host")
file(REMOVE_RECURSE "${ERGO_TEST_WORK_DIR}")

# Fake SDK in FindFMOD.cmake's layout. Both architectures, so a 32-bit target
# finds it too.
set(_sdk "${ERGO_TEST_WORK_DIR}/fmod_sdk")
file(WRITE "${_sdk}/api/core/inc/fmod.hpp" "// fake FMOD header\n")
if(CMAKE_HOST_WIN32)
    set(_runtime_name fmod.dll)
    foreach(_arch x64 x86)
        file(WRITE "${_sdk}/api/core/lib/${_arch}/fmod_vc.lib" "")
        file(WRITE "${_sdk}/api/core/lib/${_arch}/fmod.dll" "fake fmod runtime\n")
    endforeach()
elseif(CMAKE_HOST_APPLE)
    set(_runtime_name libfmod.dylib)
    file(WRITE "${_sdk}/api/core/lib/libfmod.dylib" "fake fmod runtime\n")
else()
    set(_runtime_name libfmod.so)
    foreach(_arch x64 x86)
        file(WRITE "${_sdk}/api/core/lib/${_arch}/libfmod.so" "fake fmod runtime\n")
    endforeach()
endif()

# --- fmod: staged next to the executable ----------------------------------
set(B "${ERGO_TEST_WORK_DIR}/fmod")
host_configure("${B}" "${_fixture}" _rc _log
    -DERGO_AUDIO_BACKEND=fmod "-DFMOD_SDK_DIR=${_sdk}")
if(NOT _rc EQUAL 0)
    host_fail("fmod: host configure failed\n${_log}")
endif()
if(NOT _log MATCHES "ergo_audio: backend = FMOD")
    host_fail("fmod: Ergo did not pick the FMOD backend\n${_log}")
endif()
string(REGEX REPLACE "[ \t\r\n]+" " " _log "${_log}")  # CMake wraps message text
if(_log MATCHES "cannot stage it|nothing is staged")
    host_fail("fmod: the runtime should have been resolved\n${_log}")
endif()
host_build("${B}" mygame)
host_generated_file("${B}" mygame_dir_ _dir_txt)
file(READ "${_dir_txt}" _exe_dir)
if(NOT EXISTS "${_exe_dir}/${_runtime_name}")
    host_fail("fmod: ${_runtime_name} was not staged into ${_exe_dir}")
endif()
file(READ "${_exe_dir}/${_runtime_name}" _staged)
if(NOT _staged STREQUAL "fake fmod runtime\n")
    host_fail("fmod: ${_exe_dir}/${_runtime_name} is not the SDK's runtime")
endif()
host_pass("fmod: ${_runtime_name} staged next to mygame")

# --- dummy: nothing to stage, and nothing said ----------------------------
set(B "${ERGO_TEST_WORK_DIR}/dummy")
host_configure("${B}" "${_fixture}" _rc _log -DERGO_AUDIO_BACKEND=dummy)
if(NOT _rc EQUAL 0)
    host_fail("dummy: host configure failed\n${_log}")
endif()
if(_log MATCHES "ergo_audio_stage_runtime")
    host_fail("dummy: the call should return quietly\n${_log}")
endif()
host_pass("dummy: quiet no-op")

# --- ERGO_BUILD_AUDIO=OFF: still defined, says why ------------------------
set(B "${ERGO_TEST_WORK_DIR}/audio_off")
host_configure("${B}" "${_fixture}" _rc _log -DERGO_BUILD_AUDIO=OFF)
string(REGEX REPLACE "[ \t\r\n]+" " " _log "${_log}")  # CMake wraps message text
if(_rc EQUAL 0 OR NOT _log MATCHES "ergo_audio does not exist \\(ERGO_BUILD_AUDIO=OFF\\)")
    host_fail("audio off: expected a configure error naming ERGO_BUILD_AUDIO=OFF\n${_log}")
endif()
host_pass("audio off: configure error instead of 'Unknown CMake command'")
