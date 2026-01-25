# FindKokkos.cmake
# Detects if Kokkos::kokkos target is already defined (e.g. via add_subdirectory)

if(TARGET Kokkos::kokkos)
    set(Kokkos_FOUND TRUE)
    set(Kokkos_VERSION "4.6.99") # Dummy version to satisfy > 4.1 requirements if needed

    # Cabana might check for these variables, although it should link against the target
    if(NOT Kokkos_INCLUDE_DIRS)
        get_target_property(Kokkos_INCLUDE_DIRS Kokkos::kokkos INTERFACE_INCLUDE_DIRECTORIES)
    endif()

    message(STATUS "Found Kokkos via existing target Kokkos::kokkos")
else()
    # Fallback to standard config mode if target not found
    # This prevents infinite recursion if we were called via find_package in a context
    # where we actually wanted to search system paths, though here we prioritize the submodule.
    # We explicitly look for Config mode to avoid finding THIS module again if it were in the path.
    find_package(Kokkos CONFIG)
endif()
