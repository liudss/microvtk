#include <gtest/gtest.h>
#include <filesystem>
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

TEST(VtuWriter, RejectsIncompletePointTuples) {
  VtuWriter writer;
  std::vector<double> points = {0.0, 1.0, 2.0, 3.0, 4.0};

  EXPECT_THROW(writer.setPoints(points, 3), std::invalid_argument);
  EXPECT_THROW(writer.setPoints(points, 2), std::invalid_argument);
}

TEST(VtuWriter, RejectsInvalidCellOffsets) {
  VtuWriter writer;
  std::vector<double> points = {0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
  writer.setPoints(points);

  std::vector<int32_t> conn = {0, 1, 2};
  std::vector<int32_t> non_monotonic_offsets = {3, 2};
  std::vector<uint8_t> two_types = {3, 3};
  EXPECT_THROW(writer.setCells(conn, non_monotonic_offsets, two_types),
               std::invalid_argument);

  std::vector<int32_t> wrong_final_offset = {2};
  std::vector<uint8_t> one_type = {3};
  EXPECT_THROW(writer.setCells(conn, wrong_final_offset, one_type),
               std::invalid_argument);

  std::vector<int32_t> no_offsets = {};
  std::vector<uint8_t> no_types = {};
  EXPECT_THROW(writer.setCells(conn, no_offsets, no_types),
               std::invalid_argument);
}

TEST(VtuWriter, RejectsConnectivityIndexOutOfRange) {
  VtuWriter writer;
  std::vector<double> points = {0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
  writer.setPoints(points);

  std::vector<int32_t> conn = {0, 1, 3};
  std::vector<int32_t> offsets = {3};
  std::vector<uint8_t> types = {3};

  EXPECT_THROW(writer.setCells(conn, offsets, types), std::invalid_argument);
}

TEST(VtuWriter, ThrowsWhenOutputCannotBeOpened) {
  VtuWriter writer;
  std::vector<double> points = {0.0, 0.0, 0.0};
  writer.setPoints(points);

  const std::filesystem::path dir = "vtu_writer_output_directory";
  std::filesystem::create_directory(dir);
  EXPECT_THROW(writer.write(dir.string()), std::runtime_error);
  std::filesystem::remove(dir);
}
