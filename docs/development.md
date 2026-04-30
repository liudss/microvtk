# MicroVTK Development Guide

This guide documents the local workflow for building, testing, and contributing
to MicroVTK.

For module boundaries, data flow, and extension points, see the
[Architecture](architecture.md) document.

## Repository Layout

```text
include/microvtk/          Public header-only library
include/microvtk/core/     XML, appended-data, compression, and binary helpers
examples/                  Small executable examples
tests/                     GoogleTest unit tests
tests/integration/         pytest tests that read outputs with VTK Python
benchmarks/                google/benchmark executables
cmake/                     Local CMake find modules
external/                  Vendored or checkout-provided third-party sources
```

## Prerequisites

Required:

| Tool | Minimum |
| --- | --- |
| CMake | 3.25 |
| C++ compiler | C++20 support, such as GCC 13+, Clang 16+, or MSVC 19.34+ |
| Ninja | Recommended generator |

Optional:

| Tool or library | Used for |
| --- | --- |
| `uv` | Python integration tests |
| VTK Python bindings | Integration test readers |
| Valgrind | Optional memory checks for integration tests |
| ZLIB | ZLIB-compressed VTK output |
| LZ4 | LZ4-compressed VTK output |
| Kokkos | Kokkos adapter examples and tests |
| Cabana | Cabana adapter support |
| google/benchmark | Benchmarks |

## CMake Options

| Option | Default at top level | Purpose |
| --- | --- | --- |
| `MICROVTK_BUILD_TESTS` | `ON` | Build GoogleTest unit tests and register integration tests when `uv` is available. |
| `MICROVTK_BUILD_EXAMPLES` | `ON` | Build example executables. |
| `MICROVTK_BUILD_BENCHMARKS` | `ON` | Build benchmark executables. |
| `MICROVTK_USE_ZLIB` | `ON` | Enable ZLIB compression support. |
| `MICROVTK_USE_LZ4` | `ON` | Enable LZ4 compression support. |
| `MICROVTK_USE_KOKKOS` | `OFF` | Enable Kokkos adapter support. |
| `MICROVTK_USE_CABANA` | `OFF` | Enable Cabana adapter support. |
| `MICROVTK_USE_CPM` | `ON` at top level | Allow CPM.cmake fallback when `find_package` cannot resolve dependencies. |

When MicroVTK is included as a subdirectory, tests, examples, benchmarks, and CPM
fallback default to `OFF` unless explicitly enabled by the parent project.

## Configure and Build

Use the provided CMake presets for normal development:

```bash
cmake --preset debug
cmake --build --preset debug
```

Useful alternatives:

```bash
cmake --preset release
cmake --build --preset release

cmake --preset coverage
cmake --build --preset coverage

cmake --preset clang-tidy
cmake --build --preset clang-tidy --target microvtk_ide_headers
```

For a minimal consumer-style build:

```bash
cmake -S . -B build/minimal -G Ninja \
  -D MICROVTK_BUILD_TESTS=OFF \
  -D MICROVTK_BUILD_EXAMPLES=OFF \
  -D MICROVTK_BUILD_BENCHMARKS=OFF \
  -D MICROVTK_USE_ZLIB=OFF \
  -D MICROVTK_USE_LZ4=OFF

cmake --build build/minimal
```

## Test

Run all CTest-registered tests from the debug preset:

```bash
ctest --preset all-tests
```

Run only the unit test executable:

```bash
./build/debug/unit_tests
```

Run Python integration tests directly:

```bash
uv run pytest tests/integration
```

Enable Valgrind for integration tests:

```bash
MICROVTK_USE_VALGRIND=ON uv run pytest tests/integration
```

If `uv` is not installed, CMake skips the integration test registration and
still builds the C++ unit tests.

## Examples

After building the debug preset, example executables are available under
`build/debug/`:

```bash
./build/debug/example_basic
./build/debug/example_time_series
./build/debug/example_complex_grid
./build/debug/example_compression
./build/debug/example_vti_adapt
./build/debug/example_indexing
```

The Kokkos example is built only when `MICROVTK_USE_KOKKOS=ON`.

## Benchmarks

Build and run benchmarks from a release configuration:

```bash
cmake --preset release
cmake --build --preset release --target microvtk_bench bench_adapters bench_vti

./build/release/microvtk_bench
./build/release/bench_adapters
./build/release/bench_vti
```

`bench_adapters` uses `-march=native` on GCC and Clang so the benchmark can
measure architecture-specific paths such as BMI2 acceleration.

## Dependency Resolution

MicroVTK first tries existing CMake targets and `find_package`. If a dependency
is missing and `MICROVTK_USE_CPM=ON`, the build downloads the development
fallback through CPM.cmake.

Set `MICROVTK_USE_CPM=OFF` in packaging, offline, or reproducible build
environments. With CPM disabled, missing enabled dependencies are reported as
configuration errors.

## Contribution Workflow

This repository uses short-lived branches targeting `master`.

```bash
git fetch origin
git switch master
git merge --ff-only origin/master
git switch -c docs/my-change
```

Before opening a pull request:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset all-tests
```

Keep documentation changes in `docs/`, README updates, or source comments that
explain non-obvious design constraints. Prefer documenting behavior and tradeoffs
over restating what the code already says.
