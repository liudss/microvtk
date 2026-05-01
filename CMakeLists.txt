cmake_minimum_required(VERSION 3.25)
project(microvtk VERSION 0.1.0 LANGUAGES CXX)

# Standard C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ensure all targets are built with -fPIC.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(MICROVTK_IS_TOP_LEVEL ON)
else()
    set(MICROVTK_IS_TOP_LEVEL OFF)
endif()

if(MICROVTK_IS_TOP_LEVEL AND CMAKE_EXPORT_COMPILE_COMMANDS AND NOT WIN32)
    cmake_path(NORMAL_PATH CMAKE_BINARY_DIR OUTPUT_VARIABLE MICROVTK_BINARY_DIR)
    cmake_path(NORMAL_PATH CMAKE_SOURCE_DIR OUTPUT_VARIABLE MICROVTK_SOURCE_DIR)

    set(MICROVTK_DEBUG_BINARY_DIR "${MICROVTK_SOURCE_DIR}/build/debug")
    set(MICROVTK_COMPILE_COMMANDS_LINK "${MICROVTK_SOURCE_DIR}/build/compile_commands.json")

    if(MICROVTK_BINARY_DIR STREQUAL MICROVTK_DEBUG_BINARY_DIR)
        if(IS_SYMLINK "${MICROVTK_COMPILE_COMMANDS_LINK}")
            file(REMOVE "${MICROVTK_COMPILE_COMMANDS_LINK}")
        endif()

        if(NOT EXISTS "${MICROVTK_COMPILE_COMMANDS_LINK}")
            file(CREATE_LINK
                "debug/compile_commands.json"
                "${MICROVTK_COMPILE_COMMANDS_LINK}"
                RESULT MICROVTK_COMPILE_COMMANDS_LINK_RESULT
                SYMBOLIC
            )
            if(NOT MICROVTK_COMPILE_COMMANDS_LINK_RESULT STREQUAL "0")
                message(WARNING
                    "MicroVTK: Failed to create build/compile_commands.json symlink: "
                    "${MICROVTK_COMPILE_COMMANDS_LINK_RESULT}"
                )
            endif()
        endif()
    endif()
endif()

# Options
option(MICROVTK_BUILD_TESTS "Build unit tests" ${MICROVTK_IS_TOP_LEVEL})
option(MICROVTK_BUILD_EXAMPLES "Build examples" ${MICROVTK_IS_TOP_LEVEL})
option(MICROVTK_BUILD_BENCHMARKS "Build benchmarks" ${MICROVTK_IS_TOP_LEVEL})
option(MICROVTK_USE_ZLIB "Enable ZLib compression support" ON)
option(MICROVTK_USE_LZ4 "Enable LZ4 compression support" ON)
option(MICROVTK_USE_KOKKOS "Enable Kokkos adapter support" OFF)
option(MICROVTK_USE_CABANA "Enable Cabana adapter support" OFF)

set(MICROVTK_USE_CPM_DEFAULT ${MICROVTK_IS_TOP_LEVEL})
option(MICROVTK_USE_CPM "Allow CPM.cmake fallback for dependencies that find_package cannot resolve" ${MICROVTK_USE_CPM_DEFAULT})

# Dependencies
function(microvtk_include_cpm)
    if(COMMAND CPMAddPackage)
        return()
    endif()

    set(CPM_DOWNLOAD_VERSION 0.40.8)
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

    if(NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
        message(STATUS "MicroVTK: Downloading CPM.cmake ${CPM_DOWNLOAD_VERSION}")
        file(DOWNLOAD
            "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
            "${CPM_DOWNLOAD_LOCATION}"
            STATUS CPM_DOWNLOAD_STATUS
            TLS_VERIFY ON
        )
        list(GET CPM_DOWNLOAD_STATUS 0 CPM_DOWNLOAD_RESULT)
        if(NOT CPM_DOWNLOAD_RESULT EQUAL 0)
            list(GET CPM_DOWNLOAD_STATUS 1 CPM_DOWNLOAD_ERROR)
            message(FATAL_ERROR "MicroVTK: Failed to download CPM.cmake: ${CPM_DOWNLOAD_ERROR}")
        endif()
    endif()

    include("${CPM_DOWNLOAD_LOCATION}")
endfunction()

function(microvtk_require_dependency package_name)
    if(NOT MICROVTK_USE_CPM)
        message(FATAL_ERROR
            "MicroVTK: ${package_name} was not found. Install it or configure with "
            "-DMICROVTK_USE_CPM=ON to allow the development fallback."
        )
    endif()
endfunction()

function(microvtk_resolve_zlib)
    if(TARGET ZLIB::ZLIB)
        return()
    endif()

    find_package(ZLIB QUIET)
    if(TARGET ZLIB::ZLIB)
        return()
    endif()

    microvtk_require_dependency(ZLIB)
    microvtk_include_cpm()
    message(STATUS "MicroVTK: ZLIB not found; using CPM.cmake fallback.")
    CPMAddPackage(
        NAME zlib
        GITHUB_REPOSITORY madler/zlib
        GIT_TAG v1.3.1
        OPTIONS
            "ZLIB_BUILD_EXAMPLES OFF"
            "ZLIB_BUILD_TESTING OFF"
            "ZLIB_BUILD_SHARED OFF"
            "ZLIB_BUILD_STATIC ON"
    )

    if(NOT TARGET ZLIB::ZLIB)
        if(TARGET zlibstatic)
            add_library(ZLIB::ZLIB ALIAS zlibstatic)
        elseif(TARGET zlib)
            add_library(ZLIB::ZLIB ALIAS zlib)
        endif()
    endif()
endfunction()

function(microvtk_resolve_lz4)
    if(TARGET lz4::lz4 OR TARGET lz4_static OR TARGET PkgConfig::LZ4)
        return()
    endif()

    find_package(lz4 CONFIG QUIET)
    if(TARGET lz4::lz4 OR TARGET lz4_static)
        return()
    endif()

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LZ4 QUIET IMPORTED_TARGET liblz4)
        if(TARGET PkgConfig::LZ4)
            return()
        endif()
    endif()

    microvtk_require_dependency(LZ4)
    microvtk_include_cpm()
    message(STATUS "MicroVTK: LZ4 not found; using CPM.cmake fallback.")
    CPMAddPackage(
        NAME lz4
        GITHUB_REPOSITORY lz4/lz4
        GIT_TAG v1.10.0
        SOURCE_SUBDIR build/cmake
        OPTIONS
            "LZ4_BUILD_CLI OFF"
            "LZ4_BUILD_LEGACY_LZ4C OFF"
    )
endfunction()

function(microvtk_resolve_kokkos)
    if(TARGET Kokkos::kokkos)
        return()
    endif()

    find_package(Kokkos QUIET)
    if(TARGET Kokkos::kokkos)
        return()
    endif()

    microvtk_require_dependency(Kokkos)
    microvtk_include_cpm()
    message(STATUS "MicroVTK: Kokkos not found; using CPM.cmake fallback.")
    CPMAddPackage(
        NAME kokkos
        GITHUB_REPOSITORY kokkos/kokkos
        GIT_TAG 4.4.01
        OPTIONS
            "Kokkos_ENABLE_TESTS OFF"
            "Kokkos_ENABLE_EXAMPLES OFF"
            "Kokkos_ENABLE_IMPL_VIEW_LEGACY ON"
    )
endfunction()

function(microvtk_resolve_cabana)
    if(TARGET Cabana::cabanacore)
        return()
    endif()

    find_package(Cabana QUIET)
    if(TARGET Cabana::cabanacore)
        return()
    endif()

    microvtk_require_dependency(Cabana)
    microvtk_include_cpm()
    message(STATUS "MicroVTK: Cabana not found; using CPM.cmake fallback.")
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    CPMAddPackage(
        NAME cabana
        GITHUB_REPOSITORY ECP-copa/Cabana
        GIT_TAG 0.6.0
        OPTIONS
            "Cabana_ENABLE_TESTING OFF"
            "Cabana_ENABLE_EXAMPLES OFF"
    )
endfunction()

if(MICROVTK_USE_ZLIB)
    microvtk_resolve_zlib()
endif()

if(MICROVTK_USE_LZ4)
    microvtk_resolve_lz4()
endif()

if(MICROVTK_USE_KOKKOS)
    microvtk_resolve_kokkos()
endif()

if(MICROVTK_USE_CABANA)
    microvtk_resolve_cabana()
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
    target_link_libraries(microvtk INTERFACE Cabana::cabanacore)
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

    if(NOT TARGET GTest::gtest)
        find_package(GTest QUIET)
        if(NOT TARGET GTest::gtest AND MICROVTK_USE_CPM)
            microvtk_include_cpm()
            message(STATUS "MicroVTK: GTest not found; using CPM.cmake fallback.")
            CPMAddPackage(
                NAME googletest
                GITHUB_REPOSITORY google/googletest
                GIT_TAG v1.15.2
                OPTIONS
                    "INSTALL_GTEST OFF"
            )
        elseif(NOT TARGET GTest::gtest)
            message(WARNING "GTest not found and CPM fallback is disabled. Skipping tests.")
            set(MICROVTK_BUILD_TESTS OFF)
        endif()
    endif()

    if(TARGET GTest::gtest AND NOT TARGET GTest::gtest_main)
        if(TARGET GTest::Main)
            add_library(GTest::gtest_main ALIAS GTest::Main)
        else()
            message(WARNING "GTest main target not found. Skipping tests.")
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
            tests/test_vti_writer.cpp
            tests/test_pvd_writer.cpp
            tests/test_vtu_validation.cpp
            tests/test_coverage_gap.cpp
            tests/test_compression.cpp
            tests/test_adapter_kokkos_cabana.cpp
            tests/test_indexing_adapter.cpp
        )
        microvtk_set_strict_warnings(unit_tests)
        target_link_libraries(unit_tests PRIVATE microvtk GTest::gtest GTest::gtest_main)

        include(GoogleTest)
        gtest_discover_tests(unit_tests
            DISCOVERY_MODE PRE_TEST
            DISCOVERY_TIMEOUT 60
        )

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

    add_executable(example_vti_adapt examples/vti_adapt.cpp)
    target_link_libraries(example_vti_adapt PRIVATE microvtk)
    microvtk_set_strict_warnings(example_vti_adapt)

    add_executable(example_indexing examples/indexing.cpp)
    target_link_libraries(example_indexing PRIVATE microvtk)
    microvtk_set_strict_warnings(example_indexing)

    if(MICROVTK_USE_KOKKOS)
        add_executable(example_hpc examples/hpc_adapters.cpp)
        target_link_libraries(example_hpc PRIVATE microvtk)
        microvtk_set_strict_warnings(example_hpc)
    endif()
endif()

# Benchmarks
if(MICROVTK_BUILD_BENCHMARKS)
    if(NOT TARGET benchmark::benchmark)
        find_package(benchmark CONFIG QUIET)
        if(NOT TARGET benchmark::benchmark AND MICROVTK_USE_CPM)
            microvtk_include_cpm()
            message(STATUS "MicroVTK: benchmark not found; using CPM.cmake fallback.")
            CPMAddPackage(
                NAME benchmark
                GITHUB_REPOSITORY google/benchmark
                GIT_TAG v1.9.0
                OPTIONS
                    "BENCHMARK_ENABLE_TESTING OFF"
                    "BENCHMARK_ENABLE_INSTALL OFF"
            )
        elseif(NOT TARGET benchmark::benchmark)
            message(WARNING "benchmark not found and CPM fallback is disabled. Skipping benchmarks.")
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

        add_executable(bench_vti benchmarks/bench_vti.cpp)
        target_link_libraries(bench_vti PRIVATE microvtk benchmark::benchmark)
        microvtk_set_strict_warnings(bench_vti)

        # Enable architecture-specific optimizations (BMI2) for benchmarks
        if(NOT MSVC)
            target_compile_options(bench_adapters PRIVATE -march=native)
        endif()
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
