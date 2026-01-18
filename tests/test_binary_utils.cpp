#include <gtest/gtest.h>
#include <microvtk/core/binary_utils.hpp>
#include <vector>

using namespace microvtk::core;

TEST(BinaryUtils, WriteLE) {
  std::vector<uint8_t> buffer;
  uint32_t val = 0x12345678;
  write_le(val, buffer);

  // Expect Little Endian: 78 56 34 12
  ASSERT_EQ(buffer.size(), 4);
  EXPECT_EQ(buffer[0], 0x78);
  EXPECT_EQ(buffer[1], 0x56);
  EXPECT_EQ(buffer[2], 0x34);
  EXPECT_EQ(buffer[3], 0x12);
}

TEST(BinaryUtils, AppendData) {
  std::vector<uint8_t> buffer;
  std::vector<int16_t> data = {0x1234, 0x5678};
  // 0x1234 -> 34 12
  // 0x5678 -> 78 56

  append_data(std::span(data), buffer);

  ASSERT_EQ(buffer.size(), 4);
  EXPECT_EQ(buffer[0], 0x34);
  EXPECT_EQ(buffer[1], 0x12);
  EXPECT_EQ(buffer[2], 0x78);
  EXPECT_EQ(buffer[3], 0x56);
}

TEST(BinaryUtils, Base64Encode) {
  std::string text = "Hello";
  std::vector<uint8_t> data(text.begin(), text.end());
  // "Hello" -> "SGVsbG8="
  EXPECT_EQ(base64_encode(data), "SGVsbG8=");

  std::string text2 = "Mic";
  std::vector<uint8_t> data2(text2.begin(), text2.end());
  // "Mic" -> "TWlj"
  EXPECT_EQ(base64_encode(data2), "TWlj");
}
