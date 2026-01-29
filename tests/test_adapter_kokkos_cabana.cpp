#include <gtest/gtest.h>
#include <microvtk/microvtk.hpp>
#include <vector>

#if defined(MICROVTK_HAS_KOKKOS) || defined(MICROVTK_HAS_CABANA)
#include <Kokkos_Core.hpp>

class KokkosEnv : public ::testing::Environment {
public:
  void SetUp() override {
    if (!Kokkos::is_initialized()) {
      Kokkos::initialize();
    }
  }
  void TearDown() override {
    if (Kokkos::is_initialized()) {
      Kokkos::finalize();
    }
  }
};

// Register the environment
::testing::Environment* const kokkos_env =
    ::testing::AddGlobalTestEnvironment(new KokkosEnv);
#endif

#ifdef MICROVTK_HAS_KOKKOS
TEST(KokkosAdapterTest, AdaptView1D) {
  Kokkos::View<double*, Kokkos::HostSpace> view("v", 10);
  for (int i = 0; i < 10; ++i) {
    view(i) = static_cast<double>(i);
  }

  auto span = microvtk::adapt(view);
  ASSERT_EQ(span.size(), 10);
  EXPECT_DOUBLE_EQ(span[0], 0.0);
  EXPECT_DOUBLE_EQ(span[9], 9.0);
}

TEST(KokkosAdapterTest, AdaptView2D) {
  // 5 points, 3 components, contiguous layout
  Kokkos::View<double* [3], Kokkos::LayoutRight, Kokkos::HostSpace> view("v",
                                                                         5);
  for (int i = 0; i < 5; ++i) {
    view(i, 0) = i * 3 + 0;
    view(i, 1) = i * 3 + 1;
    view(i, 2) = i * 3 + 2;
  }

  auto span = microvtk::adapt(view);
  ASSERT_EQ(span.size(), 15);
  EXPECT_DOUBLE_EQ(span[0], 0.0);
  EXPECT_DOUBLE_EQ(span[1], 1.0);
  EXPECT_DOUBLE_EQ(span[2], 2.0);
  EXPECT_DOUBLE_EQ(span[14], 14.0);
}

TEST(KokkosAdapterTest, AdaptView2D_LayoutLeft) {
  // 5 points, 3 components, LayoutLeft (Column Major)
  // Memory: x0, x1... y0, y1...
  // Logical: (x0, y0, z0), (x1, y1, z1)...
  Kokkos::View<double* [3], Kokkos::LayoutLeft, Kokkos::HostSpace> view("v_ll",
                                                                        5);
  for (int i = 0; i < 5; ++i) {
    view(i, 0) = i * 10.0 + 1.0;
    view(i, 1) = i * 10.0 + 2.0;
    view(i, 2) = i * 10.0 + 3.0;
  }

  auto range = microvtk::adapt(view);
  ASSERT_EQ(std::ranges::size(range), 15);

  // Use iterator to verify order
  auto it = std::ranges::begin(range);
  // Point 0: (1, 2, 3)
  EXPECT_DOUBLE_EQ(*it++, 1.0);
  EXPECT_DOUBLE_EQ(*it++, 2.0);
  EXPECT_DOUBLE_EQ(*it++, 3.0);
  // Point 1: (11, 12, 13)
  EXPECT_DOUBLE_EQ(*it++, 11.0);
  EXPECT_DOUBLE_EQ(*it++, 12.0);
  EXPECT_DOUBLE_EQ(*it++, 13.0);
}

TEST(KokkosAdapterTest, AdaptView2D_LayoutStride) {
  // Create a strided view via subview
  Kokkos::View<double* [3], Kokkos::LayoutRight, Kokkos::HostSpace> orig(
      "v_orig", 10);
  for (int i = 0; i < 10; ++i) {
    orig(i, 0) = i;
    orig(i, 1) = i + 0.1;
    orig(i, 2) = i + 0.2;
  }

  // Take a subview: rows 2 to 5 (exclusive) -> 3 rows
  auto sub = Kokkos::subview(orig, std::make_pair(2, 5), Kokkos::ALL);

  auto range = microvtk::adapt(sub);
  ASSERT_EQ(std::ranges::size(range), 3 * 3);

  auto it = std::ranges::begin(range);
  // Row 2
  EXPECT_DOUBLE_EQ(*it++, 2.0);
  EXPECT_DOUBLE_EQ(*it++, 2.1);
  EXPECT_DOUBLE_EQ(*it++, 2.2);
  // Row 3
  EXPECT_DOUBLE_EQ(*it++, 3.0);
}

TEST(KokkosAdapterTest, AdaptViewRank3LayoutLeft) {
  // Rank 3, LayoutLeft (Column Major in 3D)
  // (N, 3, 3)
  Kokkos::View<double* [3][3], Kokkos::LayoutLeft, Kokkos::HostSpace> view("v3",
                                                                           5);

  // Fill data to verify logical order
  // Logical: Tuple i, Comp (r, c)
  // Value = i * 100 + r * 10 + c
  for (int i = 0; i < 5; ++i) {
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) {
        view(i, r, c) = i * 100.0 + r * 10.0 + c;
      }
    }
  }

  auto range = microvtk::adapt(view);
  EXPECT_EQ(std::ranges::size(range), 5 * 9);

  // Verify values in logical Row-Major order
  auto it = std::ranges::begin(range);
  for (int i = 0; i < 5; ++i) {
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) {
        EXPECT_DOUBLE_EQ(*it++, i * 100.0 + r * 10.0 + c);
      }
    }
  }
}
#endif

#ifdef MICROVTK_HAS_CABANA
#include <Cabana_Core.hpp>

TEST(CabanaAdapterTest, AdaptSliceScalar) {
  using DataTypes = Cabana::MemberTypes<double>;
  const int num_tuples = 10;
  Cabana::AoSoA<DataTypes, Kokkos::HostSpace> aosoa("aosoa", num_tuples);
  auto slice = Cabana::slice<0>(aosoa);

  for (int i = 0; i < num_tuples; ++i) {
    slice(i) = static_cast<double>(i * 1.5);
  }

  auto flattened = microvtk::adapt(slice);
  ASSERT_EQ(flattened.size(), num_tuples);

  auto it = flattened.begin();
  EXPECT_DOUBLE_EQ(it[0], 0.0);
  EXPECT_DOUBLE_EQ(it[1], 1.5);
  EXPECT_DOUBLE_EQ(it[9], 9 * 1.5);
}

TEST(CabanaAdapterTest, AdaptSliceArray) {
  using DataTypes = Cabana::MemberTypes<double[3]>;
  const int num_tuples = 5;
  Cabana::AoSoA<DataTypes, Kokkos::HostSpace> aosoa("aosoa", num_tuples);
  auto slice = Cabana::slice<0>(aosoa);

  for (int i = 0; i < num_tuples; ++i) {
    slice(i, 0) = i * 10.0 + 1.0;
    slice(i, 1) = i * 10.0 + 2.0;
    slice(i, 2) = i * 10.0 + 3.0;
  }

  auto flattened = microvtk::adapt(slice);
  ASSERT_EQ(flattened.size(), num_tuples * 3);

  auto it = flattened.begin();
  // 0th point
  EXPECT_DOUBLE_EQ(it[0], 1.0);
  EXPECT_DOUBLE_EQ(it[1], 2.0);
  EXPECT_DOUBLE_EQ(it[2], 3.0);

  // 1st point (flat index 3, 4, 5)
  EXPECT_DOUBLE_EQ(it[3], 11.0);
}

TEST(CabanaAdapterTest, AdaptSliceMultiDimArray) {
  // Tensor 3x3
  using DataTypes = Cabana::MemberTypes<double[3][3]>;
  const int num_tuples = 5;
  Cabana::AoSoA<DataTypes, Kokkos::HostSpace> aosoa("aosoa", num_tuples);
  auto slice = Cabana::slice<0>(aosoa);

  for (int i = 0; i < num_tuples; ++i) {
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) {
        slice(i, r, c) = i * 100.0 + r * 10.0 + c;
      }
    }
  }

  auto flattened = microvtk::adapt(slice);

  // Check size: 5 tuples * 9 components = 45
  EXPECT_EQ(flattened.size(), 45);

  auto it = flattened.begin();
  for (int i = 0; i < num_tuples; ++i) {
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) {
        EXPECT_DOUBLE_EQ(*it++, i * 100.0 + r * 10.0 + c);
      }
    }
  }
}
#endif
