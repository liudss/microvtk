#include <gtest/gtest.h>
#include <list>
#include <deque>
#include <sstream>
#include <microvtk/core/data_accessor.hpp>
#include <microvtk/core/binary_utils.hpp>

using namespace microvtk::core;

TEST(DataAccessor, NonContiguousContainer) {
    // Test std::list which is definitely not contiguous_range
    std::list<int32_t> data = {1, 2, 3, 4, 5};
    RangeAccessor<std::list<int32_t>> accessor(data);
    
    std::stringstream ss;
    accessor.write_to_stream(ss);
    
    std::string output = ss.str();
    EXPECT_EQ(output.size(), 5 * sizeof(int32_t));
    
    // Check values (Little Endian assumed for this test environment)
    const int32_t* ptr = reinterpret_cast<const int32_t*>(output.data());
    EXPECT_EQ(ptr[0], 1);
    EXPECT_EQ(ptr[1], 2);
    EXPECT_EQ(ptr[2], 3);
    EXPECT_EQ(ptr[3], 4);
    EXPECT_EQ(ptr[4], 5);
}

TEST(DataAccessor, LegacyWriteTo) {
    // Cover the write_to method for backward compatibility/completeness
    std::vector<int32_t> data = {10, 20};
    RangeAccessor<std::vector<int32_t>> accessor(data);
    
    std::vector<uint8_t> buffer;
    accessor.write_to(buffer);
    
    EXPECT_EQ(buffer.size(), 2 * sizeof(int32_t));
    const int32_t* ptr = reinterpret_cast<const int32_t*>(buffer.data());
    EXPECT_EQ(ptr[0], 10);
    EXPECT_EQ(ptr[1], 20);
}

TEST(BinaryUtils, Base64Padding) {
    // Case 1: No padding (size % 3 == 0)
    std::vector<uint8_t> data1 = {'M', 'a', 'n'}; // "TWFu"
    EXPECT_EQ(base64_encode(data1), "TWFu");

    // Case 2: 1 padding char (size % 3 == 2)
    std::vector<uint8_t> data2 = {'M', 'a'}; // "TWE="
    EXPECT_EQ(base64_encode(data2), "TWE=");

    // Case 3: 2 padding chars (size % 3 == 1)
    std::vector<uint8_t> data3 = {'M'}; // "TQ=="
    EXPECT_EQ(base64_encode(data3), "TQ==");
    
    // Case 4: Empty
    std::vector<uint8_t> data4 = {};
    EXPECT_EQ(base64_encode(data4), "");
}
