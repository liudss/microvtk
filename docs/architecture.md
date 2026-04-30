# MicroVTK Architecture

MicroVTK is a header-only C++20 library for writing VTK XML files from existing
simulation data structures. Its architecture is centered on a small public API,
range-based data adapters, and a shared appended-data writer pipeline.

## Design Goals

| Goal | Architectural consequence |
| --- | --- |
| Header-only integration | The exported target is an `INTERFACE` CMake target and public behavior lives in headers under `include/microvtk/`. |
| Low memory overhead | Writers store views to caller-owned data and stream bytes to the output file when possible. |
| VTK compatibility | Writers emit VTK XML files with appended raw binary payloads, little-endian byte order, and `UInt64` block headers. |
| Optional HPC dependencies | Kokkos, Cabana, ZLIB, and LZ4 are enabled through CMake options and compile definitions. |
| Extensible data ingestion | Adapters expose external layouts as C++ ranges rather than requiring writer-specific container types. |

## Module Map

```text
include/microvtk/
├── microvtk.hpp                 Umbrella include
├── vtu_writer.hpp               Public UnstructuredGrid writer
├── vti_writer.hpp               Public ImageData writer
├── pvd_writer.hpp               Public time-series collection writer
├── adapter.hpp                  Standard range and AoS adapters
├── indexing_adapter.hpp         Morton/Z-order reordering views
├── kokkos_adapter.hpp           Optional Kokkos View adapter
├── cabana_adapter.hpp           Optional Cabana Slice adapter
├── common/types.hpp             VTK cell types and scalar-to-VTK type names
└── core/
    ├── appended_data_writer.hpp Shared VTK appended-file orchestration
    ├── appended_data_attributes.hpp Attribute registration and validation
    ├── appended_data_payload.hpp Offset computation and payload writing
    ├── appended_data_block.hpp Metadata for one VTK DataArray payload
    ├── data_accessor.hpp        Type-erased range-to-byte streaming
    ├── binary_utils.hpp         Little-endian and binary helpers
    ├── compressor.hpp           ZLIB/LZ4 compression abstraction
    └── xml_utils.hpp            Streaming XML builder
```

## Layering

```text
Application containers and framework data
        |
        v
Adapters and range views
        |
        v
Public writers: VtuWriter, VtiWriter, PvdWriter
        |
        v
AppendedDataAttributes and DataAccessor registry
        |
        v
AppendedDataWriter orchestration
        |
        v
XML headers + appended binary payload + optional compression
        |
        v
.vtu, .vti, and .pvd files
```

The public writer classes own format-specific validation and XML structure.
The core layer owns shared mechanics: registering data blocks, computing appended
payload offsets, emitting VTK XML headers, converting scalar ranges to bytes, and
optionally compressing payloads.

## Write Pipeline

For `.vtu` and `.vti` output, the pipeline is:

1. User code passes ranges to a writer through `setPoints()`, `setCells()`,
   `addPointData()`, or `addCellData()`.
2. `AppendedDataAttributes::registerData()` records one `DataBlockInfo` and one
   `RangeAccessor` for each range.
3. The concrete writer validates format-specific sizes before writing.
4. `AppendedDataWriter::writeAppendedVtkFile()` prepares offsets and opens the
   target file.
5. The concrete writer emits XML structure and `DataArray` metadata with
   appended offsets.
6. `writeAppendedData()` writes the appended payload blocks in the same order as
   the XML headers.

In uncompressed output, each appended block is:

```text
UInt64 byte_count
raw scalar bytes in little-endian order
```

In compressed output, each block is prepared before XML emission so the writer
can compute final offsets. The compressed block header follows the VTK appended
compression convention used by this project.

## Data Ownership and Lifetime

Writers do not take ownership of large user data. `RangeAccessor` stores a
range view created from the caller's range, and writers keep those accessors
until `write()` finishes.

This has two important constraints:

| Constraint | Reason |
| --- | --- |
| Caller data must outlive `write()` | The writer may still hold views into the original container. |
| Temporary adapted views must remain valid | A pipeline view can reference its source container and adapter closure. |

This design keeps the common path lightweight, but it makes lifetime part of
the public contract.

## Format-Specific Writers

### `VtuWriter`

`VtuWriter` writes VTK `UnstructuredGrid` files.

Responsibilities:

- Register point coordinates through `setPoints()`.
- Accept 1D, 2D, or 3D flattened coordinates and pad lower-dimensional input to
  VTK's 3-component coordinate layout.
- Validate cell topology: offsets must be strictly increasing, the final offset
  must match connectivity size, and connectivity indices must reference existing
  points.
- Emit `Points`, `Cells`, `PointData`, and `CellData` sections.

### `VtiWriter`

`VtiWriter` writes VTK `ImageData` files.

Responsibilities:

- Store inclusive whole extent, origin, and spacing.
- Validate point and cell data sizes against the extent-derived tuple counts.
- Emit `ImageData`, `Piece`, `PointData`, and `CellData` sections.

### `PvdWriter`

`PvdWriter` writes VTK collection files for time series.

It is intentionally separate from the appended-data pipeline because `.pvd`
files contain XML references to per-step datasets rather than binary payloads.

## Adapter Strategy

Adapters convert application layouts into ranges of scalar values that the core
writer can stream.

| Adapter | Purpose |
| --- | --- |
| `microvtk::view(container)` | Expose contiguous scalar containers as `std::span`. |
| `microvtk::adapt(container, &Type::member)` | Expose one member from an array-of-structs layout. |
| `microvtk::views::reorder_z_curve(dims)` | Present Morton/Z-order storage in raster order. |
| `microvtk::adapt(Kokkos::View)` | Flatten Kokkos views in VTK-compatible logical order. |
| `microvtk::adapt(Cabana::Slice)` | Flatten Cabana slices, including multidimensional member arrays. |

The writer layer only requires ranges of arithmetic scalar values. This keeps
framework-specific logic outside the core VTK writing path.

## Dependency Boundaries

The main `microvtk::microvtk` target always exposes the public headers. Optional
features add compile definitions and link dependencies:

| Option | Compile definition | Dependency |
| --- | --- | --- |
| `MICROVTK_USE_ZLIB` | `MICROVTK_HAS_ZLIB` | `ZLIB::ZLIB` |
| `MICROVTK_USE_LZ4` | `MICROVTK_HAS_LZ4` | `lz4::lz4`, `lz4_static`, or `PkgConfig::LZ4` |
| `MICROVTK_USE_KOKKOS` | `MICROVTK_HAS_KOKKOS` | `Kokkos::kokkos` |
| `MICROVTK_USE_CABANA` | `MICROVTK_HAS_CABANA` | `Cabana::cabanacore` |

The build first uses existing CMake targets or `find_package`. In top-level
development builds, CPM.cmake can provide fallback dependencies when enabled.

## Extension Points

Add a new container adapter when data can be represented as a scalar range
without changing file semantics. The preferred shape is a lightweight view that
preserves caller ownership.

Add a new writer when the VTK XML file type needs a different top-level XML
structure or validation model. Reuse `AppendedDataWriter` when the format uses
appended binary `DataArray` payloads.

Add a new compression backend behind `core::Compressor` when VTK readers support
the compressor name and block header convention.

## Key Tradeoffs

| Tradeoff | Rationale |
| --- | --- |
| Views instead of owned buffers | Reduces memory pressure for large simulation outputs, but requires explicit lifetime discipline. |
| Appended binary only | Keeps the implementation focused on high-throughput output and avoids duplicating ASCII/base64 paths. |
| Type-erased data accessors | Lets writers store heterogeneous ranges in one registry while preserving range-based input APIs. |
| Optional dependency flags | Keeps core use simple while allowing HPC and compression integrations in builds that need them. |
