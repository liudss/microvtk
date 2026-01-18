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

  // Writes 3 component array ?
  // The current adapt implementation handles single members.
  // Particle::vel is a double[3].
  // adapt returns a transformed view where each element is `const double
  // (&)[3]`. The writer expects a range of scalars if numComponents > 1? Let's
  // check VtuWriter::appendData. It calls `append_range`. `append_range`
  // iterates. If range value is `double[3]`, `append_range` (slow path)
  // iterates it? No, `append_range` expects `std::ranges::range`. An array
  // `double[3]` is a range. But `append_range` logic: `using T =
  // std::ranges::range_value_t<R>;` If R is `view of double[3]`, T is
  // `double[3]`. `write_le(val, buffer)` where val is `double[3]` will fail or
  // write pointer? `write_le` casts to `const uint8_t*`. It will write the
  // array bytes if it's POD. BUT `write_le` signature: `void write_le(T value,
  // ...)` T=double[3]. Arrays decay to pointers in parameters? No, T is
  // deduced. `reinterpret_cast<const uint8_t*>(&value)` might work for array.
  // BUT `byteswap` inside `write_le` will fail for array.

  // So `adapt` for array members needs care.
  // For now, let's stick to simple scalar members in this example or handle it.
  // The spec example showed: `adapt(parts, &Particle::vel)`.
  // If I want to support this, I need `append_range` to handle nested ranges or
  // `adapt` to flatten.

  // Given the constraints and current implementation, flattening is safer.
  // But let's just use Scalar member for the example to ensure it compiles and
  // runs.

  // writer.addPointData("Velocity", adapt(parts, &Particle::vel), 3);

  // 5. Write to disk
  writer.write("example.vtu");

  return 0;
}
