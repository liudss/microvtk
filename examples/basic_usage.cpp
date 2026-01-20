#include <array>
#include <iostream>
#include <microvtk/microvtk.hpp>
#include <vector>

using namespace microvtk;

struct Particle {
  double mass;
  std::array<double, 3> vel;
};

int main() {
  try {
    // 1. Setup Writer
    VtuWriter writer(DataFormat::Appended);

    // 2. Data (Zero Copy)
    // Create a simple tetra: 4 points
    std::vector<double> points = {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
    writer.setPoints(points);

    // 3. Topology
    std::vector<int32_t> conn = {0, 1, 2, 3};
    std::vector<int32_t> offsets = {4};
    std::vector<uint8_t> types = {static_cast<uint8_t>(CellType::Tetra)};
    writer.setCells(conn, offsets, types);

    // 4. Attributes (using Adapters)
    std::vector<Particle> parts(4);
    for (int i = 0; i < 4; ++i) {
      parts[i].mass = 1.0 + i;
      parts[i].vel[0] = i * 0.1;
      parts[i].vel[1] = i * 0.2;
      parts[i].vel[2] = i * 0.3;
    }

    // Writes 1 component array
    writer.addPointData("Mass", adapt(parts, &Particle::mass));

    // 5. Write to disk
    writer.write("example.vtu");
  } catch (const std::exception& e) {
    return 1;
  }

  return 0;
}
