# FindFMOD.cmake — locate FMOD Core SDK.
#
# Inputs (any of):
#   FMOD_SDK_DIR   CMake cache variable pointing at the FMOD Studio
#                  API install root (the dir that contains `api/core/`).
#   $ENV{FMOD_SDK_DIR}  Same, via environment.
#
# Outputs:
#   FMOD_FOUND          TRUE if headers + library were located
#   FMOD_INCLUDE_DIR    path containing `fmod.hpp`
#   FMOD_LIBRARY        the import library to link against
#   FMOD_RUNTIME        path to the DLL / .so for POST_BUILD copies
#   Target: FMOD::FMOD  INTERFACE library wrapping the above
#
# The FMOD Studio API installer lays out paths like:
#   api/core/inc/fmod.hpp
#   api/core/lib/x64/fmod_vc.lib
#   api/core/lib/x64/fmod.dll
# (Linux: api/core/lib/<arch>/libfmod.so; macOS: api/core/lib/libfmod.dylib)

set(_FMOD_HINT_DIRS
    "${FMOD_SDK_DIR}"
    "$ENV{FMOD_SDK_DIR}"
    "C:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows"
    "C:/Program Files/FMOD SoundSystem/FMOD Studio API Windows"
)

find_path(FMOD_INCLUDE_DIR
    NAMES fmod.hpp
    HINTS ${_FMOD_HINT_DIRS}
    PATH_SUFFIXES api/core/inc
    NO_DEFAULT_PATH
)

# Pick the arch-appropriate subfolder.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_FMOD_ARCH "x64")
else()
    set(_FMOD_ARCH "x86")
endif()

if(WIN32)
    set(_FMOD_LIB_NAMES   fmod_vc fmod)
    set(_FMOD_DLL_NAMES   fmod.dll fmodL.dll)
elseif(APPLE)
    set(_FMOD_LIB_NAMES   fmod)
    set(_FMOD_DLL_NAMES   libfmod.dylib libfmodL.dylib)
else()
    set(_FMOD_LIB_NAMES   fmod)
    set(_FMOD_DLL_NAMES   libfmod.so libfmodL.so)
endif()

set(_FMOD_LIB_SUFFIXES "api/core/lib/${_FMOD_ARCH}")
if(APPLE)
    # The macOS SDK ships its universal dylib directly in lib/, not lib/x64/.
    list(PREPEND _FMOD_LIB_SUFFIXES api/core/lib)
endif()
find_library(FMOD_LIBRARY
    NAMES ${_FMOD_LIB_NAMES}
    HINTS ${_FMOD_HINT_DIRS}
    PATH_SUFFIXES ${_FMOD_LIB_SUFFIXES}
    NO_DEFAULT_PATH)

# Resolve the runtime beside the selected import library, including on a
# repeated configure; do not accidentally stage a DLL from another SDK.
unset(FMOD_RUNTIME)
if(FMOD_LIBRARY)
    get_filename_component(_FMOD_LIBRARY_DIR "${FMOD_LIBRARY}" DIRECTORY)
    foreach(DLL ${_FMOD_DLL_NAMES})
        if(EXISTS "${_FMOD_LIBRARY_DIR}/${DLL}")
            set(FMOD_RUNTIME "${_FMOD_LIBRARY_DIR}/${DLL}")
            break()
        endif()
    endforeach()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FMOD
    REQUIRED_VARS FMOD_INCLUDE_DIR FMOD_LIBRARY)

if(FMOD_FOUND AND NOT TARGET FMOD::FMOD)
    add_library(FMOD::FMOD INTERFACE IMPORTED)
    set_target_properties(FMOD::FMOD PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${FMOD_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES      "${FMOD_LIBRARY}")
    if(FMOD_RUNTIME)
        set_target_properties(FMOD::FMOD PROPERTIES
            INTERFACE_IMPORTED_LOCATION "${FMOD_RUNTIME}")
    endif()
endif()

mark_as_advanced(FMOD_INCLUDE_DIR FMOD_LIBRARY FMOD_RUNTIME)
