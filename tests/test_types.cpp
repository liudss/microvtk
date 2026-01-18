#include <gtest/gtest.h>
#include <microvtk/common/types.hpp>

TEST(CommonTypes, EnumValues) {
  using namespace microvtk;
  EXPECT_EQ(static_cast<int>(CellType::Tetra), 10);
  EXPECT_EQ(static_cast<int>(CellType::Hexahedron), 12);
  EXPECT_EQ(static_cast<int>(CellType::Triangle), 5);
}
