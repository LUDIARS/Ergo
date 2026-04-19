cmake_minimum_required(VERSION 3.16)
project(ErgoParticle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ERGO_PARTICLE_BUILD_TESTS "Build particle tests" ON)
option(ERGO_PARTICLE_BUILD_DUMMY "Build dummy plug implementations" ON)

# ---------------------------------------------------------------------------
# Core library — CPU simulation only. No Vulkan/Pictor dependency, so this
# part always builds and is fully testable on any host.
# ---------------------------------------------------------------------------
set(ERGO_PARTICLE_SOURCES
    src/particle/effect_config.cpp
    src/particle/particle_system.cpp
)

add_library(ergo_particle ${ERGO_PARTICLE_SOURCES})
target_include_directories(ergo_particle PUBLIC include)

find_package(Threads REQUIRED)
target_link_libraries(ergo_particle PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
# Optional Vulkan billboard renderer (requires the host to have already added
# Pictor and Vulkan). When unavailable, renderer.h declarations are gated by
# ERGO_PARTICLE_HAS_RENDERER.
# ---------------------------------------------------------------------------
if(TARGET pictor)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        target_sources(ergo_particle PRIVATE src/particle/renderer.cpp)
        target_link_libraries(ergo_particle PUBLIC pictor Vulkan::Vulkan)
        target_compile_definitions(ergo_particle PUBLIC ERGO_PARTICLE_HAS_RENDERER=1)

        if(Vulkan_GLSLC_EXECUTABLE)
            set(EP_SHADER_SOURCES
                ${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.vert
                ${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.frag
            )
            set(EP_SPIRV_FILES)
            foreach(SHADER ${EP_SHADER_SOURCES})
                get_filename_component(NAME ${SHADER} NAME)
                set(SPIRV_OUT ${CMAKE_BINARY_DIR}/shaders/${NAME}.spv)
                add_custom_command(
                    OUTPUT ${SPIRV_OUT}
                    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
                    COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${SPIRV_OUT}
                    DEPENDS ${SHADER}
                    COMMENT "Compiling ${NAME} -> SPIR-V"
                )
                list(APPEND EP_SPIRV_FILES ${SPIRV_OUT})
            endforeach()
            add_custom_target(ergo_particle_shaders ALL DEPENDS ${EP_SPIRV_FILES})
            add_dependencies(ergo_particle ergo_particle_shaders)
        endif()

        message(STATUS "ergo_particle: renderer enabled (Pictor + Vulkan)")
    else()
        message(STATUS "ergo_particle: renderer disabled (Vulkan not found)")
    endif()
else()
    message(STATUS "ergo_particle: renderer disabled (pictor target not found)")
endif()

# ---------------------------------------------------------------------------
# Dummy plug
# ---------------------------------------------------------------------------
if(ERGO_PARTICLE_BUILD_DUMMY)
    add_library(ergo_particle_dummy
        src/particle/particle_dummy.cpp
    )
    target_include_directories(ergo_particle_dummy PUBLIC include)
    target_link_libraries(ergo_particle_dummy PUBLIC Threads::Threads)
endif()

# ---------------------------------------------------------------------------
# Tests — simulation only, no Vulkan needed.
# ---------------------------------------------------------------------------
if(ERGO_PARTICLE_BUILD_TESTS)
    enable_testing()

    add_library(gtest_main STATIC tests/gtest_main.cpp)
    target_include_directories(gtest_main PUBLIC third_party)

    function(ergo_add_test NAME)
        add_executable(${NAME} tests/particle/${NAME}.cpp)
        target_link_libraries(${NAME} PRIVATE ergo_particle gtest_main)
        add_test(NAME ${NAME} COMMAND ${NAME})
    endfunction()

    ergo_add_test(test_effect_config)
    ergo_add_test(test_particle_system)
endif()
