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
#endif
