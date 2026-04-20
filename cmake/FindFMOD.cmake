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
# (linux / macos: api/core/lib/<arch>/libfmod.{so,dylib})

set(_FMOD_HINT_DIRS
    "${FMOD_SDK_DIR}"
    "$ENV{FMOD_SDK_DIR}"
    "C:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows"
    "C:/Program Files/FMOD SoundSystem/FMOD Studio API Windows"
)

find_path(FMOD_INCLUDE_DIR
    NAMES api/core/inc/fmod.hpp
    HINTS ${_FMOD_HINT_DIRS}
    NO_DEFAULT_PATH
)

if(FMOD_INCLUDE_DIR)
    set(FMOD_INCLUDE_DIR "${FMOD_INCLUDE_DIR}/api/core/inc" CACHE PATH "FMOD headers" FORCE)
endif()

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

foreach(HINT ${_FMOD_HINT_DIRS})
    if(IS_DIRECTORY "${HINT}/api/core/lib/${_FMOD_ARCH}")
        find_library(FMOD_LIBRARY
            NAMES ${_FMOD_LIB_NAMES}
            PATHS "${HINT}/api/core/lib/${_FMOD_ARCH}"
            NO_DEFAULT_PATH)

        foreach(DLL ${_FMOD_DLL_NAMES})
            if(EXISTS "${HINT}/api/core/lib/${_FMOD_ARCH}/${DLL}")
                set(FMOD_RUNTIME "${HINT}/api/core/lib/${_FMOD_ARCH}/${DLL}")
                break()
            endif()
        endforeach()
    endif()
    if(FMOD_LIBRARY)
        break()
    endif()
endforeach()

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
