# MicroVTK

A lightweight, high-performance, header-only C++20 library for writing VTK XML files (`.vtu`, `.pvd`) with a streaming zero-copy architecture.

## Key Features

- **Modern C++20**: Built with concepts, spans, ranges, and `std::format`.
- **Streaming Zero-Copy**: True memory-to-disk streaming architecture. Data is referenced via type-erased `DataAccessor` and written directly to the file stream, minimizing peak memory usage.
- **Compression Support**: Built-in support for **ZLib** and **LZ4** compression schemes for efficient storage.
- **High Performance**:
    - effective ingestion throughput exceeding **600 TiB/s** (O(1) pointer registration).
    - Disk-bound write speeds (measured ~1.6 GiB/s on standard I/O).
- **Header-Only**: Easy integration. Linkable via modern CMake `microvtk::microvtk`.
- **Robustness**: Includes size validation for topology arrays and exhaustive unit tests (>90% coverage).

## Requirements

- **Compiler**: GCC 13+, Clang 16+, or MSVC 19.30+.
- **Build System**: CMake 3.25+.
- **Dependencies (for dev/test)**:
    - ZLIB & LZ4 (Optional, included via Git submodules for standalone builds).
    - GoogleTest & Google Benchmark (Included via Git submodules).

## Integration

### Via CMake (Recommended)

Add MicroVTK as a submodule:
```bash
git submodule add https://github.com/liudss/microvtk.git external/microvtk
```

Then in your `CMakeLists.txt`:
```cmake
add_subdirectory(external/microvtk)
target_link_libraries(your_app PRIVATE microvtk::microvtk)
```

## Quick Start

### Basic VTU Writing (Streaming Mode)

```cpp
#include <microvtk/microvtk.hpp>
#include <vector>

using namespace microvtk;

int main() {
    VtuWriter writer;

    // Data is referenced (not copied) until writer.write() is called
    std::vector<double> points = {0,0,0, 1,0,0, 0,1,0};
    writer.setPoints(points);

    std::vector<int32_t> conn = {0, 1, 2}, offsets = {3};
    std::vector<uint8_t> types = {static_cast<uint8_t>(CellType::Triangle)};
    writer.setCells(conn, offsets, types);

    writer.write("output.vtu");
    return 0;
}
```

### Compression

```cpp
writer.setCompression(core::CompressionType::LZ4);
writer.write("compressed_output.vtu");
```

### Time-Series (PVD)

```cpp
PvdWriter pvd("simulation.pvd");
pvd.addStep(0.0, "step_0.vtu");
pvd.addStep(0.1, "step_1.vtu");
pvd.save(); // Explicit save for performance in loops
```

## Benchmarks

MicroVTK is designed for extreme performance in simulation contexts:
- **Registration**: O(1) pointer-based data registration.
- **Writing**: Efficiently handles millions of points with minimal CPU overhead.
- **LZ4**: Often results in *faster* wall-clock write times than uncompressed I/O due to reduced data volume.

## Acknowledgments

MicroVTK utilizes the following excellent open-source libraries for its optional compression and testing features:

- **[ZLIB](https://zlib.net/)**: A massive thank you to Jean-loup Gailly and Mark Adler for providing the gold standard of compression.
- **[LZ4](https://lz4.org/)**: Thanks to Yann Collet for the extremely fast compression algorithm that enables high-performance visualization.
- **[GoogleTest](https://github.com/google/googletest)** and **[Google Benchmark](https://github.com/google/benchmark)** for the robust testing and performance measurement infrastructure.

## License

MIT License
