# Repository Guidelines

## Project Structure & Module Organization

MicroVTK is a header-only C++20 library with optional Python bindings. Core public headers live in `include/microvtk/`: writer APIs are at the top level, shared utilities are under `common/` and `core/`, and adapter support is in `*_adapter.hpp`. C++ unit tests are in `tests/test_*.cpp`; Python/VTK integration tests are in `tests/integration/test_*.py`. Examples are in `examples/`, benchmarks in `benchmarks/`, CMake helpers in `cmake/`, and nanobind-based Python sources in `python/`.

## Build, Test, and Development Commands

- `cmake --preset debug`: configure a Ninja debug build in `build/debug` with tests, examples, benchmarks, ZLIB, and LZ4 enabled.
- `cmake --build --preset debug`: build the debug preset.
- `ctest --preset all-tests`: run all CTest-registered tests and print failures.
- `./build/debug/unit_tests`: run the GoogleTest C++ unit test binary directly.
- `uv run pytest tests/integration`: run Python integration tests declared in `pyproject.toml`.
- `pre-commit run --all-files`: run whitespace, YAML, large-file, and clang-format hooks.

Use `cmake --preset release` and `cmake --build --preset release` for optimized builds.

## Coding Style & Naming Conventions

C++ code follows `.clang-format` based on Google style: 2-space indentation, 80-column limit, attached braces, sorted includes, left-aligned pointers, and no tabs. Keep public APIs in namespace `microvtk`, prefer descriptive lower_snake_case for local variables and functions when matching existing code, and use existing writer/adaptor naming patterns such as `VtuWriter`, `VtiWriter`, and `adapt(...)`. Python tests use pytest naming: files and functions should start with `test_`.

## Testing Guidelines

Add focused C++ tests near related files in `tests/test_*.cpp` and use GoogleTest assertions. Add integration coverage in `tests/integration/` when validating generated VTK artifacts through Python bindings or VTK readers. Prefer running `ctest --preset all-tests` before submitting; run `uv run pytest tests/integration` when Python bindings or file-format behavior changes.

## Commit & Pull Request Guidelines

Recent history uses conventional-style subjects such as `fix(microvtk): validate VTI data sizes`, `perf(microvtk): reduce compressed write copies`, and `build: fix dependency conflict`. Keep commits imperative, scoped when useful, and focused on one logical change. Pull requests should summarize behavior changes, list commands run, link related issues, and include screenshots or generated artifact notes only when visualization output is relevant.

## Agent-Specific Instructions

Do not modify `external/` vendored dependencies unless the task explicitly requires it. Preserve the header-only design and existing CMake options when adding features.
