# MicroVTK: High-Performance C++20 VTK Library Specification

## 1. Role & Objective
You are a Senior C++ Systems Engineer specializing in High-Performance Computing (HPC) and Visualization.
**Your Goal:** Implement `MicroVTK`, a lightweight, header-only, modern C++20 library for writing VTK files (`.vtu`, `.pvd`).
**Constraints:** - Strict C++20 standard (Concepts, Spans, Ranges).
- **Zero-Dependency** for the core library (no external XML or Compression libs).
- **Zero-Copy** architecture using `std::span` and Adapters.
- Optimized for "Appended Binary" format (Raw binary dump) for speed.

---

## 2. Development Environment
* **OS:** Windows (MSYS2 UCRT64 environment).
* **Build System:** CMake 3.25+ (Generator: Ninja).
* **Compilers:** Must compile cleanly on **GCC 13+** and **Clang 16+**.
* **Tools:** `clang-format` (Google Style), `clang-tidy` (Performance & Modernize checks).
* **Testing:** GoogleTest (GTest) via Git Submodule.

---

## 3. Architecture Overview

### 3.1 Directory Structure
```text
microvtk/
├── cmake/                  # CMake helper modules
├── examples/               # Usage examples
├── external/               # Third-party dependencies
│   └── googletest/         # Git submodule
├── include/
│   └── microvtk/
│       ├── common/         # Enums, Concepts, Traits
│       ├── core/           # XML Builder, Endianness, Base64, binary buffers
│       ├── vtu_writer.hpp  # Main UnstructuredGrid Writer
│       ├── pvd_writer.hpp  # Time Series Writer
│       ├── adapter.hpp     # Data Adapters (Zero-copy)
│       └── microvtk.hpp    # Main entry header
├── tests/                  # Unit tests (GTest)
├── CMakeLists.txt
└── .clang-format

```

### 3.2 Key Design Decisions

1. **File Format:** Focus on **XML UnstructuredGrid (.vtu)** using **Appended Mode**.
* *Structure:* XML Metadata -> `<AppendedData>` -> `_` (marker) -> `[SizeHeader][BinaryData]...`
* *Why:* Fastest write speed, smallest parsing overhead.


2. **Memory Management:** * The library does **not** own data (except the internal binary buffer for writing).
* It uses `std::span<const T>` to view user data.


3. **Endianness:** VTK XML defaults to Little Endian. The library must detect host endianness and swap bytes if necessary (e.g., using `std::byteswap` or `std::endian`).
4. **Adapters:** Use C++20 Concepts (`std::ranges::contiguous_range`) and custom traits to allow users to pass `std::vector`, `std::array`, `Eigen::Matrix`, or AoS structs directly.

---

## 4. Implementation Roadmap (Execute Sequentially)

### Phase 1: Infrastructure & Scaffolding

* **Task:** Setup CMake, git submodule, and directory tree.
* **Requirement:** Ensure `cmake --build` works and links GTest.
* **File:** `CMakeLists.txt` must expose `microvtk` as an `INTERFACE` library.

### Phase 2: Core Utilities (The Foundation)

* **Task:** Implement independent utilities.
* **Files:**
* `include/microvtk/common/types.hpp`: Enums for `CellType` (Tetra=10, Hex=12, etc.) and `DataFormat`.
* `include/microvtk/core/xml_utils.hpp`: A minimal stream-based XML builder (no deps).
* `include/microvtk/core/binary_utils.hpp`:
* `Base64` encoder (for the header of binary blocks, if needed, though Raw uses size headers).
* `Endian` handling.
* `append_buffer`: A helper to push `std::span` data into a `std::vector<uint8_t>`.





### Phase 3: The Data Adapter Layer (C++20 Magic)

* **Task:** Implement the Zero-Copy mechanism.
* **File:** `include/microvtk/adapter.hpp`.
* **Logic:**
* Define `concept Scalar` and `concept NumericRange`.
* Implement `vtk::view(container)` that returns `std::span<const T>`.
* Implement `vtk::adapt(container, &Struct::member)` to handle Array-of-Structures (AoS). *Note: For Appended mode, AoS data must be copied to the contiguous binary buffer internally.*



### Phase 4: VTU Writer (Appended Mode Logic)

* **Task:** The main logic.
* **File:** `include/microvtk/vtu_writer.hpp`.
* **Workflow:**
1. User calls `writer.setPoints(data)`.
2. Writer appends data to internal `std::vector<uint8_t> binary_blob_`.
3. Writer records `offset` (start position in blob) and `size`.
4. User calls `writer.write(filename)`.
5. Writer generates XML using recorded offsets, then dumps `binary_blob_` at the end.


* **Header Format:** For each binary block: `[uint32_t payload_size] + [payload_bytes]`.

### Phase 5: PVD Writer (Time Series)

* **Task:** Link multiple VTU files.
* **File:** `include/microvtk/pvd_writer.hpp`.
* **Logic:** Simple XML generation. Support `overwrite` mode for live updates.

---

## 5. Coding Standards & Quality Gates

* **Namespace:** All code under `namespace microvtk`.
* **Style:** Google C++ Style (`.clang-format`).
* **Safety:** Use `static_assert` to prevent unsupported types. Use `std::bit_cast` for safe type punning if needed.
* **Documentation:** Doxygen-style `///` comments for public APIs.
* **Testing:** Every feature must have a corresponding test in `tests/`.

---

## 6. Detailed API Specification (Reference)

```cpp
// Example Usage to guide your implementation:
using namespace microvtk;

// 1. Setup Writer
VtuWriter writer(DataFormat::Appended);

// 2. Data (Zero Copy)
std::vector<double> points = { ... };
writer.setPoints(points);

// 3. Topology
std::vector<int32_t> conn = { ... };
std::vector<int32_t> offsets = { ... };
std::vector<uint8_t> types = { ... };
writer.setCells(conn, offsets, types);

// 4. Attributes (using Adapters)
struct Particle { double mass; double vel[3]; };
std::vector<Particle> parts = ...;

// Writes 1 component array
writer.addPointData("Mass", adapt(parts, &Particle::mass)); 
// Writes 3 component array
// Note: You need to implement logic to handle array members or flatten them
writer.addPointData("Velocity", adapt(parts, &Particle::vel)); 

// 5. Write to disk
writer.write("output.vtu");

```

---

## Instruction to Agent

Start by initializing the project structure and Phase 1 (CMake/Conan). Once confirmed working, proceed phase by phase. **Always implement the Header-only library code first, then the corresponding Unit Test.**
