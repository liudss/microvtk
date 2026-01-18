#include <gtest/gtest.h>
#include <microvtk/microvtk.hpp>

TEST(MicroVTK, Initialization) {
  // Basic test to verify include paths and linking
  EXPECT_TRUE(true);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
