#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace microvtk::core {

namespace internal {
// Simple byteswap for C++20 where std::byteswap might be missing
template <typename T>
T byteswap(T value) {
  auto* bytes = reinterpret_cast<uint8_t*>(&value);
  std::reverse(bytes, bytes + sizeof(T));
  return value;
}
}  // namespace internal

// Writes a value to the buffer in Little Endian format
template <typename T>
void write_le(T value, std::vector<uint8_t>& buffer) {
  if constexpr (std::endian::native == std::endian::big) {
    value = internal::byteswap(value);
  }
  const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
  buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
}

// Appends raw bytes from a span to the buffer
inline void append_buffer(std::span<const uint8_t> data,
                          std::vector<uint8_t>& buffer) {
  buffer.insert(buffer.end(), data.begin(), data.end());
}

// Helper to append any range of arithmetic values
template <std::ranges::range R>
  requires std::is_arithmetic_v<std::ranges::range_value_t<R>>
void append_range(R&& range, std::vector<uint8_t>& buffer) {
  using T = std::ranges::range_value_t<R>;

  if constexpr (std::ranges::contiguous_range<R>) {
    // Fast path for vectors, arrays, spans
    std::span<const T> data{std::ranges::data(range), std::ranges::size(range)};

    size_t total_bytes = data.size() * sizeof(T);

    if constexpr (std::endian::native == std::endian::little) {
      const auto* ptr = reinterpret_cast<const uint8_t*>(data.data());
      buffer.insert(buffer.end(), ptr, ptr + total_bytes);
    } else {
      size_t start_idx = buffer.size();
      buffer.resize(start_idx + total_bytes);
      uint8_t* dest = buffer.data() + start_idx;

      for (const auto& val : data) {
        T swapped = internal::byteswap(val);
        std::memcpy(dest, &swapped, sizeof(T));
        dest += sizeof(T);
      }
    }
  } else {
    // Slow path for non-contiguous ranges (e.g. strided views)
    // We always write Little Endian
    for (const auto& val : range) {
      write_le(static_cast<T>(val), buffer);
    }
  }
}

// Deprecated/Legacy wrapper
template <typename T>
  requires std::is_arithmetic_v<std::remove_const_t<T>>
void append_data(std::span<T> data, std::vector<uint8_t>& buffer) {
  append_range(data, buffer);
}

// Base64 Encoder
inline std::string base64_encode(std::span<const uint8_t> input) {
  static const char* lookup =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string output;
  output.reserve(((input.size() + 2) / 3) * 4);

  size_t i = 0;
  for (; i + 2 < input.size(); i += 3) {
    uint32_t val = (input[i] << 16) | (input[i + 1] << 8) | input[i + 2];
    output.push_back(lookup[(val >> 18) & 0x3F]);
    output.push_back(lookup[(val >> 12) & 0x3F]);
    output.push_back(lookup[(val >> 6) & 0x3F]);
    output.push_back(lookup[val & 0x3F]);
  }

  if (i < input.size()) {
    uint32_t val = (input[i] << 16);
    if (i + 1 < input.size()) val |= (input[i + 1] << 8);

    output.push_back(lookup[(val >> 18) & 0x3F]);
    output.push_back(lookup[(val >> 12) & 0x3F]);

    if (i + 1 < input.size()) {
      output.push_back(lookup[(val >> 6) & 0x3F]);
    } else {
      output.push_back('=');
    }
    output.push_back('=');
  }
  return output;
}

}  // namespace microvtk::core
