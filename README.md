# MicroVTK

A lightweight, high-performance, header-only C++20 library for writing VTK XML files (`.vtu`, `.pvd`) with zero-copy architecture.

## Key Features

- **Modern C++20**: Utilizes concepts, spans, and ranges for a clean and efficient API.
- **Zero-Copy**: Leverages `std::span` and custom adapters to write user data structures directly to disk without intermediate copies.
- **High Performance**: Optimized for "Appended Binary" format, providing the fastest write speeds and smallest file sizes.
- **Header-Only**: Easy to integrate into existing projects. Just include and go.
- **Zero-Dependency Core**: The core library has no external dependencies (no XML or compression libs required for basic usage).
- **Time-Series Support**: Built-in support for generating `.pvd` files to manage simulation time steps.

## Requirements

- **Compiler**: GCC 13+, Clang 16+, or MSVC 19.30+ (supporting C++20).
- **Build System**: CMake 3.25+.
- **Testing**: GoogleTest (included via Git submodules).

## Installation & Integration

### As a Submodule

You can add MicroVTK as a submodule to your project:

```bash
git submodule add https://github.com/your-repo/microvtk.git external/microvtk
```

Then, in your `CMakeLists.txt`:

```cmake
add_subdirectory(external/microvtk)
target_link_libraries(your_target PRIVATE microvtk)
```

### Manual Inclusion

Since it is header-only, you can simply copy the `include/microvtk` directory to your project's include path.

## Quick Start

```cpp
#include <microvtk/microvtk.hpp>
#include <vector>
#include <array>

using namespace microvtk;

int main() {
    // 1. Setup Writer
    VtuWriter writer;

    // 2. Define Geometry (Zero Copy)
    std::vector<double> points = {0.0, 0.0, 0.0,  1.0, 0.0, 0.0,  0.0, 1.0, 0.0};
    writer.setPoints(points);

    // 3. Define Topology
    std::vector<int32_t> conn = {0, 1, 2};
    std::vector<int32_t> offsets = {3};
    std::vector<uint8_t> types = {5}; // VTK_TRIANGLE
    writer.setCells(conn, offsets, types);

    // 4. Add Attribute Data
    std::vector<float> temperature = {100.0f, 50.0f, 75.0f};
    writer.addPointData("Temperature", temperature);

    // 5. Write to disk
    writer.write("output.vtu");

    return 0;
}
```

## Advanced Usage: Adapters

MicroVTK can adapt custom structures directly:

```cpp
struct Particle {
    double mass;
    std::array<double, 3> velocity;
};

std::vector<Particle> particles = ...;
writer.addPointData("Mass", vtk::adapt(particles, &Particle::mass));
writer.addPointData("Velocity", vtk::adapt(particles, &Particle::velocity), 3);
```

## Development

### Building Tests

```bash
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

### Code Style

The project follows the Google C++ Style Guide. Use `clang-format` and `clang-tidy` to maintain code quality:

```bash
# Format code
Get-ChildItem -Include *.hpp,*.cpp -Recurse | ForEach-Object { clang-format -i $_.FullName }

# Run linting
clang-tidy -p build tests/test_vtu_writer.cpp
```

## License

MIT License
