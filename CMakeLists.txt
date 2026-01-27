cmake_minimum_required(VERSION 3.25)
project(microvtk VERSION 0.1.0 LANGUAGES CXX)

# Standard C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(MICROVTK_BUILD_TESTS "Build unit tests" OFF)
option(MICROVTK_BUILD_EXAMPLES "Build examples" OFF)
option(MICROVTK_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(MICROVTK_USE_ZLIB "Enable ZLib compression support" ON)
option(MICROVTK_USE_LZ4 "Enable LZ4 compression support" ON)
option(MICROVTK_USE_KOKKOS "Enable Kokkos adapter support" OFF)
option(MICROVTK_USE_CABANA "Enable Cabana adapter support" OFF)

# Default to ON if top-level project
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(MICROVTK_BUILD_TESTS ON CACHE BOOL "Build unit tests" FORCE)
    set(MICROVTK_BUILD_EXAMPLES ON CACHE BOOL "Build examples" FORCE)
    set(MICROVTK_BUILD_BENCHMARKS ON CACHE BOOL "Build benchmarks" FORCE)
endif()

# Dependencies
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    # -------------------------------------------------------------------------
    # Development / Standalone Build: Use Bundled Dependencies (Submodules)
    # -------------------------------------------------------------------------
    message(STATUS "MicroVTK: Building in standalone mode, using bundled dependencies.")

    # ZLIB
    if(MICROVTK_USE_ZLIB)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/zlib/CMakeLists.txt")
            set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
            set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
            # Force static build for dependencies to avoid DLL issues
            set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
            set(ZLIB_BUILD_SHARED OFF CACHE BOOL "Build shared zlib" FORCE)
            set(ZLIB_BUILD_STATIC ON CACHE BOOL "Build static zlib" FORCE)
            add_subdirectory(external/zlib)
            if(NOT TARGET ZLIB::ZLIB)
                if(TARGET zlibstatic)
                     add_library(ZLIB::ZLIB ALIAS zlibstatic)
                elseif(TARGET zlib)
                     add_library(ZLIB::ZLIB ALIAS zlib)
                endif()
            endif()
            # zlib typically adds its source dir to include path for itself,
            # but we need to ensure interface includes if it doesn't propagate them nicely in older versions.
            if(TARGET zlib)
                target_include_directories(zlib INTERFACE
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/zlib>
                )
            elseif(TARGET zlibstatic)
                target_include_directories(zlibstatic INTERFACE
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/zlib>
                )
            endif()
        else()
            message(FATAL_ERROR "ZLIB enabled but external/zlib not found. Did you run 'git submodule update --init'?")
        endif()
    endif()

    # LZ4
    if(MICROVTK_USE_LZ4)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/lz4/build/cmake/CMakeLists.txt")
            set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
            set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)
            add_subdirectory(external/lz4/build/cmake)
            # LZ4 cmake usually defines target 'lz4_static' or 'lz4_shared' depending on options.
            # It creates an alias 'lz4::lz4' usually.
        else()
            message(FATAL_ERROR "LZ4 enabled but external/lz4 not found. Did you run 'git submodule update --init'?")
        endif()
    endif()

    # Kokkos
    if(MICROVTK_USE_KOKKOS)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/kokkos/CMakeLists.txt")
            set(Kokkos_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
            set(Kokkos_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
            set(Kokkos_ENABLE_IMPL_VIEW_LEGACY ON CACHE BOOL "" FORCE) # Required for Cabana
            add_subdirectory(external/kokkos)
        else()
            message(FATAL_ERROR "Kokkos enabled but external/kokkos not found. Did you run 'git submodule update --init'?")
        endif()
    endif()

    # Cabana
    if(MICROVTK_USE_CABANA)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/cabana/CMakeLists.txt")
            set(Cabana_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
            set(Cabana_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)

            # Allow Cabana to find our internal Kokkos target via cmake/FindKokkos.cmake
            list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

            add_subdirectory(external/cabana)
        else()
             message(FATAL_ERROR "Cabana enabled but external/cabana not found. Did you run 'git submodule update --init'?")
        endif()
    endif()

else()
    # -------------------------------------------------------------------------
    # Library Usage Mode: Use System/User Provided Dependencies
    # -------------------------------------------------------------------------
    if(MICROVTK_USE_ZLIB)
        find_package(ZLIB REQUIRED)
    endif()

    if(MICROVTK_USE_LZ4)
        # Try finding LZ4 via Config mode first, then PkgConfig
        find_package(lz4 CONFIG)
        if(NOT lz4_FOUND)
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(LZ4 REQUIRED liblz4)
        endif()
    endif()

    if(MICROVTK_USE_KOKKOS)
        find_package(Kokkos REQUIRED)
    endif()

    if(MICROVTK_USE_CABANA)
        find_package(Cabana REQUIRED)
    endif()
endif()

# Main Library (Header-Only)
add_library(microvtk INTERFACE)
add_library(microvtk::microvtk ALIAS microvtk)

target_include_directories(microvtk INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(MICROVTK_USE_ZLIB)
    target_compile_definitions(microvtk INTERFACE MICROVTK_HAS_ZLIB)
    target_link_libraries(microvtk INTERFACE ZLIB::ZLIB)
endif()

if(MICROVTK_USE_LZ4)
    target_compile_definitions(microvtk INTERFACE MICROVTK_HAS_LZ4)
    if(TARGET lz4::lz4)
         target_link_libraries(microvtk INTERFACE lz4::lz4)
    elseif(TARGET lz4_static)
         target_link_libraries(microvtk INTERFACE lz4_static)
    elseif(TARGET PkgConfig::LZ4)
         target_link_libraries(microvtk INTERFACE PkgConfig::LZ4)
    else()
         # Fallback if PkgConfig just gave variables
         target_include_directories(microvtk INTERFACE ${LZ4_INCLUDE_DIRS})
         target_link_libraries(microvtk INTERFACE ${LZ4_LIBRARIES})
    endif()
endif()

if(MICROVTK_USE_KOKKOS)
    target_compile_definitions(microvtk INTERFACE MICROVTK_HAS_KOKKOS)
    target_link_libraries(microvtk INTERFACE Kokkos::kokkos)
endif()

if(MICROVTK_USE_CABANA)
    target_compile_definitions(microvtk INTERFACE MICROVTK_HAS_CABANA)
    target_link_libraries(microvtk INTERFACE Cabana::Core)
endif()



# Helper for strict warnings (only applied to internal targets)
function(microvtk_set_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic)
    endif()
endfunction()

# Testing
if(MICROVTK_BUILD_TESTS)
    enable_testing()

    # Check if GTest is already available (e.g. from parent)
    if(NOT TARGET GTest::gtest)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/googletest/CMakeLists.txt")
            add_subdirectory(external/googletest)
        else()
            message(WARNING "GTest submodule not found. Skipping tests.")
            set(MICROVTK_BUILD_TESTS OFF)
        endif()
    endif()

    if(MICROVTK_BUILD_TESTS)
        add_executable(unit_tests
            tests/test_main.cpp
            tests/test_types.cpp
            tests/test_xml_utils.cpp
            tests/test_binary_utils.cpp
            tests/test_adapter.cpp
            tests/test_vtu_writer.cpp
            tests/test_pvd_writer.cpp
            tests/test_vtu_validation.cpp
            tests/test_coverage_gap.cpp
            tests/test_compression.cpp
            tests/test_adapter_kokkos_cabana.cpp
        )
        microvtk_set_strict_warnings(unit_tests)
        target_link_libraries(unit_tests PRIVATE microvtk GTest::gtest GTest::gtest_main)

        include(GoogleTest)
        gtest_discover_tests(unit_tests)

        # Integration Tests (Python + VTK)
        find_program(UV_EXECUTABLE uv)
        if(UV_EXECUTABLE)
            add_test(NAME Integration.Python
                COMMAND ${UV_EXECUTABLE} run pytest
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            )
            message(STATUS "MicroVTK: Python integration tests enabled (using uv at ${UV_EXECUTABLE})")
        else()
            message(WARNING "MicroVTK: 'uv' not found. Integration tests skipped.")
        endif()
    endif()
endif()

# Examples
if(MICROVTK_BUILD_EXAMPLES)
    add_executable(example_basic examples/basic_usage.cpp)
    target_link_libraries(example_basic PRIVATE microvtk)
    microvtk_set_strict_warnings(example_basic)

    add_executable(example_time_series examples/time_series.cpp)
    target_link_libraries(example_time_series PRIVATE microvtk)
    microvtk_set_strict_warnings(example_time_series)

    add_executable(example_complex_grid examples/complex_grid.cpp)
    target_link_libraries(example_complex_grid PRIVATE microvtk)
    microvtk_set_strict_warnings(example_complex_grid)

    add_executable(example_compression examples/compression.cpp)
    target_link_libraries(example_compression PRIVATE microvtk)
    microvtk_set_strict_warnings(example_compression)

    if(MICROVTK_USE_KOKKOS)
        add_executable(example_hpc examples/hpc_adapters.cpp)
        target_link_libraries(example_hpc PRIVATE microvtk)
        microvtk_set_strict_warnings(example_hpc)
    endif()
endif()

# Benchmarks
if(MICROVTK_BUILD_BENCHMARKS)
    if(NOT TARGET benchmark::benchmark)
        set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/benchmark/CMakeLists.txt")
            add_subdirectory(external/benchmark)
        else()
            message(WARNING "Benchmark submodule not found. Skipping benchmarks.")
            set(MICROVTK_BUILD_BENCHMARKS OFF)
        endif()
    endif()

    if(MICROVTK_BUILD_BENCHMARKS)
        add_executable(microvtk_bench benchmarks/benchmark_main.cpp)
        target_link_libraries(microvtk_bench PRIVATE microvtk benchmark::benchmark)
        microvtk_set_strict_warnings(microvtk_bench)

        add_executable(bench_formatting benchmarks/bench_formatting.cpp)
        target_link_libraries(bench_formatting PRIVATE benchmark::benchmark)
        microvtk_set_strict_warnings(bench_formatting)

        add_executable(bench_adapters benchmarks/bench_adapters.cpp)
        target_link_libraries(bench_adapters PRIVATE microvtk benchmark::benchmark)
        microvtk_set_strict_warnings(bench_adapters)
    endif()
endif()

# IDE/LSP Support: Force compilation of headers to generate compile_commands.json entries
# This target is always defined to ensure compile_commands.json includes headers
# whenever CMAKE_EXPORT_COMPILE_COMMANDS is ON (which it is in our presets).
file(GLOB_RECURSE MICROVTK_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")
# Force CMake to treat headers as C++ source files for compilation database generation
set_source_files_properties(${MICROVTK_HEADERS} PROPERTIES LANGUAGE CXX)

# Using OBJECT library prevents actual linking/archiving overhead,
# but still runs the compiler front-end which generates the commands.
add_library(microvtk_ide_headers OBJECT ${MICROVTK_HEADERS})
target_link_libraries(microvtk_ide_headers PRIVATE microvtk)
set_target_properties(microvtk_ide_headers PROPERTIES LINKER_LANGUAGE CXX)
# Ensure it doesn't get built by default 'all' target
set_target_properties(microvtk_ide_headers PROPERTIES EXCLUDE_FROM_ALL ON)
