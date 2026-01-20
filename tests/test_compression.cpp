#include <filesystem>
#include <gtest/gtest.h>
#include <microvtk/core/compressor.hpp>
#include <microvtk/vtu_writer.hpp>
#include <numeric>
#include <vector>

using namespace microvtk;

class CompressionTest : public ::testing::TestWithParam<core::CompressionType> {
protected:
  void SetUp() override {
    // Generate some data
    points.resize(300);  // 100 points
    std::iota(points.begin(), points.end(), 0.0);
  }

  std::vector<double> points;
};

TEST_P(CompressionTest, WriteCompressed) {
  core::CompressionType type = GetParam();

  // Check if compressor is available
  if (!core::createCompressor(type) && type != core::CompressionType::None) {
    GTEST_SKIP() << "Compressor not compiled in.";
  }

  VtuWriter writer;
  writer.setCompression(type);
  writer.setPoints(points);

  std::string filename =
      "test_compress_" + std::to_string(static_cast<int>(type)) + ".vtu";
  writer.write(filename);

  ASSERT_TRUE(std::filesystem::exists(filename));

  uint64_t fileSize = std::filesystem::file_size(filename);
  EXPECT_GT(fileSize, 0);

  // Cleanup
  // std::filesystem::remove(filename);
}

INSTANTIATE_TEST_SUITE_P(CompressionSchemes, CompressionTest,
                         ::testing::Values(core::CompressionType::None,
                                           core::CompressionType::ZLib,
                                           core::CompressionType::LZ4));
