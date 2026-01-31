# MicroVTK: High-Performance C++20 VTK Library Specification

## 1. Role & Objective
You are a Senior C++ Systems Engineer specializing in High-Performance Computing (HPC) and Visualization.
**Your Goal:** Implement `MicroVTK`, a lightweight, header-only, modern C++20 library for writing VTK files (`.vtu`, `.pvd`).
**Constraints:**
- Strict C++20 standard (Concepts, Spans, Ranges, `std::format`).
- **Zero-Dependency** for the core library (no external XML or Compression libs).
- **True Zero-Copy** streaming architecture using Type Erasure (`DataAccessor`).
- Optimized for "Appended Binary" format (Raw binary dump) with minimal memory footprint.

---

## 2. Development Environment
* **OS:** Linux / Windows (MSYS2 UCRT64).
* **Build System:** CMake 3.25+ (Generator: Ninja).
* **Compilers:** Must compile cleanly on **GCC 13+** and **Clang 16+**.
* **Dependencies:**
    * **GoogleTest:** Git submodule (testing).
    * **Google Benchmark:** Git submodule (benchmarking).
    * **ZLIB:** Git submodule (optional, for compression).
    * **LZ4:** Git submodule (optional, for compression).
    * **Python (VTK/Pytest):** Managed via **uv** (Integration testing).
* **Tools:** `clang-format` (Google Style), `clang-tidy`, `uv`.
* **Testing:**
    * **C++:** GoogleTest (Unit Tests).
    * **Python:** pytest + official VTK (Integration Tests).

---

## 3. Architecture Overview

### 3.1 Directory Structure
```text
microvtk/
├── benchmarks/             # Google Benchmark suites
├── cmake/                  # CMake helper modules
├── examples/               # Usage examples
├── external/               # Third-party dependencies (GTest, Benchmark)
├── include/
│   └── microvtk/
│       ├── common/
│       │   └── types.hpp         # Enums, Concepts, Traits
│       ├── core/
│       │   ├── binary_utils.hpp  # Endianness, Base64
│       │   ├── compressor.hpp    # Compression wrappers
│       │   ├── data_accessor.hpp # Type-erased data streaming (Zero-copy)
│       │   └── xml_utils.hpp     # std::format based XML builder
│       ├── vtu_writer.hpp    # Main UnstructuredGrid Writer (Streaming)
│       ├── pvd_writer.hpp    # Time Series Writer (Explicit Save)
│       ├── adapter.hpp       # Data Adapters (AoS support)
│       ├── cabana_adapter.hpp # Cabana Adapter
│       ├── kokkos_adapter.hpp # Kokkos Adapter
│       └── microvtk.hpp      # Main entry header
├── tests/
│   ├── integration/        # Python compatibility tests (Official VTK)
│   └── ...                 # Unit tests (GTest)
├── pyproject.toml          # Python dependencies (uv)
├── CMakeLists.txt
└── .clang-format
```

---

## 4. Implementation Status

### Phase 3: HPC Adapters (Completed)
- Native adapters for **Kokkos Views** and **Cabana Slices**.
- **Kokkos**: Automatic dispatch between fast path (`std::span`) and logical indexing path (`std::views::transform`) to support all layouts including `LayoutLeft` and `LayoutStride` for **arbitrary Ranks**.
- **Cabana**: Automatic flattening of AoSoA slices, including support for **multidimensional array (tensor) members**.

### Phase 4: VTU Writer (Completed)
- Implemented **Streaming Mode**.
- Virtual offset tracking for XML metadata.
- Support for Points, Cells (Topology), PointData, and CellData attributes.
- Built-in validation for array size consistency.

### Phase 5: VTI Writer & Adapt support (Completed)
- Support for **ImageData** (.vti) with structured grids (Extent, Origin, Spacing).
- Full compatibility with `microvtk::adapt()` for writing from Array-of-Structs (AoS).
- Zero-copy streaming and compression support.

### Phase 6: PVD Writer (Completed)
- Support for multi-step time series.
- **Explicit Save**: Performance-optimized to avoid redundant I/O during simulation loops.

### Phase 7: Integration Testing (Completed)
- Automated compatibility verification using official VTK Python library for all formats (VTU, VTI, PVD).
- Dependency management using `uv`.
- **Memory Safety**: Integration with **Valgrind** in CI to detect memory leaks and undefined behavior.
- GitHub Actions integration for both Linux and Windows.

---

## 5. Coding Standards & Quality Gates

* **Namespace:** `namespace microvtk`.
* **Safety:** Size validation on topology arrays. `noexcept` specifications on performance-critical adapters.
*   **Testing:**
    *   Comprehensive unit tests covering edge cases.
    *   **Integration Gate**: All generated files must be readable by official VTK bindings.
    *   **Memory Gate**: CI must pass Valgrind checks with zero errors and zero leaks.
*   **Coverage:** Target >85% (excluding platform-specific endian logic).

---

## 6. API Specification (Final)

```cpp
using namespace microvtk;

// 1. Unstructured Grid (.vtu)
VtuWriter writer;
std::vector<double> points = { ... };
writer.setPoints(points);
// ... set cells ...
writer.write("output.vtu");

// 2. Image Data (.vti) with AoS adaptation
std::array<int, 6> extent = {0, 9, 0, 9, 0, 0};
VtiWriter vti(extent);

struct Pixel { double val; };
std::vector<Pixel> data(100);
vti.addPointData("Val", adapt(data, &Pixel::val));
vti.write("image.vti");

// 3. Time Series (.pvd)
PvdWriter pvd("sim.pvd");
pvd.addStep(0.0, "step0.vtu");
pvd.save(); // Explicit save for performance
```
