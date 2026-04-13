# MicroVTK Python Bindings Specification

## 1. Purpose

The Python bindings provide a NumPy-first API for writing VTK XML files through
the MicroVTK C++ core. The binding layer should make common scientific Python
workflows convenient while preserving MicroVTK's streaming writer model, VTK type
mapping, appended binary output, compression support, and late validation.

The bindings are not intended to mirror every C++ range adapter directly. Python
users should work with NumPy arrays, dictionaries of fields, and small writer
objects. The C++ core remains responsible for the actual `.vtu`, `.vti`, and
`.pvd` serialization.

## 2. Design Goals

- Provide a small, idiomatic Python API for NumPy arrays.
- Reuse MicroVTK's C++ writers instead of duplicating VTK XML logic in Python.
- Keep zero-copy behavior for C-contiguous arrays with supported dtypes.
- Preserve object lifetimes safely when writer objects defer serialization until
  `write()`.
- Support explicit writer classes and one-shot convenience functions.
- Keep the first implementation focused on VTU, VTI, and PVD writing.
- Use nanobind as a thin bridge between Python objects and MicroVTK.

## 3. Non-Goals

- Do not expose C++20 ranges, `adapt()`, Kokkos, or Cabana concepts directly in
  the first Python API.
- Do not implement a general mesh processing library.
- Do not depend on PyVista, meshio, or VTK for writing.
- Do not silently convert unsupported dtypes such as complex or object arrays.
- Do not make Python own or reimplement the VTK appended binary format.

## 4. Binding Technology

The binding should use nanobind with scikit-build-core.

Nanobind is appropriate because the public surface is small and the primary
task is passing NumPy buffers into C++ writer methods. It provides direct support
for `nb::ndarray`, shape inspection, dtype inspection, and C++ class bindings
without adding a large abstraction layer.

Recommended division of responsibility:

- nanobind: expose C++ classes and safely bridge NumPy buffers.
- Python package layer: provide ergonomic helpers such as `write_vtu()` and
  `write_vti()`.
- MicroVTK C++ core: perform VTK XML writing, appended data offsets, compression,
  type mapping, and validation.

## 5. Public Module Surface

The top-level package should expose:

```python
import microvtk as vtk

vtk.write_vtu
vtk.write_vti
vtk.write_pvd

vtk.VtuWriter
vtk.VtiWriter
vtk.PvdWriter

vtk.CellType
vtk.Compression
```

The C++ enum may remain `CompressionType`, but Python should expose the shorter
name `Compression`.

Compression arguments should accept either enum values or strings:

```python
compression=None
compression="none"
compression="zlib"
compression="lz4"
compression=vtk.Compression.LZ4
```

## 6. High-Level API

### 6.1 `write_vtu`

`write_vtu()` writes an unstructured grid.

```python
vtk.write_vtu(
    filename,
    points,
    cells,
    point_data=None,
    cell_data=None,
    compression=None,
)
```

Example:

```python
import numpy as np
import microvtk as vtk

points = np.array(
    [
        [0.0, 0.0, 0.0],
        [1.0, 0.0, 0.0],
        [0.0, 1.0, 0.0],
        [0.0, 0.0, 1.0],
    ],
    dtype=np.float64,
)

connectivity = np.array([0, 1, 2, 3], dtype=np.int32)
offsets = np.array([4], dtype=np.int32)
types = np.array([vtk.CellType.Tetra], dtype=np.uint8)

vtk.write_vtu(
    "mesh.vtu",
    points=points,
    cells=(connectivity, offsets, types),
    point_data={"temperature": np.arange(4, dtype=np.float32)},
    cell_data={"material": np.array([7], dtype=np.int32)},
    compression="lz4",
)
```

First-phase cell input should be the explicit VTK XML tuple:

```python
cells = (connectivity, offsets, types)
```

Later phases may add a friendlier list-of-cells helper, but the explicit tuple
is the stable core API.

### 6.2 `write_vti`

`write_vti()` writes image data on a uniform grid.

```python
vtk.write_vti(
    filename,
    point_data=None,
    cell_data=None,
    extent=None,
    origin=(0.0, 0.0, 0.0),
    spacing=(1.0, 1.0, 1.0),
    compression=None,
)
```

Example:

```python
density = np.random.random((64, 64, 32)).astype(np.float32)
velocity = np.random.random((64, 64, 32, 3)).astype(np.float32)

vtk.write_vti(
    "field.vti",
    point_data={"density": density, "velocity": velocity},
    origin=(0.0, 0.0, 0.0),
    spacing=(0.1, 0.1, 0.2),
    compression="zlib",
)
```

When `extent` is omitted, the implementation may infer it from point data. If
both point and cell data are present and the grid shape is ambiguous, the
function should require an explicit extent.

### 6.3 `write_pvd`

`write_pvd()` writes a time-series collection.

```python
vtk.write_pvd(
    "series.pvd",
    [
        (0.0, "step_000.vtu"),
        (0.1, "step_001.vtu"),
    ],
)
```

## 7. Writer Class API

The writer classes provide lower-level control while staying NumPy-oriented.

### 7.1 `VtuWriter`

```python
writer = vtk.VtuWriter()
writer.set_compression("lz4")
writer.set_points(points)
writer.set_cells(connectivity, offsets, types)
writer.add_point_data("temperature", temperature)
writer.add_point_data("velocity", velocity, components=3)
writer.add_cell_data("material", material_id)
writer.write("mesh.vtu")
```

Methods:

```python
set_compression(compression)
set_points(points, input_dim=None)
set_cells(connectivity, offsets, types)
add_point_data(name, data, components=None)
add_cell_data(name, data, components=None)
write(filename)
```

### 7.2 `VtiWriter`

```python
writer = vtk.VtiWriter(
    extent=(0, 63, 0, 63, 0, 31),
    origin=(0.0, 0.0, 0.0),
    spacing=(0.1, 0.1, 0.2),
)
writer.set_compression("zlib")
writer.add_point_data("density", density)
writer.add_point_data("velocity", velocity, components=3)
writer.write("field.vti")
```

Methods:

```python
set_compression(compression)
add_point_data(name, data, components=None)
add_cell_data(name, data, components=None)
write(filename)
```

### 7.3 `PvdWriter`

```python
pvd = vtk.PvdWriter("series.pvd")
pvd.add_step(0.0, "step_000.vtu")
pvd.add_step(0.1, "step_001.vtu")
pvd.save()
```

## 8. NumPy Shape Rules

### 8.1 VTU Points

Accepted point shapes:

```python
(num_points, 3)
(num_points, 2)
(num_points, 1)
```

For a flat coordinate array, the user should provide `input_dim`:

```python
writer.set_points(flat_points, input_dim=3)
```

The implementation should pass `input_dim` through to
`microvtk::VtuWriter::setPoints`, allowing the C++ writer to pad 1D and 2D
coordinates to 3D.

### 8.2 VTU Point and Cell Data

Accepted field shapes:

```python
(num_tuples,)      # scalar field
(num_tuples, k)    # k-component field
```

If `components` is omitted, it should be inferred as:

- `1` for 1D arrays.
- `array.shape[1]` for 2D arrays.

### 8.3 VTI Point and Cell Data

VTI fields may use grid-shaped arrays:

```python
(nx, ny)
(nx, ny, nz)
(nx, ny, nz, k)
```

For VTI, the last dimension may be treated as `components` when it is explicitly
provided or inferable. The remaining dimensions describe the logical grid shape.

The implementation should flatten arrays in C order before passing them to the
C++ writer.

## 9. Dtype Rules

The Python binding should support NumPy dtypes that map directly to MicroVTK
type traits:

| NumPy dtype | VTK XML type |
| --- | --- |
| `np.int8` | `Int8` |
| `np.uint8` | `UInt8` |
| `np.int16` | `Int16` |
| `np.uint16` | `UInt16` |
| `np.int32` | `Int32` |
| `np.uint32` | `UInt32` |
| `np.int64` | `Int64` |
| `np.uint64` | `UInt64` |
| `np.float32` | `Float32` |
| `np.float64` | `Float64` |

Unsupported dtypes should raise `TypeError`. Examples of unsupported dtypes:

- `bool`
- `complex64`
- `complex128`
- `object`
- string dtypes

The binding should not silently change dtype. If users want a different dtype,
they should explicitly call `array.astype(...)`.

## 10. Memory and Copy Semantics

The preferred path is zero-copy:

- dtype is supported.
- array is C-contiguous.
- array lifetime is retained until `write()`.

If an array is not C-contiguous, the Python layer may convert it with
`np.ascontiguousarray()` and store the converted array in the writer object.
This is a layout copy, not a dtype conversion.

The writer must retain Python objects that back C++ spans until the write
operation completes. This is necessary because MicroVTK writer objects store
accessors and write data later.

Recommended behavior:

- C-contiguous supported dtype: no copy.
- Non-C-contiguous supported dtype: make a contiguous copy and retain it.
- Unsupported dtype: raise `TypeError`.

## 11. Implementation Notes

### 11.1 Dtype Dispatch

The nanobind layer should use a small dispatch helper that inspects the NumPy
dtype and calls the C++ writer with a typed `std::span<const T>`.

Conceptual shape:

```cpp
dispatch_array(obj, [&](auto span, int components) {
  writer_.addPointData(name, span, components);
});
```

The helper should cover all supported numeric dtypes and produce clear error
messages for unsupported dtypes.

### 11.2 Component Inference

Component inference should be handled at the binding or Python wrapper layer,
before calling C++:

```text
components argument provided -> use it
1D field array -> components = 1
2D VTU field array -> components = shape[1]
VTI grid field -> infer from field/grid rules
otherwise -> raise ValueError
```

### 11.3 Path Handling

Public Python APIs should accept `str` and `pathlib.Path`. Values should be
converted to strings before crossing into C++.

### 11.4 Enum Handling

`CellType` values should be convertible to `uint8` for the VTK cell types array.
For the first phase, requiring `types` to be a `uint8` array is acceptable.
Later helpers can accept lists of enum values and normalize them.

## 12. Error Handling

Errors should be early and explicit when possible.

Examples:

```text
TypeError: dtype complex64 is not supported by microvtk.
ValueError: points must have shape (n, 1), (n, 2), or (n, 3).
ValueError: point_data['velocity'] has 5 tuples, expected 4.
ValueError: cell connectivity, offsets, and types must be one-dimensional.
ValueError: VTI extent is ambiguous; pass extent explicitly.
```

Late size validation in the C++ writers remains useful and should not be
disabled.

## 13. Testing Strategy

Add Python binding tests separate from the existing example-driven integration
tests.

Recommended tests:

- Import `microvtk`.
- Write a basic VTU from NumPy arrays and read it back with official VTK.
- Write VTU point data with `float32`, `float64`, `int32`, and `uint8`.
- Write multi-component point data such as shape `(n, 3)`.
- Write VTI scalar field from shape `(nx, ny, nz)`.
- Write VTI vector field from shape `(nx, ny, nz, 3)`.
- Write a PVD file through `write_pvd()` and `PvdWriter`.
- Validate error paths for unsupported dtype and invalid shapes.
- Test non-contiguous arrays if the API chooses to make contiguous layout copies.

## 14. Development Plan

### Phase 1: Stable NumPy Writer Core

- Keep nanobind and scikit-build-core.
- Add dtype dispatch for supported NumPy numeric dtypes.
- Add `components` support to `add_point_data()` and `add_cell_data()`.
- Improve point shape handling for VTU.
- Preserve Python object lifetimes for deferred writes.
- Add Python tests for writer classes.

### Phase 2: High-Level Python Helpers

- Implement `write_vtu()`.
- Implement `write_vti()`.
- Implement `write_pvd()`.
- Add `pathlib.Path` support.
- Add string-to-enum compression normalization.

### Phase 3: Convenience and Advanced Layouts

- Add helpers for building `connectivity`, `offsets`, and `types` from simple
  Python cell lists if needed.
- Consider exposing Morton/Z-order reorder helpers for NumPy arrays.
- Consider additional packaging metadata and wheel CI.

## 15. Open Questions

- Should `write_vti()` infer extent from `point_data` only, or also from
  `cell_data` when no point data is provided?
- Should non-contiguous arrays be copied automatically in the C++ binding, or
  normalized in the Python wrapper with NumPy?
- Should `CellType` arrays accept enum lists in phase 1, or only `np.uint8`?
- Should `Compression` be a renamed Python enum while preserving
  `CompressionType` as an alias for compatibility?
