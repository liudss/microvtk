# MicroVTK

![Language](https://img.shields.io/badge/language-C%2B%2B20-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
[![CI](https://github.com/liudss/microvtk/actions/workflows/ci.yml/badge.svg)](https://github.com/liudss/microvtk/actions/workflows/ci.yml)

**MicroVTK** is a lightweight, header-only C++20 library designed for high-performance scientific data visualization. It specializes in writing **VTK XML** files (`.vtu`, `.pvd`) with a focus on efficiency, memory safety, and seamless integration with modern HPC ecosystems.

> **Key Philosophy:** "Your data stays where it is. We just stream it to disk."

## 🚀 Key Features

| Feature | Description |
| :--- | :--- |
| **Zero-Copy Streaming** | Data is streamed directly from user memory to disk. No intermediate buffers, no redundant copies. |
| **Modern C++20** | Built with Concepts, Ranges, Spans, and `std::format` for a type-safe and expressive API. |
| **HPC Ready** | Native adapters for **Kokkos Views** (arbitrary Rank/Layout) and **Cabana Slices** (SoA/AoS/Tensor). |
| **Compression** | Transparent support for **ZLIB** and **LZ4** compression to reduce I/O bottlenecks. |
| **Zero Dependencies** | The core library depends *only* on the C++ Standard Library. Optional features use standard submodules. |
| **Memory Safe** | Rigorously tested with **Valgrind** in CI to ensure no leaks or dangling references. |

---

## 📦 Integration

MicroVTK is designed as a **header-only** library. The recommended way to use it is via CMake `add_subdirectory`.

### 1. Add as Submodule
```bash
git submodule add https://github.com/liudss/microvtk.git external/microvtk
```

### 2. Configure CMake
```cmake
# Optional: Enable HPC features
set(MICROVTK_USE_KOKKOS ON)
set(MICROVTK_USE_CABANA ON)
set(MICROVTK_USE_LZ4 ON)

add_subdirectory(external/microvtk)

add_executable(my_simulation main.cpp)
target_link_libraries(my_simulation PRIVATE microvtk::microvtk)
```

---

## 💡 Usage Examples

### 1. Basic Unstructured Grid (.vtu)
Stream standard C++ vectors directly to a VTU file.

```cpp
#include <microvtk/microvtk.hpp>
#include <vector>

int main() {
    microvtk::VtuWriter writer;

    // 1. Geometry (N points, 3 components)
    std::vector<double> points = {0,0,0, 1,0,0, 0,1,0, 0,0,1};
    writer.setPoints(points);

    // 2. Topology (Single Tetrahedron)
    std::vector<int32_t> conn = {0, 1, 2, 3};
    std::vector<int32_t> offsets = {4};
    std::vector<uint8_t> types = {static_cast<uint8_t>(microvtk::CellType::Tetra)};
    writer.setCells(conn, offsets, types);

    // 3. Data Attributes
    std::vector<double> pressure = {101.3, 102.0, 101.5, 100.8};
    writer.addPointData("Pressure", pressure);

    writer.write("mesh.vtu");
}
```

### 2. High-Performance Computing (Kokkos)
Handle complex, multi-dimensional array layouts automatically. MicroVTK maps logical indices to VTK's expected order without data replication.

```cpp
// 1. Contiguous Data (Fast Path)
// Direct memory mapping for standard LayoutRight (C-style) views
Kokkos::View<double*[3], Kokkos::LayoutRight, Kokkos::HostSpace> coords("coords", N);
writer.setPoints(microvtk::adapt(coords));

// 2. Strided / Non-Contiguous Data (Rank-3 Tensor Field)
// Supports LayoutLeft (Fortran-style) and arbitrary Rank
Kokkos::View<double*[3][3], Kokkos::LayoutLeft, Kokkos::HostSpace> stress("stress", N);

// Automatically maps (N, 3, 3) -> (N, 9) flat components
writer.addPointData("StressTensor", microvtk::adapt(stress), 9);
```

### 3. Particle Systems (Cabana)
Seamlessly slice and flatten Array-of-Structs-of-Arrays (AoSoA) structures.

```cpp
using DataTypes = Cabana::MemberTypes<double[3], double[3][3]>;
Cabana::AoSoA<DataTypes, Kokkos::HostSpace> particles("particles", N);

auto velocity_slice = Cabana::slice<0>(particles);
auto stress_slice = Cabana::slice<1>(particles);

// Write velocity (3 components) and stress tensor (9 components)
writer.addPointData("Velocity", microvtk::adapt(velocity_slice), 3);
writer.addPointData("Stress", microvtk::adapt(stress_slice), 9);
```

### 4. Time Series (.pvd)
Manage transient simulations with the PVD writer.

```cpp
microvtk::PvdWriter pvd("simulation.pvd");

for (int step = 0; step < 100; ++step) {
    std::string filename = std::format("step_{:04d}.vtu", step);

    // ... write vtu file ...

    // Register file and timestamp
    pvd.addStep(step * 0.01, filename);
    pvd.save(); // Explicit save updates the file on disk immediately
}
```

---

## 🛠️ Building & Testing

### Requirements
*   **Compiler**: GCC 13+, Clang 16+, MSVC 19.34+ (C++20 compliant)
*   **CMake**: 3.25+
*   **Ninja**: Recommended build system

### Build Instructions
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

### Running Tests
The project includes comprehensive unit tests (GoogleTest) and integration tests (Python + VTK).

**Unit Tests:**
```bash
./build/unit_tests
```

**Integration Tests (with Memory Safety):**
MicroVTK supports running integration tests under **Valgrind** to guarantee memory safety.

```bash
# Install uv for Python dependency management
curl -LsSf https://astral.sh/uv/install.sh | sh

# Run integration tests
# Set environment variable to enable strict Valgrind checking
export MICROVTK_USE_VALGRIND=ON
uv run pytest tests/integration
```

---

## 📂 Project Structure

```text
microvtk/
├── include/microvtk/       # Header-only library source
│   ├── common/             # Concepts, Types, Enums
│   ├── core/               # Streaming logic, Compression, XML Builders
│   ├── vtu_writer.hpp      # UnstructuredGrid Writer
│   ├── pvd_writer.hpp      # Time Series Writer
│   └── *_adapter.hpp       # Data Adapters (std, Kokkos, Cabana)
├── examples/               # Example usages (Basic, HPC, Compression)
├── tests/                  # Unit and Integration tests
└── external/               # Third-party dependencies (Submodules)
```

## 📄 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.
