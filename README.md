# MicroVTK

![Language](https://img.shields.io/badge/language-C%2B%2B20-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)

**MicroVTK** is a lightweight, high-performance, header-only C++20 library designed for writing VTK XML files (`.vtu`, `.pvd`) in scientific simulations.

It features a **true zero-copy streaming architecture**, meaning it streams data directly from your memory structures to disk without intermediate buffering, minimizing memory footprint and maximizing I/O throughput.

## ✨ Key Features

- **Modern C++20**: Built from the ground up using Concepts, Ranges, Spans, and `std::format`.
- **Zero-Copy Streaming**: Data is referenced via type-erased accessors and streamed directly to disk. Ideal for handling massive datasets where memory is scarce.
- **Zero-Dependency Core**: The core library depends only on the C++ Standard Library.
- **Compression Support**: Optional, seamless integration with **ZLIB** and **LZ4** for efficient disk usage.
- **Flexible Adapters**: Built-in support for Array-of-Structures (AoS) and strided data layouts via `vtk::adapt`.
- **Time Series Support**: Native support for `.pvd` files to manage time-dependent simulations.
- **High Performance**: Optimized for the "Appended Binary" VTK format, achieving disk-bound write speeds.

## 📦 Integration

### CMake (Recommended)

MicroVTK is designed to be easily integrated as a CMake submodule.

1.  **Add as a submodule**:
    ```bash
    git submodule add https://github.com/liudss/microvtk.git external/microvtk
    ```

2.  **Update `CMakeLists.txt`**:
    ```cmake
    add_subdirectory(external/microvtk)
    target_link_libraries(your_target PRIVATE microvtk::microvtk)
    ```

### Compression Options

By default, MicroVTK looks for ZLIB and LZ4. You can control this via CMake options before adding the subdirectory:

```cmake
set(MICROVTK_USE_ZLIB ON) # or OFF
set(MICROVTK_USE_LZ4 ON)  # or OFF
add_subdirectory(external/microvtk)
```

## 🚀 Usage Examples

### 1. Basic VTU Writer (Unstructured Grid)

```cpp
#include <microvtk/microvtk.hpp>
#include <vector>

int main() {
    using namespace microvtk;

    VtuWriter writer;

    // 1. Set Geometry (Zero-copy references)
    // Data must remain valid until writer.write() is called
    std::vector<double> points = {
        0.0, 0.0, 0.0,
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0
    };
    writer.setPoints(points);

    // 2. Set Topology
    std::vector<int32_t> conn = {0, 1, 2};
    std::vector<int32_t> offsets = {3};
    std::vector<uint8_t> types = {static_cast<uint8_t>(CellType::Triangle)};
    writer.setCells(conn, offsets, types);

    // 3. Add Data Attributes
    std::vector<double> temperature = {298.15, 300.0, 310.5};
    writer.addPointData("Temperature", temperature);

    // 4. Write to Disk
    // By default, uses Appended Binary mode (Little Endian)
    writer.write("simulation_step.vtu");
}
```

### 2. Handling Array-of-Structures (AoS)

Use `vtk::adapt` to write data directly from your custom structs without manual copying.

```cpp
struct Particle {
    double x, y, z;
    double mass;
    double velocity[3];
};

std::vector<Particle> particles = ...;

// Write 'mass' directly from the struct vector
writer.addPointData("Mass", vtk::adapt(particles, &Particle::mass));

// Write 'velocity' (3 components)
writer.addPointData("Velocity", vtk::adapt(particles, &Particle::velocity));
```

### 3. Time Series (.pvd)

```cpp
PvdWriter pvd("output/simulation.pvd");

for (int step = 0; step < 100; ++step) {
    std::string filename = std::format("step_{}.vtu", step);
    
    // ... write vtu file ...
    
    // Register step in PVD file
    pvd.addStep(step * 0.01, filename);
    
    // Explicit save allows updating the PVD file during the simulation
    pvd.save(); 
}
```

### 4. Enabling Compression

```cpp
// Set compression mode before writing
writer.setCompression(core::CompressionType::LZ4); 
writer.write("compressed_output.vtu");
```

## 🛠 Building the Project

To build the tests and benchmarks included in this repository:

```bash
mkdir build && cd build
cmake -G Ninja -D MICROVTK_BUILD_TESTS=ON -D MICROVTK_BUILD_BENCHMARKS=ON ..
cmake --build .
```

**Run Tests:**
```bash
./unit_tests
```

## 📂 Project Structure

```text
microvtk/
├── include/microvtk/   # Header files
│   ├── common/         # Enums, Concepts
│   ├── core/           # Data Accessors, XML Utils, Binary Utils
│   └── ...             # Writer implementations
├── examples/           # Usage examples
├── tests/              # Unit tests (GoogleTest)
└── benchmarks/         # Performance benchmarks (Google Benchmark)
```

## 📜 License

This project is licensed under the **MIT License**.