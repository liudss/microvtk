#include <cmath>
#include <microvtk/microvtk.hpp>
#include <vector>

using namespace microvtk;

int main() {
  try {
    VtuWriter writer;

    // Enable LZ4 Compression
    // Requires MICROVTK_USE_LZ4 to be ON (default)
    writer.setCompression(core::CompressionType::LZ4);

    const int N = 1000;
    std::vector<double> points;
    points.reserve(N * 3);
    std::vector<double> data;
    data.reserve(N);

    // Generate a spiral
    for (int i = 0; i < N; ++i) {
      double t = i * 0.1;
      points.push_back(t * std::cos(t));  // X
      points.push_back(t * std::sin(t));  // Y
      points.push_back(t);                // Z
      data.push_back(std::sin(t));        // Scalar Data
    }

    writer.setPoints(points);

    // Topology: PolyLine (Single cell connecting all points)
    std::vector<int32_t> conn;
    conn.reserve(N);
    for (int i = 0; i < N; ++i) conn.push_back(i);

    std::vector<int32_t> offsets = {static_cast<int32_t>(N)};
    std::vector<uint8_t> types = {static_cast<uint8_t>(CellType::PolyLine)};

    writer.setCells(conn, offsets, types);
    writer.addPointData("SineWave", data);

    writer.write("compressed.vtu");
  } catch (...) {
    return 1;
  }
  return 0;
}
