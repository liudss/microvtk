#include <gtest/gtest.h>
#include <microvtk/vtu_writer.hpp>
#include <vector>
#include <stdexcept>

using namespace microvtk;

TEST(VtuWriter, CellsSizeMismatch) {
  VtuWriter writer;
  
  std::vector<int32_t> conn = {0, 1, 2};
  std::vector<int32_t> offsets = {3}; // 1 cell
  std::vector<uint8_t> types = {};    // 0 types (Mismatch!)

  EXPECT_THROW(writer.setCells(conn, offsets, types), std::invalid_argument);
}
