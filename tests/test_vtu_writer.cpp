#include <cstring>
#include <filesystem>
#include <fstream>
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

  {
    std::ifstream ifs(filename);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        (std::istreambuf_iterator<char>()));

    EXPECT_TRUE(content.find("<VTKFile") != std::string::npos);
    EXPECT_TRUE(content.find("AppendedData") != std::string::npos);
    EXPECT_TRUE(content.find("ScalarField") != std::string::npos);
  }

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

  {
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
    // ScalarField should have a non-zero offset because it comes after Points
    // and Cells
    EXPECT_EQ(pointsOffset, "0");
    EXPECT_NE(fieldOffset, "0");
    EXPECT_NE(pointsOffset, fieldOffset);
  }

  std::filesystem::remove(filename);
}

TEST(VtuWriter, UncompressedAppendedDataUsesLogicalOrder) {
  VtuWriter writer;

  std::vector<float> pointData = {1.1F, 2.2F, 3.3F};
  writer.addPointData("ScalarField", pointData);

  std::vector<double> points = {0.0, 0.0, 0.0, 1.0, 0.0,
                                0.0, 0.0, 1.0, 0.0};
  writer.setPoints(points);

  std::vector<int32_t> conn = {0, 1, 2};
  std::vector<int32_t> offsets = {3};
  std::vector<uint8_t> types = {static_cast<uint8_t>(CellType::Triangle)};
  writer.setCells(conn, offsets, types);

  std::string filename = "test_uncompressed_order.vtu";
  writer.write(filename);

  {
    std::ifstream ifs(filename, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        (std::istreambuf_iterator<char>()));

    size_t start = content.find(">_");
    ASSERT_NE(start, std::string::npos);
    start += 2;

    uint64_t dataSize = 0;
    std::memcpy(&dataSize, &content[start], sizeof(uint64_t));
    EXPECT_EQ(dataSize, points.size() * sizeof(double));
  }

  std::filesystem::remove(filename);
}

TEST(VtuWriter, AutoPadding) {
  // Test 1D padding
  {
    VtuWriter writer;
    std::vector<double> points1d = {1.0, 2.0, 3.0};  // 3 points in 1D
    writer.setPoints(points1d, 1);

    std::string filename = "test_padding_1d.vtu";
    writer.write(filename);

    {
      std::ifstream ifs(filename, std::ios::binary);
      std::string content((std::istreambuf_iterator<char>(ifs)),
                          (std::istreambuf_iterator<char>()));

      // Find the end of the XML part: ">_" starts the raw data
      size_t start = content.find(">_");
      ASSERT_NE(start, std::string::npos);
      start += 2;  // Skip ">_"

      // The first block is Points. VTK Appended format: [size_header][data]
      uint64_t dataSize;
      std::memcpy(&dataSize, &content[start], sizeof(uint64_t));
      EXPECT_EQ(dataSize, 3 * 3 * sizeof(double));

      const double* data =
          reinterpret_cast<const double*>(&content[start + sizeof(uint64_t)]);
      EXPECT_DOUBLE_EQ(data[0], 1.0);
      EXPECT_DOUBLE_EQ(data[1], 0.0);
      EXPECT_DOUBLE_EQ(data[2], 0.0);
      EXPECT_DOUBLE_EQ(data[3], 2.0);
      EXPECT_DOUBLE_EQ(data[4], 0.0);
      EXPECT_DOUBLE_EQ(data[5], 0.0);
      EXPECT_DOUBLE_EQ(data[6], 3.0);
      EXPECT_DOUBLE_EQ(data[7], 0.0);
      EXPECT_DOUBLE_EQ(data[8], 0.0);
    }

    std::filesystem::remove(filename);
  }
  // Test 2D padding
  {
    VtuWriter writer;
    std::vector<double> points2d = {1.0, 1.1, 2.0, 2.2};  // 2 points in 2D
    writer.setPoints(points2d, 2);

    std::string filename = "test_padding_2d.vtu";
    writer.write(filename);

    {
      std::ifstream ifs(filename, std::ios::binary);
      std::string content((std::istreambuf_iterator<char>(ifs)),
                          (std::istreambuf_iterator<char>()));

      size_t start = content.find(">_") + 2;
      const double* data =
          reinterpret_cast<const double*>(&content[start + sizeof(uint64_t)]);

      EXPECT_DOUBLE_EQ(data[0], 1.0);
      EXPECT_DOUBLE_EQ(data[1], 1.1);
      EXPECT_DOUBLE_EQ(data[2], 0.0);
      EXPECT_DOUBLE_EQ(data[3], 2.0);
      EXPECT_DOUBLE_EQ(data[4], 2.2);
      EXPECT_DOUBLE_EQ(data[5], 0.0);
    }

    std::filesystem::remove(filename);
  }

  // Test with a mock "Adapted" range (simulating Kokkos::View<double*[2]>)
  {
    VtuWriter writer;
    std::vector<double> raw = {10.0, 20.0};  // 1 point in 2D
    auto adapted = std::views::all(raw);
    writer.setPoints(adapted, 2);

    std::string filename = "test_padding_adapted.vtu";
    writer.write(filename);

    {
      std::ifstream ifs(filename, std::ios::binary);
      std::string content((std::istreambuf_iterator<char>(ifs)),
                          (std::istreambuf_iterator<char>()));

      size_t start = content.find(">_") + 2;
      const double* data =
          reinterpret_cast<const double*>(&content[start + sizeof(uint64_t)]);

      EXPECT_DOUBLE_EQ(data[0], 10.0);
      EXPECT_DOUBLE_EQ(data[1], 20.0);
      EXPECT_DOUBLE_EQ(data[2], 0.0);
    }

    std::filesystem::remove(filename);
  }
}
