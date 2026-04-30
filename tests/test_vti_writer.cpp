#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <microvtk/adapter.hpp>
#include <microvtk/vti_writer.hpp>
#include <stdexcept>
#include <string>
#include <vector>

using namespace microvtk;

TEST(VtiWriter, SimpleWrite) {
  // Define a 2x2x1 grid
  std::array<int, 6> extent = {0, 1, 0, 1, 0, 0};
  // 4 points: (0,0,0), (1,0,0), (0,1,0), (1,1,0)
  // 1 cell (pixel)

  VtiWriter writer(extent);

  // Point Data (4 values)
  std::vector<double> pData = {1.0, 2.0, 3.0, 4.0};
  writer.addPointData("PointScalars", pData);

  // Cell Data (1 value)
  std::vector<int> cData = {100};
  writer.addCellData("CellID", cData);

  // Write
  std::string filename = "test_image.vti";
  writer.write(filename);

  // Verify file exists
  ASSERT_TRUE(std::filesystem::exists(filename));

  {
    std::ifstream ifs(filename);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        (std::istreambuf_iterator<char>()));

    EXPECT_TRUE(content.find("<VTKFile") != std::string::npos);
    EXPECT_TRUE(content.find("type=\"ImageData\"") != std::string::npos);
    EXPECT_TRUE(content.find("WholeExtent=\"0 1 0 1 0 0\"") !=
                std::string::npos);
    EXPECT_TRUE(content.find("AppendedData") != std::string::npos);
    EXPECT_TRUE(content.find("PointScalars") != std::string::npos);
    EXPECT_TRUE(content.find("CellID") != std::string::npos);
  }

  // Cleanup
  std::filesystem::remove(filename);
}

TEST(VtiWriter, Compression) {
  std::array<int, 6> extent = {0, 10, 0, 10, 0, 10};  // 11x11x11 points
  VtiWriter writer(extent);
  writer.setCompression(
      core::CompressionType::ZLib);  // Assuming ZLib is enabled

  std::vector<float> data(static_cast<size_t>(11) * 11 * 11, 1.0F);
  writer.addPointData("DenseField", data);

  std::string filename = "test_image_compressed.vti";
  writer.write(filename);

  ASSERT_TRUE(std::filesystem::exists(filename));

  {
    std::ifstream ifs(filename);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        (std::istreambuf_iterator<char>()));

    EXPECT_TRUE(content.find("compressor=\"vtkZLibDataCompressor\"") !=
                std::string::npos);
  }

  std::filesystem::remove(filename);
}

struct CellProp {
  double density;
};

TEST(VtiWriter, WorksWithAoS) {
  // 2x2x1 grid -> 4 points, 1 cell
  std::array<int, 6> extent = {0, 1, 0, 1, 0, 0};
  VtiWriter writer(extent);

  // 1. AoS Data
  std::vector<CellProp> cells = {{1.25}};

  // 2. Use adapt
  // adapt returns a view, which is a valid range
  writer.addCellData("Density", adapt(cells, &CellProp::density));

  // 3. Write
  std::string filename = "test_vti_adapt.vti";
  writer.write(filename);

  ASSERT_TRUE(std::filesystem::exists(filename));

  // Cleanup
  std::filesystem::remove(filename);
}

TEST(VtiWriter, UncompressedAppendedDataUsesLogicalOrder) {
  std::array<int, 6> extent = {0, 1, 0, 1, 0, 0};
  VtiWriter writer(extent);

  std::vector<int> cData = {100};
  writer.addCellData("CellID", cData);

  std::vector<double> pData = {1.0, 2.0, 3.0, 4.0};
  writer.addPointData("PointScalars", pData);

  std::string filename = "test_vti_uncompressed_order.vti";
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
    EXPECT_EQ(dataSize, pData.size() * sizeof(double));
  }

  std::filesystem::remove(filename);
}

TEST(VtiWriter, DataSizeMismatch) {
  std::array<int, 6> extent = {0, 1, 0, 1, 0, 0};  // 4 points, 1 cell

  VtiWriter writer(extent);
  std::vector<double> pointData = {1.0, 2.0, 3.0};
  writer.addPointData("bad_points", pointData);
  EXPECT_THROW(writer.write("should_fail_points.vti"), std::invalid_argument);

  VtiWriter writer2(extent);
  std::vector<int> cellData = {1, 2};
  writer2.addCellData("bad_cells", cellData);
  EXPECT_THROW(writer2.write("should_fail_cells.vti"), std::invalid_argument);
}

TEST(VtiWriter, RejectsInvalidExtentAtConstruction) {
  const std::array<int, 6> invalidExtent = {1, 0, 0, 0, 0, 0};

  EXPECT_THROW(VtiWriter writer(invalidExtent), std::invalid_argument);
}

TEST(VtiWriter, ThrowsWhenOutputCannotBeOpened) {
  std::array<int, 6> extent = {0, 0, 0, 0, 0, 0};
  VtiWriter writer(extent);
  std::vector<double> pointData = {1.0};
  writer.addPointData("points", pointData);

  const std::filesystem::path dir = "vti_writer_output_directory";
  std::filesystem::create_directory(dir);
  EXPECT_THROW(writer.write(dir.string()), std::runtime_error);
  std::filesystem::remove(dir);
}
