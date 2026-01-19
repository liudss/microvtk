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

# Default to ON if top-level project
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(MICROVTK_BUILD_TESTS ON CACHE BOOL "Build unit tests" FORCE)
    set(MICROVTK_BUILD_EXAMPLES ON CACHE BOOL "Build examples" FORCE)
    set(MICROVTK_BUILD_BENCHMARKS ON CACHE BOOL "Build benchmarks" FORCE)
endif()

# Main Library (Header-Only)
add_library(microvtk INTERFACE)
add_library(microvtk::microvtk ALIAS microvtk)

target_include_directories(microvtk INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

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
