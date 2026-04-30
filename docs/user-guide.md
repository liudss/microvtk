# MicroVTK User Guide

This guide covers the day-to-day API surface for writing VTK XML files from
C++20 simulation and analysis code.

## Headers

Use the umbrella header for normal integration:

```cpp
#include <microvtk/microvtk.hpp>
```

Use narrower headers when compile time matters:

```cpp
#include <microvtk/vtu_writer.hpp>
#include <microvtk/vti_writer.hpp>
#include <microvtk/pvd_writer.hpp>
#include <microvtk/adapter.hpp>
#include <microvtk/indexing_adapter.hpp>
```

Kokkos and Cabana adapters are available through:

```cpp
#include <microvtk/kokkos_adapter.hpp>
#include <microvtk/cabana_adapter.hpp>
```

Those headers require the matching CMake option and dependency target.

## Data Lifetime

MicroVTK stores views to user-provided ranges. Keep every container or view that
you pass to a writer alive until `write()` or `save()` completes.

```cpp
std::vector<double> points = {/* ... */};
std::vector<double> pressure = {/* ... */};

microvtk::VtuWriter writer;
writer.setPoints(points);
writer.addPointData("pressure", pressure);
writer.write("mesh.vtu"); // points and pressure must still be alive here.
```

The uncompressed path streams contiguous little-endian data directly where
possible. Adapted, padded, non-contiguous, or compressed data may require
transforming or materializing bytes as part of producing the VTK layout.

## Writing VTU Files

`VtuWriter` writes VTK `UnstructuredGrid` files (`.vtu`).

```cpp
#include <cstdint>
#include <microvtk/microvtk.hpp>
#include <vector>

int main() {
  std::vector<double> points = {
      0.0, 0.0, 0.0,
      1.0, 0.0, 0.0,
      0.0, 1.0, 0.0,
      0.0, 0.0, 1.0,
  };

  std::vector<std::int32_t> connectivity = {0, 1, 2, 3};
  std::vector<std::int32_t> offsets = {4};
  std::vector<std::uint8_t> types = {
      static_cast<std::uint8_t>(microvtk::CellType::Tetra),
  };

  std::vector<double> pressure = {101.3, 102.0, 101.5, 100.8};

  microvtk::VtuWriter writer;
  writer.setPoints(points);
  writer.setCells(connectivity, offsets, types);
  writer.addPointData("pressure", pressure);
  writer.write("mesh.vtu");
}
```

`setPoints(points, inputDim)` accepts flattened coordinates. `inputDim` may be
`1`, `2`, or `3`; lower-dimensional points are padded to VTK's 3D coordinate
layout while writing.

Cell topology follows the VTK XML convention:

| Argument | Meaning |
| --- | --- |
| `connectivity` | Flattened point indices for all cells. |
| `offsets` | Exclusive end offset for each cell in `connectivity`. |
| `types` | VTK cell type code for each cell. |

`setCells()` validates that offsets are strictly increasing, the final offset
matches the connectivity size, and all point indices are in range.

## Writing VTI Files

`VtiWriter` writes VTK `ImageData` files (`.vti`) for rectilinear index spaces
with uniform origin and spacing.

```cpp
#include <array>
#include <microvtk/microvtk.hpp>
#include <vector>

int main() {
  std::array<int, 6> extent = {0, 9, 0, 9, 0, 0};
  std::array<double, 3> origin = {0.0, 0.0, 0.0};
  std::array<double, 3> spacing = {1.0, 1.0, 1.0};

  std::vector<double> temperature(100, 300.0);

  microvtk::VtiWriter writer(extent, origin, spacing);
  writer.addPointData("temperature", temperature);
  writer.write("image.vti");
}
```

The extent is inclusive and ordered as:

```text
x_min, x_max, y_min, y_max, z_min, z_max
```

For point data, the number of tuples must match the point count implied by the
extent. For cell data, MicroVTK uses at least one cell along each axis, which
allows 2D and 1D image datasets to carry cell data consistently.

## Point and Cell Data

Both `VtuWriter` and `VtiWriter` support:

```cpp
writer.addPointData("name", range, components);
writer.addCellData("name", range, components);
```

The default component count is `1`. Pass `3` for vector fields or `9` for
tensor fields:

```cpp
writer.addPointData("velocity", velocity, 3);
writer.addCellData("stress", stress, 9);
```

The range must contain `tuple_count * components` scalar values after adaptation.
Validation runs before writing and throws exceptions for mismatched sizes.

## Adapters

Use `microvtk::view(container)` when you want an explicit `std::span` over a
contiguous scalar container:

```cpp
auto pressure_view = microvtk::view(pressure);
writer.addPointData("pressure", pressure_view);
```

Use `microvtk::adapt(container, &Type::member)` for array-of-structs data:

```cpp
struct Particle {
  double mass;
  std::array<double, 3> velocity;
};

std::vector<Particle> particles = /* ... */;
writer.addPointData("mass", microvtk::adapt(particles, &Particle::mass));
```

The adapter also supports pipe syntax:

```cpp
auto mass = particles | microvtk::adapt(&Particle::mass);
writer.addPointData("mass", mass);
```

## Morton/Z-Order Data

`indexing_adapter.hpp` provides a view that exposes Morton-ordered storage in
raster order:

```cpp
#include <microvtk/indexing_adapter.hpp>

std::vector<double> morton_temperature = /* ... */;
std::array<std::size_t, 3> dims = {128, 128, 128};

auto raster_view =
    morton_temperature | microvtk::views::reorder_z_curve(dims);

writer.addPointData("temperature", raster_view);
```

The adapter supports 2D and 3D dimensions. When the compiler and CPU support
BMI2, coordinate encoding can use hardware `PDEP` instructions.

## Compression

Compression is optional and configured through CMake. The default top-level
configuration enables ZLIB and LZ4 support.

```cpp
writer.setCompression(microvtk::core::CompressionType::ZLib);
writer.setCompression(microvtk::core::CompressionType::LZ4);
writer.setCompression(microvtk::core::CompressionType::None);
```

If a compression backend is disabled or unavailable, do not select it at runtime.
Configure with `-DMICROVTK_USE_ZLIB=ON` or `-DMICROVTK_USE_LZ4=ON` when the
application needs compressed output.

## Time Series

`PvdWriter` writes `.pvd` collection files that point to per-step `.vtu` files.

```cpp
#include <format>
#include <microvtk/microvtk.hpp>

int main() {
  microvtk::PvdWriter pvd("simulation.pvd");

  for (int step = 0; step < 10; ++step) {
    auto filename = std::format("step_{:04d}.vtu", step);

    // Write filename with VtuWriter here.

    pvd.addStep(step * 0.01, filename);
  }

  pvd.save();
}
```

Paths stored in the `.pvd` file should usually be relative to the `.pvd`
location so ParaView and other VTK readers can move the dataset directory as a
unit.
