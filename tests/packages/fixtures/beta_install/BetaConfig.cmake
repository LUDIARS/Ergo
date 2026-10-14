# Installed form of the fake package "Beta" (find_package route).
if(NOT TARGET Beta::Core)
    add_library(Beta::Core INTERFACE IMPORTED)
endif()
