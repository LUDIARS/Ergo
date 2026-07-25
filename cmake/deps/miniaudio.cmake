# ---------------------------------------------------------------------------
# cmake/deps/miniaudio.cmake — miniaudio integration
#
# miniaudio (https://github.com/mackron/miniaudio) is a single-header C audio
# playback/capture library. We do NOT run its bundled CMake: it builds the
# upstream examples/tests and installs a package we don't consume. Instead we
# populate the source and expose it as the INTERFACE target `miniaudio`, which
# only adds the include dir plus the platform link libs the implementation
# needs. The single MINIAUDIO_IMPLEMENTATION site lives in
# src/audio/audio_engine_miniaudio.cpp — no separate compiled TU here.
#
# Consume with:  target_link_libraries(<tgt> PRIVATE miniaudio)
#                #define MINIAUDIO_IMPLEMENTATION   // exactly one TU
#                #include <miniaudio.h>
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

ergo_populate_dependency(miniaudio OUT_SOURCE_DIR _miniaudio_src)

if(NOT EXISTS "${_miniaudio_src}/miniaudio.h")
    message(FATAL_ERROR
        "miniaudio header '${_miniaudio_src}/miniaudio.h' missing — upstream layout "
        "changed; update cmake/deps/miniaudio.cmake")
endif()

add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE "${_miniaudio_src}")

# Backend link requirements (upstream README): Windows resolves its audio APIs
# at runtime and needs nothing extra; POSIX needs pthread/dl/m.
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(miniaudio INTERFACE Threads::Threads ${CMAKE_DL_LIBS} m)
endif()
