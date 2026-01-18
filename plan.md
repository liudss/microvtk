### 1. System Overview

We will adopt a **Layered Architecture**, abstracting from low-level memory operations to high-level user APIs step-by-step.

```mermaid
graph TD
    UserCode[User Code (Solver / Sim)] --> AdapterLayer[Adapter Layer]
    
    subgraph MicroVTK Library
        direction TB
        AdapterLayer --> DataModel[Data Model Layer]
        DataModel --> LogicLayer[Logic Layer]
        LogicLayer --> IOBackend[I/O Backend (Physical I/O)]
    end
    
    IOBackend --> Disk[(Disk Files .vtu / .pvd)]
```

---

### 2. Detailed Module Design

#### Layer 1: Adapter Layer - C++20 Concepts

This is the **soul** of the library. It is responsible for "translating" various user data structures into views that the library can understand, ensuring **zero-copy**.

*   **`concepts.hpp`**: Defines standard concepts.
    *   `ScalarArray`: Contiguous scalar memory (e.g., `std::vector<float>`).
    *   `VectorArray`: Contiguous vector memory (e.g., `std::vector<Eigen::Vector3d>`).
    *   `StridedArray`: Non-contiguous or AoS (Array of Structures) which requires a `get(i)` method.
*   **`adapter.hpp`**: Provides helper functions.
    *   `vtk::view(container)`: Automatically deduces and generates a `std::span`.
    *   `vtk::adapt(container, &Struct::member)`: Generates member accessors.

#### Layer 2: Data Model Layer - Unified View

This layer does not store data; it only stores **Metadata and References**.

*   **`DataArrayView`**: A lightweight structure containing `Name`, `ComponentCount`, `DataType` (float/int), and a `std::span<const byte>` (or a generator lambda).
*   **`MeshTopology`**:
    *   Describes the mesh structure. For `.vtu`, it holds views of Points, Connectivity, Offsets, and Types.
    *   For `.vti`, it only holds Origin, Spacing, and Extent.

#### Layer 3: Logic Layer - Format Orchestration

This layer handles the complex logic of the VTK XML format, especially offset calculations for the `Appended` mode.

*   **`XMLWriter`**: A minimal XML generator (simpler than pugixml, responsible only for streaming `<Tag attr="...">`).
*   **`Encoder` Strategy Pattern**:
    *   `RawEncoder`: Direct memory copy (fastest).
    *   `Base64Encoder`: Used for Inline mode (fallback).
    *   `Compressor` (Optional): Wraps `miniz` or `lz4`. Activated if the `USE_COMPRESSION` macro is defined.
*   **`OffsetManager`**: Records the byte offset of each DataArray in the binary block at the end of the file while writing the XML header.

#### Layer 4: I/O Backend (Physical I/O)

*   **`FileWriter`**: Wraps `std::ofstream` or `FILE*`. Handles file buffering, endianness conversion (if necessary), and appending binary data.

---

### 3. Proposed Directory Structure

To keep it lightweight, a **Single-Header** or **Micro-Module** structure is suggested.

```text
microvtk/
├── include/
│   └── microvtk/
│       ├── common/
│       │   ├── types.hpp       // CellType enum, basic typedefs
│       │   └── concepts.hpp    // C++20 concepts
│       ├── core/
│       │   ├── xml_builder.hpp // Simple XML string builder
│       │   ├── encoder.hpp     // Base64 & Compression logic
│       │   └── binary_io.hpp   // Endianness & Raw writing
│       ├── format/
│       │   ├── vtu_writer.hpp  // Unstructured Grid
│       │   ├── vtp_writer.hpp  // PolyData
│       │   ├── vti_writer.hpp  // ImageData
│       │   └── pvd_writer.hpp  // Time series
│       └── microvtk.hpp        // Main entry, includes all headers
├── examples/
│   ├── 01_simple_fem.cpp       // Basic usage
│   ├── 02_eigen_adapter.cpp    // Adapting Eigen
│   ├── 03_mpi_parallel.cpp     // Parallel PVTU
│   └── 04_time_series.cpp      // PVD Animation
├── tests/
│   └── ...
└── CMakeLists.txt              // Supports FetchContent
```

---

### 4. Core API Pseudo-code Preview

This is how the final API will appear to the user:

```cpp
#include <microvtk/microvtk.hpp>

void export_simulation() {
    using namespace microvtk;

    // 1. Initialize Writer (Specify Appended mode + Compression)
    VtuWriter writer("output.vtu", Mode::Appended | Mode::Compressed);

    // 2. Set Geometry (Zero-copy, pass vector directly)
    std::vector<double> points = {0,0,0, 1,0,0, ...};
    writer.setPoints(points);

    // 3. Set Topology
    // Use the CellType enum provided by the library
    std::vector<uint8_t> types = { CellType::Tetra, CellType::Hexahedron };
    writer.setCells(connectivity, offsets, types);

    // 4. Add Attributes (Using Adapters)
    // Assuming particles is an array of structs
    writer.addPointData("Velocity", adapt(particles, &Particle::vel)); // 3 components
    writer.addPointData("Mass", adapt(particles, &Particle::mass));    // 1 component

    // 5. Add Global Fields (Field Data)
    writer.addFieldData("TimeValue", 0.05);
    writer.addFieldData("Cycle", 100);

    // 6. Write to Disk
    writer.write();
}
```

---

### 5. Development Roadmap & Priorities

To ensure project controllability, the following implementation order is recommended:

#### Phase 1: The Skeleton

*   **Goal:** Output valid, non-visual XML structures.
*   **Tasks:**
    *   Set up CMake structure.
    *   Implement `XMLBuilder`.
    *   Define `CellType` enums.
    *   Implement `VtuWriter` in ASCII mode (supporting only `std::vector`).

#### Phase 2: The Performance Core

*   **Goal:** Implement `Binary Appended` mode.
*   **Tasks:**
    *   Implement `OffsetManager` for binary block position calculation.
    *   Implement `RawEncoder` (using `fwrite`).
    *   Implement Base64 encoding (as the header encoder for Binary).
*   **Milestone:** The library is now practically useful.

#### Phase 3: The Ecosystem

*   **Goal:** Enhance user experience.
*   **Tasks:**
    *   Introduce C++20 Concepts.
    *   Implement `vtk::adapt` template magic.
    *   Implement `PvdWriter` (Time Series).

#### Phase 4: Advanced Features

*   **Goal:** Cover professional requirements.
*   **Tasks:**
    *   Integrate `miniz` for compression.
    *   Implement `PvtuWriter` (MPI Parallel).
    *   Implement `VtpWriter` (PolyData).
    *   Support high-order elements.

---

### 6. Design Highlights

1.  **Separation of Data Ownership**: The library **never** owns data; it only holds `std::span` views. This fundamentally eliminates memory overhead.
2.  **Hidden Complexity**: Users don't need to know how VTK encodes headers with Base64 or manually calculate offsets; these are automated in the `LogicLayer`.
3.  **Extremely Flexible Interface**: If your data is iterable, the library can write it.
4.  **Future-Oriented PVD**: Time-series management is a core design feature, not an afterthought.

This architecture meets the requirement for a **lightweight** solution while retaining high **flexibility** through the adapter pattern, providing a modern C++ solution for long-term maintenance.
