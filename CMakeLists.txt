cmake_minimum_required(VERSION 3.25)
project(microvtk VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Standard C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Dependencies (Git Submodule)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(external/googletest)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(external/benchmark)

# Compiler Options
if(MSVC)
    add_compile_options(/W4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Main Library (Header-Only)
add_library(microvtk INTERFACE)
target_include_directories(microvtk INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Testing
enable_testing()

add_executable(unit_tests 
    tests/test_main.cpp
    tests/test_types.cpp
    tests/test_xml_utils.cpp
    tests/test_binary_utils.cpp
    tests/test_adapter.cpp
    tests/test_vtu_writer.cpp
    tests/test_pvd_writer.cpp
    tests/test_vtu_validation.cpp
)

add_executable(example_basic examples/basic_usage.cpp)
target_link_libraries(example_basic PRIVATE microvtk)

add_executable(example_time_series examples/time_series.cpp)
target_link_libraries(example_time_series PRIVATE microvtk)

add_executable(example_complex_grid examples/complex_grid.cpp)
target_link_libraries(example_complex_grid PRIVATE microvtk)

target_link_libraries(unit_tests PRIVATE microvtk GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)

# Benchmarks
add_executable(microvtk_bench benchmarks/benchmark_main.cpp)
target_link_libraries(microvtk_bench PRIVATE microvtk benchmark::benchmark)

add_executable(bench_formatting benchmarks/bench_formatting.cpp)
target_link_libraries(bench_formatting PRIVATE benchmark::benchmark)