#include <gtest/gtest.h>
#include <microvtk/adapter.hpp>
#include <microvtk/core/binary_utils.hpp>
#include <vector>

using namespace microvtk;

TEST(Adapter, ViewVector) {
  std::vector<int> v = {1, 2, 3};
  auto s = view(v);
  EXPECT_EQ(s.size(), 3);
  EXPECT_EQ(s[0], 1);

  // Check if it works with append_range
  std::vector<uint8_t> buffer;
  core::append_range(s, buffer);
  EXPECT_EQ(buffer.size(), 3 * sizeof(int));
}

struct Particle {
  double mass;
  int id;
};

TEST(Adapter, AdaptAoS) {
  std::vector<Particle> particles = {{.mass = 1.0, .id = 10},
                                     {.mass = 2.0, .id = 20}};

  // Adapt mass
  auto masses = adapt(particles, &Particle::mass);

  // Check values
  auto it = masses.begin();
  EXPECT_DOUBLE_EQ(*it, 1.0);
  EXPECT_DOUBLE_EQ(*(++it), 2.0);

  // Check appending
  std::vector<uint8_t> buffer;
  core::append_range(masses, buffer);
  // 2 doubles = 16 bytes
  EXPECT_EQ(buffer.size(), 16);
}
