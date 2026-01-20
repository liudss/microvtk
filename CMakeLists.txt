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
        )
        microvtk_set_strict_warnings(unit_tests)
        target_link_libraries(unit_tests PRIVATE microvtk GTest::gtest GTest::gtest_main)

        include(GoogleTest)
        gtest_discover_tests(unit_tests)
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
    endif()
endif()
