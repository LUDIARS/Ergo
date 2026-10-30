# Host for tests/packages/test_host_consume.cmake: a real compiled host (a game
# would look like this) that takes Ergo by add_subdirectory and links Ergo::
# targets. The script configures it with CMAKE_CXX_STANDARD=14 (MSVC's default)
# and BUILD_SHARED_LIBS=ON / BUILD_TESTING=ON on the command line.
#   - host_app links input / log / bind / physics2d: Ergo's usage requirements
#     raise the host's C++14 (to C++20 through ergo_math); built with -Werror
#     on GCC / Clang
#   - test_math: the host's own target with the name of an Ergo test (defined
#     only while ERGO_BUILD_TESTS is OFF)
#   - host_plugin: a SHARED module linking Ergo's static libraries (PIC); not on
#     Windows, where Ergo has no DLL story to test
#   - host_smoke: the host's own test; Ergo must add none
# host_facts.txt records what the script asserts on.
cmake_minimum_required(VERSION 3.20)
project(HostConsume LANGUAGES CXX)

enable_testing()
add_subdirectory("${ERGO_SOURCE_DIR}" ergo)

add_executable(host_app main.cpp)
target_link_libraries(host_app PRIVATE Ergo::input Ergo::log Ergo::bind Ergo::physics2d)
# Hosts that build with -Werror must not trip over Ergo's public headers
# (clang: -Wdefaulted-function-deleted on World's move assignment).
target_compile_options(host_app PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Werror>)
add_test(NAME host_smoke COMMAND host_app)

# Only while Ergo's tests are off (the default): a host that opts into them
# (the script's ERGO_BUILD_TESTS toggle) gets Ergo's own test_math.
if(NOT ERGO_BUILD_TESTS)
    add_executable(test_math main.cpp)
    target_link_libraries(test_math PRIVATE Ergo::input Ergo::log Ergo::bind Ergo::physics2d)
endif()

if(NOT WIN32)
    add_library(host_plugin SHARED plugin.cpp)
    target_link_libraries(host_plugin PRIVATE Ergo::log Ergo::bind)
endif()

get_property(_ergo_targets DIRECTORY "${ERGO_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
get_property(_ergo_excluded DIRECTORY "${ERGO_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL)
set(_facts
    "BUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}"
    "BUILD_TESTING=${BUILD_TESTING}"
    "CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}"
    "ergo_exclude_from_all=${_ergo_excluded}")
foreach(_t IN LISTS _ergo_targets)
    if(NOT _t MATCHES "^ergo_(.+)$")
        continue()
    endif()
    set(_name "${CMAKE_MATCH_1}")
    get_target_property(_type ${_t} TYPE)
    if(_type MATCHES "_LIBRARY$")
        set(_alias "no-alias")
        if(TARGET Ergo::${_name})
            set(_alias "alias")
        endif()
        list(APPEND _facts "library ${_t} ${_type} ${_alias}")
    endif()
endforeach()
string(REPLACE ";" "\n" _facts "${_facts}")
file(WRITE "${CMAKE_BINARY_DIR}/host_facts.txt" "${_facts}\n")

# Where the build puts a linked Ergo library and one the host does not use
# (EXCLUDE_FROM_ALL: building `all` must not build it). Per configuration for
# multi-config generators.
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/host_files_$<CONFIG>.txt"
    CONTENT "linked=$<TARGET_FILE:ergo_log>\nunused=$<TARGET_FILE:ergo_package_manager>\n")
