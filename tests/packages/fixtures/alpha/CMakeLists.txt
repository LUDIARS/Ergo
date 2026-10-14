# Fake package "Alpha" for tests/packages/test_hub_routes.cmake. Shaped like
# Pictor: a render target whose GPU path publishes a contract definition, a
# web target behind an option, and an Emscripten build that skips the main
# target. The test copies this directory and makes it a git repository.
cmake_minimum_required(VERSION 3.20)
project(FakeAlpha VERSION 1.2.0 LANGUAGES NONE)

option(ALPHA_BUILD_DEMO "Fake demo switch (manifest cache default OFF)" ON)
option(ALPHA_BUILD_WEB "Fake web target (feature web)" OFF)

set_property(GLOBAL PROPERTY FAKE_ALPHA_SEEN
    "demo=${ALPHA_BUILD_DEMO};web=${ALPHA_BUILD_WEB};nogpu=${ALPHA_NO_GPU}")

if(ALPHA_BUILD_WEB)
    add_library(alpha_web INTERFACE)
endif()
if(EMSCRIPTEN)
    return()
endif()

add_library(alpha INTERFACE)
if(NOT ALPHA_NO_GPU)
    target_compile_definitions(alpha INTERFACE ALPHA_HAS_GPU=1)
endif()
