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
* **Tools:** `clang-format` (Google Style), `clang-tidy`, `gcovr` (Coverage).
* **Testing:** GoogleTest (GTest) via Git Submodule.

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
│       ├── common/         # Enums, Concepts, Traits
│       ├── core/
│       │   ├── binary_utils.hpp   # Endianness, Base64
│       │   ├── data_accessor.hpp # Type-erased data streaming (Zero-copy)
│       │   └── xml_utils.hpp     # std::format based XML builder
│       ├── vtu_writer.hpp  # Main UnstructuredGrid Writer (Streaming)
│       ├── pvd_writer.hpp  # Time Series Writer (Explicit Save)
│       ├── adapter.hpp     # Data Adapters (AoS support)
│       └── microvtk.hpp    # Main entry header
├── tests/                  # Unit tests (GTest)
├── CMakeLists.txt
└── .clang-format
```

### 3.2 Key Design Decisions

1. **File Format:** Focus on **XML UnstructuredGrid (.vtu)** using **Appended Mode**.
2. **Streaming Write (Zero-Copy):**
    - The library does **not** buffer binary data in memory.
    - It uses `core::DataAccessor` to store references to user containers.
    - Data is streamed directly from user memory to disk during the `write()` call.
    - **Note:** User must ensure data longevity until `write()` completes.
3. **XML Generation:** Powered by `std::format` (C++20) for superior performance over `std::stringstream`.
4. **Endianness:** VTK XML defaults to Little Endian. The library detects host endianness and performs on-the-fly byte swapping during streaming if necessary.

---

## 4. Implementation Status

### Phase 1-3: Core & Adapters (Completed)
- Full support for C++20 Ranges and Spans.
- `vtk::adapt` for Array-of-Structures (AoS) processing.
- Type-erased `DataAccessor` for seamless integration of various container types.

### Phase 4: VTU Writer (Completed)
- Implemented **Streaming Mode**.
- Virtual offset tracking for XML metadata.
- Support for Points, Cells (Topology), PointData, and CellData attributes.
- Built-in validation for array size consistency.

### Phase 5: PVD Writer (Completed)
- Support for multi-step time series.
- **Explicit Save**: Performance-optimized to avoid redundant I/O during simulation loops.

---

## 5. Coding Standards & Quality Gates

* **Namespace:** `namespace microvtk`.
* **Safety:** Size validation on topology arrays. `noexcept` specifications on performance-critical adapters.
* **Testing:** Comprehensive unit tests covering edge cases (Base64 padding, non-contiguous containers).
* **Coverage:** Target >85% (excluding platform-specific endian logic).

---

## 6. API Specification (Final)

```cpp
using namespace microvtk;

// 1. Setup Writer
VtuWriter writer;

// 2. Data (Zero Copy - References stored)
std::vector<double> points = { ... };
writer.setPoints(points);

// 3. Topology (Validation included)
std::vector<int32_t> conn = { ... };
std::vector<int32_t> offsets = { ... };
std::vector<uint8_t> types = { ... };
writer.setCells(conn, offsets, types);

// 4. Attributes (AoS supported)
struct Particle { double mass; };
std::vector<Particle> parts = ...;
writer.addPointData("Mass", adapt(parts, &Particle::mass));

// 5. Write to disk (Direct stream from 'points', 'conn', etc.)
writer.write("output.vtu");

// 6. Time Series
PvdWriter pvd("sim.pvd");
pvd.addStep(0.0, "step0.vtu");
pvd.save(); // Explicit save for performance
```
