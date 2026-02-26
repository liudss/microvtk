#include <filesystem>
#include <gtest/gtest.h>
#include <microvtk/vtu_writer.hpp>
#include <vector>

using namespace microvtk;

TEST(VtuWriter, SimpleWrite) {
  VtuWriter writer;

  // Points (Triangle)
  std::vector<double> points = {0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
  writer.setPoints(points);

  // Cell (1 Triangle)
  std::vector<int32_t> conn = {0, 1, 2};
  std::vector<int32_t> offsets = {3};
  std::vector<uint8_t> types = {static_cast<uint8_t>(CellType::Triangle)};

  writer.setCells(conn, offsets, types);

  // Point Data
  std::vector<float> pointData = {1.1F, 2.2F, 3.3F};
  writer.addPointData("ScalarField", pointData);

  // Write
  std::string filename = "test_output.vtu";
  writer.write(filename);

  // Verify file exists and has some content
  ASSERT_TRUE(std::filesystem::exists(filename));

  std::ifstream ifs(filename);
  std::string content((std::istreambuf_iterator<char>(ifs)),
                      (std::istreambuf_iterator<char>()));

  EXPECT_TRUE(content.find("<VTKFile") != std::string::npos);
  EXPECT_TRUE(content.find("AppendedData") != std::string::npos);
  EXPECT_TRUE(content.find("ScalarField") != std::string::npos);

  // Cleanup
  std::filesystem::remove(filename);
}

TEST(VtuWriter, OrderIndependence) {
  VtuWriter writer;
  writer.setCompression(microvtk::core::CompressionType::LZ4);

  // Call Point Data BEFORE setPoints
  std::vector<float> pointData = {1.1F, 2.2F, 3.3F};
  writer.addPointData("ScalarField", pointData);

  // Call setPoints
  std::vector<double> points = {0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
  writer.setPoints(points);

  // Cell (1 Triangle)
  std::vector<int32_t> conn = {0, 1, 2};
  std::vector<int32_t> offsets = {3};
  std::vector<uint8_t> types = {static_cast<uint8_t>(CellType::Triangle)};
  writer.setCells(conn, offsets, types);

  std::string filename = "test_order.vtu";
  writer.write(filename);

  ASSERT_TRUE(std::filesystem::exists(filename));

  std::ifstream ifs(filename);
  std::string content((std::istreambuf_iterator<char>(ifs)),
                      (std::istreambuf_iterator<char>()));

  // Extract offsets for Points and ScalarField
  auto findOffset = [&](std::string_view name) -> std::string {
    size_t pos = content.find(name);
    if (pos == std::string::npos) return "";
    size_t offsetPos = content.find("offset=\"", pos);
    if (offsetPos == std::string::npos) return "";
    size_t start = offsetPos + 8;
    size_t end = content.find("\"", start);
    return content.substr(start, end - start);
  };

  std::string pointsOffset = findOffset("Points");
  std::string fieldOffset = findOffset("ScalarField");

  EXPECT_FALSE(pointsOffset.empty());
  EXPECT_FALSE(fieldOffset.empty());

  // With logical ordering: Points (first) should have offset 0
  // ScalarField should have a non-zero offset because it comes after Points and
  // Cells
  EXPECT_EQ(pointsOffset, "0");
  EXPECT_NE(fieldOffset, "0");
  EXPECT_NE(pointsOffset, fieldOffset);

  std::filesystem::remove(filename);
}
