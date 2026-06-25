# ---------------------------------------------------------------------------
# cmake/deps/kazmath.cmake — kazmath integration
#
# kazmath (https://github.com/Kazade/kazmath) is a small C89 math library
# (vec2/3/4, mat3/4, quaternion, aabb, plane, ray). We do NOT run its bundled
# CMake: it declares cmake_minimum_required(2.8) (rejected by modern CMake) and
# also builds an OpenGL matrix-stack helper (kazmath/GL/) we don't want. Instead
# we populate the source and compile the core translation units into our own
# canonical target `kazmath` so consumers get a stable, GL-free library.
#
# Consume with:  target_link_libraries(<tgt> PRIVATE kazmath)
#                #include <kazmath/kazmath.h>
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

ergo_populate_dependency(kazmath OUT_SOURCE_DIR _kazmath_src)

# Core C sources (everything under kazmath/ except the optional GL/ helpers).
set(_kazmath_sources
    aabb2.c aabb3.c
    mat3.c mat4.c
    plane.c quaternion.c
    ray2.c ray3.c
    utility.c
    vec2.c vec3.c vec4.c)
list(TRANSFORM _kazmath_sources PREPEND "${_kazmath_src}/kazmath/")

foreach(_f IN LISTS _kazmath_sources)
    if(NOT EXISTS "${_f}")
        message(FATAL_ERROR
            "kazmath source '${_f}' missing — upstream layout changed; update cmake/deps/kazmath.cmake")
    endif()
endforeach()

add_library(kazmath STATIC ${_kazmath_sources})
# Source root is the include root so consumers resolve <kazmath/...>.
target_include_directories(kazmath PUBLIC "${_kazmath_src}")
set_target_properties(kazmath PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    # kazmath predates _CRT_SECURE_NO_WARNINGS hygiene; silence C runtime noise.
    target_compile_definitions(kazmath PRIVATE _CRT_SECURE_NO_WARNINGS)
    # As the only C target, kazmath otherwise falls back to cl.exe's default /MT
    # (static CRT) and clashes (LNK4098) with Ergo's dynamic-CRT C++ targets.
    # Pin it to the dynamic CRT (CMake's own default) so it links cleanly.
    set_target_properties(kazmath PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()
