# MicroVTK

![Language](https://img.shields.io/badge/language-C%2B%2B20-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
[![CI](https://github.com/liudss/microvtk/actions/workflows/ci.yml/badge.svg)](https://github.com/liudss/microvtk/actions/workflows/ci.yml)

MicroVTK is a header-only C++20 library developed for high-performance computing (HPC) and scientific visualization applications. It facilitates the efficient generation of VTK XML artifacts (`.vtu`, `.vti`, `.pvd`) through a strictly zero-copy streaming architecture. The library adheres to modern C++ standards to ensure type safety, minimal memory footprint, and seamless interoperability with contemporary simulation frameworks.

## Core Capabilities

*   **Zero-Copy Architecture**
    Implements a streaming I/O model that serializes data directly from application memory to disk, eliminating redundant data replication and intermediate buffering.
    > **⚠️ Lifetime Note:** The writer stores *views* to the provided data. The application must ensure the underlying data containers remain valid until the `.write()` method is called.

*   **Format Support**
    Provides comprehensive support for Unstructured Grids (`.vtu`) for finite element/volume methods and Structured Image Data (`.vti`) for regular grids.

*   **HPC Interoperability**
    Includes specialized adapters for the Kokkos performance portability ecosystem and Cabana particle toolkit. It automatically handles arbitrary memory layouts (e.g., LayoutLeft, LayoutStride) and complex data structures (AoSoA).

*   **Advanced Indexing**
    Supports non-linear memory layouts, specifically Morton Codes (Z-Order curves) in two and three dimensions. It utilizes BMI2 hardware instructions (`PDEP`) for accelerated coordinate encoding when available.

*   **Compression**
    Integrates transparent support for ZLIB and LZ4 compression algorithms to mitigate I/O bandwidth limitations.

*   **Reliability & Standards**
    The core library maintains zero external dependencies. The codebase is rigorously validated via GoogleTest and verified for memory safety using Valgrind.

---

## Integration

MicroVTK is distributed as a header-only library. Integration via CMake `add_subdirectory` is recommended.

### 1. Submodule Configuration
```bash
git submodule add https://github.com/liudss/microvtk.git external/microvtk
```

### 2. CMake Integration
```cmake
# Optional: Enable HPC extensions
set(MICROVTK_USE_KOKKOS ON)
set(MICROVTK_USE_CABANA ON)
set(MICROVTK_USE_LZ4 ON)

add_subdirectory(external/microvtk)

add_executable(simulation_solver main.cpp)
target_link_libraries(simulation_solver PRIVATE microvtk::microvtk)
```

---

## Usage Examples

### 1. Unstructured Grid Generation (.vtu)
Serialization of standard STL containers into an unstructured grid format.

```cpp
#include <microvtk/microvtk.hpp>
#include <vector>

int main() {
    microvtk::VtuWriter writer;

    // 1. Geometry Definition (N points, 3 components)
    std::vector<double> points = {0,0,0, 1,0,0, 0,1,0, 0,0,1};
    writer.setPoints(points);

    // 2. Topology Definition (Single Tetrahedron)
    std::vector<int32_t> conn = {0, 1, 2, 3};
    std::vector<int32_t> offsets = {4};
    std::vector<uint8_t> types = {static_cast<uint8_t>(microvtk::CellType::Tetra)};
    writer.setCells(conn, offsets, types);

    // 3. Attribute Association
    std::vector<double> pressure = {101.3, 102.0, 101.5, 100.8};
    writer.addPointData("Pressure", pressure);

    writer.write("mesh.vtu");
}
```

### 2. Structured Grid with AoS Adaptation (.vti)
Direct serialization from an Array-of-Structs (AoS) memory layout using the `adapt` utility.

```cpp
#include <microvtk/microvtk.hpp>
#include <vector>

struct Pixel { double value; int id; };

int main() {
    // 10x10x1 uniform grid
    std::array<int, 6> extent = {0, 9, 0, 9, 0, 0};
    microvtk::VtiWriter writer(extent);

    std::vector<Pixel> data = ...; // 100 pixels

    // Extract members directly from AoS without data copy
    writer.addPointData("Intensity", microvtk::adapt(data, &Pixel::value));
    writer.addPointData("ID", microvtk::adapt(data, &Pixel::id));

    writer.write("image.vti");
}
```

### 3. HPC Integration (Kokkos & Cabana)
Automatic mapping of logical indices to VTK-compatible ordering for complex memory layouts.

```cpp
// Kokkos: Contiguous Memory Mapping (Fast Path)
Kokkos::View<double*[3], Kokkos::LayoutRight, Kokkos::HostSpace> coords("coords", N);
writer.setPoints(microvtk::adapt(coords));

// Cabana: Particle Data Management
using DataTypes = Cabana::MemberTypes<double[3], double[3][3]>;
Cabana::AoSoA<DataTypes, Kokkos::HostSpace> particles("particles", N);

auto velocity_slice = Cabana::slice<0>(particles);
auto stress_slice = Cabana::slice<1>(particles);

// Serialize vector (3 components) and tensor (9 components) fields
writer.addPointData("Velocity", microvtk::adapt(velocity_slice), 3);
writer.addPointData("Stress", microvtk::adapt(stress_slice), 9);
```

### 4. Morton (Z-Order) Curve Support
On-the-fly reordering of spatially sorted data (Morton code) to standard Raster order.

```cpp
#include <microvtk/indexing_adapter.hpp>

// Data stored in Z-order curve layout
std::vector<double> morton_data = ...;
std::array<size_t, 3> dims = {128, 128, 128};

// 1. Initialize Reordering View (Zero-Copy)
// Automatically infers 2D/3D dimensionality and enables BMI2 acceleration if available
auto view = morton_data | microvtk::views::reorder_z_curve(dims);

// 2. Serialize
writer.addPointData("Temperature", view);
```

### 5. Transient Data Management (.pvd)
Management of time-series data collections.

```cpp
microvtk::PvdWriter pvd("simulation.pvd");

for (int step = 0; step < 100; ++step) {
    std::string filename = std::format("step_{:04d}.vtu", step);

    // ... Generate frame ...

    // Register artifact with timestamp
    pvd.addStep(step * 0.01, filename);
    pvd.save(); // Synchronize file state
}
```

---

## Build & Verification

### System Requirements
*   **Compiler**: GCC 13+, Clang 16+, or MSVC 19.34+ (C++20 compliant)
*   **Build System**: CMake 3.25+ (Ninja generator recommended)

### Compilation
```bash
git clone --recursive https://github.com/liudss/microvtk.git
cd microvtk

# Configure
cmake -S . -B build -G Ninja \
    -D MICROVTK_BUILD_TESTS=ON \
    -D MICROVTK_BUILD_EXAMPLES=ON

# Build
cmake --build build
```

### Testing
The project implements a two-tier testing strategy: C++ Unit Tests (GoogleTest) and Python Integration Tests (PyTest + VTK).

**Unit Tests:**
```bash
./build/unit_tests
```

**Integration Tests (with Memory Sanitization):**
Integration tests can be executed under Valgrind supervision to ensure memory safety.

```bash
# Initialize dependency manager
curl -LsSf https://astral.sh/uv/install.sh | sh

# Execute tests with strict memory checking
export MICROVTK_USE_VALGRIND=ON
uv run pytest tests/integration
```

---

## Project Organization

```text
microvtk/
├── include/microvtk/       # Core library headers
│   ├── common/             # Concepts, Types, Enums
│   ├── core/               # Streaming logic, Compression, XML Builders
│   ├── vtu_writer.hpp      # UnstructuredGrid Writer
│   ├── vti_writer.hpp      # ImageData Writer
│   ├── pvd_writer.hpp      # Time Series Writer
│   └── *_adapter.hpp       # Data Adapters (Standard, HPC, Indexing)
├── examples/               # Implementation examples
├── tests/                  # Verification suite
└── external/               # Third-party dependencies
```

## License

This project is licensed under the MIT License. Please refer to the [LICENSE](LICENSE) file for full terms and conditions.
