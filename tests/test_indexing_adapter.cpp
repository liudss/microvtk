#include <gtest/gtest.h>
#include <microvtk/adapter.hpp>
#include <microvtk/indexing_adapter.hpp>
#include <numeric>
#include <vector>

namespace {

// Helper to manually calculate simple 2D/3D morton for small integers
uint64_t simple_morton(uint32_t x, uint32_t y, uint32_t z) {
  uint64_t answer = 0;
  for (int i = 0; i < 21; ++i) {
    answer |= ((x & (1U << i)) << (2 * i));
    answer |= ((y & (1U << i)) << (2 * i + 1));
    answer |= ((z & (1U << i)) << (2 * i + 2));
  }
  return answer;
}

}  // namespace

TEST(IndexingAdapter, MortonEncodingCorrectness) {
  // Check known values
  EXPECT_EQ(microvtk::detail::morton_encode_3d(0, 0, 0), 0);
  EXPECT_EQ(microvtk::detail::morton_encode_3d(1, 0, 0), 1);  // 001
  EXPECT_EQ(microvtk::detail::morton_encode_3d(0, 1, 0), 2);  // 010
  EXPECT_EQ(microvtk::detail::morton_encode_3d(1, 1, 0), 3);  // 011
  EXPECT_EQ(microvtk::detail::morton_encode_3d(0, 0, 1), 4);  // 100
  EXPECT_EQ(microvtk::detail::morton_encode_3d(1, 1, 1), 7);  // 111

  // Check larger value (x=2) -> 10 binary -> shifts to 100 binary in x-slot?
  // x=2 (10), y=0, z=0.
  // split_by_3(2) -> bit 1 moves to pos 3?
  // ... c b a -> ... c00 b00 a00
  // bit 0 (value 1) -> pos 0
  // bit 1 (value 2) -> pos 3 (value 8)
  EXPECT_EQ(microvtk::detail::morton_encode_3d(2, 0, 0), 8);

  // Randomized / Exhaustive check for small range against reference
  // implementation
  for (uint32_t z = 0; z < 5; ++z) {
    for (uint32_t y = 0; y < 5; ++y) {
      for (uint32_t x = 0; x < 5; ++x) {
        EXPECT_EQ(microvtk::detail::morton_encode_3d(x, y, z),
                  simple_morton(x, y, z));
      }
    }
  }
}

TEST(IndexingAdapter, ViewReorderingMismatch) {
  // Case: 4x1x1 grid.
  // Raster: 0, 1, 2, 3
  // Coords: (0,0,0), (1,0,0), (2,0,0), (3,0,0)
  // Morton: 0, 1, 8, 9
  // Note: 2 (binary 10) -> encoded 1000 (8). 3 (binary 11) -> encoded 1001 (9).

  // User data storage (Morton order).
  // We need to size the vector large enough to hold the max morton code.
  // For 4x1x1, max coord is (3,0,0). Morton(3,0,0) = 9. Size needs to be 10.
  std::vector<int> storage(16, -1);

  // Fill specific slots that will be accessed
  storage[0] = 100;  // (0,0,0)
  storage[1] = 101;  // (1,0,0)
  storage[8] = 102;  // (2,0,0)
  storage[9] = 103;  // (3,0,0)

  std::array<size_t, 3> dims = {4, 1, 1};

  auto view = storage | microvtk::views::reorder_z_curve(dims);

  // Verify view size
  EXPECT_EQ(std::ranges::distance(view), 4);  // Should only expose 4 elements

  // Verify access in Raster order (0, 1, 2, 3)
  auto it = view.begin();
  EXPECT_EQ(*it++, 100);
  EXPECT_EQ(*it++, 101);
  EXPECT_EQ(*it++, 102);
  EXPECT_EQ(*it++, 103);
}

TEST(IndexingAdapter, View2x2x2) {
  // 2x2x2 Grid.
  // Total 8 elements.
  // Raster: (0,0,0)...(1,1,1). Indices 0..7
  // Morton: 0..7 (Coincidentally same for 2x2x2)

  std::vector<int> storage(8);
  std::iota(storage.begin(), storage.end(), 10);  // 10, 11, ... 17

  std::array<size_t, 3> dims = {2, 2, 2};
  auto view = storage | microvtk::views::reorder_z_curve(dims);

  int expected = 10;
  for (auto val : view) {
    EXPECT_EQ(val, expected++);
  }
}

TEST(IndexingAdapter, View3x3_SparseAccess) {
  // 3x3x1 Grid.
  // Max coord (2,2,0). Morton(2,2,0) -> x=2(1000), y=2(10000) -> 11000?
  // x=2 (bit 1->pos 3=8). y=2 (bit 1->pos 4=16). Sum = 24.
  // Storage needs to be at least size 25.

  std::vector<int> storage(32, 0);

  // Populate storage at Morton indices corresponding to raster coordinates
  // Raster (0,0) -> M(0)
  // Raster (1,0) -> M(1)
  // Raster (2,0) -> M(8)
  // Raster (0,1) -> M(2)
  // Raster (1,1) -> M(3)
  // Raster (2,1) -> M(10)
  // Raster (0,2) -> M(16)
  // Raster (1,2) -> M(17)
  // Raster (2,2) -> M(24)

  std::vector<int> expected_values = {0, 1, 2, 3, 4, 5, 6, 7, 8};

  storage[0] = 0;
  storage[1] = 1;
  storage[8] = 2;
  storage[2] = 3;
  storage[3] = 4;
  storage[10] = 5;
  storage[16] = 6;
  storage[17] = 7;
  storage[24] = 8;

  std::array<size_t, 3> dims = {3, 3, 1};
  auto view = storage | microvtk::views::reorder_z_curve(dims);

  int idx = 0;
  for (auto val : view) {
    EXPECT_EQ(val, expected_values[idx++]);
  }
}

TEST(IndexingAdapter, CombinedWithAoS) {
  struct Particle {
    double dummy;
    double value;
  };

  // 2x2x2 Grid (8 elements)
  // Morton(0,0,0) = 0, Morton(1,1,1) = 7
  std::vector<Particle> storage(8);
  for (int i = 0; i < 8; ++i) {
    storage[i].value = static_cast<double>(i + 100);
  }

  std::array<size_t, 3> dims = {2, 2, 2};

  // Combine: Reorder sequence then extract member
  auto combined_view = storage | microvtk::views::reorder_z_curve(dims) |
                       microvtk::adapt(&Particle::value);

  static_assert(std::ranges::random_access_range<decltype(combined_view)>);

  double expected = 100.0;
  for (double val : combined_view) {
    EXPECT_DOUBLE_EQ(val, expected);
    expected += 1.0;
  }
}

TEST(IndexingAdapter, Morton2DCorrectness) {
  // 2D Check
  // (1,1) -> 11 binary
  // split(1) -> 1.  1 | 1<<1 = 3 (11 binary).
  EXPECT_EQ(microvtk::detail::morton_encode_2d(0, 0), 0);
  EXPECT_EQ(microvtk::detail::morton_encode_2d(1, 0), 1);  // 01
  EXPECT_EQ(microvtk::detail::morton_encode_2d(0, 1), 2);  // 10
  EXPECT_EQ(microvtk::detail::morton_encode_2d(1, 1), 3);  // 11
  EXPECT_EQ(microvtk::detail::morton_encode_2d(2, 0), 4);  // x=10 -> 100 (4)
  EXPECT_EQ(microvtk::detail::morton_encode_2d(2, 2),
            12);  // x=100, y=100 -> 1100 (12)
}

TEST(IndexingAdapter, View2DReordering) {
  // 2x2 Grid.
  // Raster: (0,0), (1,0), (0,1), (1,1) -> 0, 1, 2, 3
  // Morton: (0,0)->0, (1,0)->1, (0,1)->2, (1,1)->3
  // Coincidentally same.

  // 4x4 Grid.
  // Raster (row major): (0,0), (1,0), (2,0), (3,0) ...
  // Indices: 0, 1, 2, 3...
  // Morton codes for first row:
  // (0,0) -> 0
  // (1,0) -> 1
  // (2,0) -> 4  (10 -> 0100)
  // (3,0) -> 5  (11 -> 0101)

  std::vector<int> storage(16, 0);
  // Fill storage based on morton index
  storage[0] = 0;  // (0,0)
  storage[1] = 1;  // (1,0)
  storage[4] = 2;  // (2,0)
  storage[5] = 3;  // (3,0)

  std::array<size_t, 2> dims = {
      4, 1};  // 1D strip effectively, but treated as 2D slice
  // Wait, testing 2D view on 4x4
  dims = {4, 4};

  // Let's just test the first row access
  auto view = storage | microvtk::views::reorder_z_curve(dims);

  auto it = view.begin();
  EXPECT_EQ(*it++, 0);  // (0,0)
  EXPECT_EQ(*it++, 1);  // (1,0)
  EXPECT_EQ(*it++, 2);  // (2,0) -> accessed storage[4]
  EXPECT_EQ(*it++, 3);  // (3,0) -> accessed storage[5]
}
