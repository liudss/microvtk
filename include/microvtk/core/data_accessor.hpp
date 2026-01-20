#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <iostream>
#include <microvtk/core/binary_utils.hpp>
#include <ranges>
#include <vector>

namespace microvtk::core {

// Abstract base class for type-erased data access
struct DataAccessor {
  virtual ~DataAccessor() = default;
  virtual void write_to(
      std::vector<uint8_t>& buffer) const = 0;  // Legacy support (optional)
  virtual void write_to_stream(std::ostream& os) const = 0;
  [[nodiscard]] virtual size_t size_bytes() const = 0;
};

// Concrete implementation for a specific range type
template <std::ranges::range R>
class RangeAccessor : public DataAccessor {
public:
  explicit RangeAccessor(const R& range) : range_(range) {}

  void write_to(std::vector<uint8_t>& buffer) const override {
    // This might be inefficient if used, but provided for compatibility
    microvtk::core::append_range(range_, buffer);
  }

  void write_to_stream(std::ostream& os) const override {
    // Determine if we can just write raw bytes
    if constexpr (std::ranges::contiguous_range<R> &&
                  std::endian::native == std::endian::little) {
      auto data_span = std::span{range_};
      os.write(reinterpret_cast<const char*>(data_span.data()),
               static_cast<std::streamsize>(data_span.size_bytes()));
    } else {
      // Slow path: copy to small buffer and write or write element by element
      // Using a small buffer for buffering is better than 1-byte writes
      // But for now, let's reuse a small buffer strategy similar to
      // append_range logic but directed to stream Or just use a local buffer
      // vector
      std::vector<uint8_t> temp;
      // Heuristic: reserve some space
      temp.reserve(4096);

      for (const auto& val : range_) {
        microvtk::core::write_le(val, temp);
        if (temp.size() >= 4096) {
          os.write(reinterpret_cast<const char*>(temp.data()),
                   static_cast<std::streamsize>(temp.size()));
          temp.clear();
        }
      }
      if (!temp.empty()) {
        os.write(reinterpret_cast<const char*>(temp.data()),
                 static_cast<std::streamsize>(temp.size()));
      }
    }
  }

  [[nodiscard]] size_t size_bytes() const override {
    using T = std::ranges::range_value_t<R>;
    return std::ranges::size(range_) * sizeof(T);
  }

private:
  const R& range_;
};

}  // namespace microvtk::core
