#include <gtest/gtest.h>
#include <microvtk/vtu_writer.hpp>
#include <stdexcept>
#include <vector>

using namespace microvtk;

TEST(VtuWriter, CellsSizeMismatch) {
  VtuWriter writer;

  std::vector<int32_t> conn = {0, 1, 2};
  std::vector<int32_t> offsets = {3};  // 1 cell
  std::vector<uint8_t> types = {};     // 0 types (Mismatch!)

  EXPECT_THROW(writer.setCells(conn, offsets, types), std::invalid_argument);
}

TEST(VtuWriter, DataSizeMismatch) {
  VtuWriter writer;

  // 2 points
  std::vector<double> points = {0, 0, 0, 1, 0, 0};
  writer.setPoints(points);

  // Add data for 3 points (Mismatch!)
  std::vector<double> data3 = {1, 2, 3};
  writer.addPointData("bad", data3);
  EXPECT_THROW(writer.write("should_fail.vtu"), std::invalid_argument);

  VtuWriter writer2;
  writer2.setPoints(points);

  // 1 cell
  std::vector<int32_t> conn = {0, 1};
  std::vector<int32_t> offsets = {2};
  std::vector<uint8_t> types = {3};  // Polyline
  writer2.setCells(conn, offsets, types);

  // Add cell data for 2 cells (Mismatch!)
  std::vector<double> cell_data2 = {1, 2};
  writer2.addCellData("bad_cell", cell_data2);
  EXPECT_THROW(writer2.write("should_fail_cell.vtu"), std::invalid_argument);
}
