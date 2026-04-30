#pragma once

#include <concepts>
#include <cstdint>
#include <iostream>
#include <microvtk/core/binary_utils.hpp>
#include <optional>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

namespace microvtk::core {

struct ByteWriter {
  virtual ~ByteWriter() = default;
  virtual void write(std::span<const uint8_t> bytes) = 0;
};

class VectorByteWriter : public ByteWriter {
public:
  explicit VectorByteWriter(std::vector<uint8_t>& buffer) : buffer_(buffer) {}

  void write(std::span<const uint8_t> bytes) override {
    buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
  }

private:
  std::vector<uint8_t>& buffer_;
};

class OstreamByteWriter : public ByteWriter {
public:
  explicit OstreamByteWriter(std::ostream& os) : os_(os) {}

  void write(std::span<const uint8_t> bytes) override {
    os_.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }

private:
  std::ostream& os_;
};

// Abstract base class for type-erased data access
struct DataAccessor {
  virtual ~DataAccessor() = default;
  virtual void write_bytes(ByteWriter& writer) const = 0;
  [[nodiscard]] virtual std::optional<std::span<const uint8_t>>
  contiguous_bytes() const = 0;
  [[nodiscard]] virtual size_t size_bytes() const = 0;
};

// Concrete implementation for a specific range type
template <std::ranges::range R>
class RangeAccessor : public DataAccessor {
public:
  explicit RangeAccessor(R range) : range_(std::move(range)) {}

  void write_bytes(ByteWriter& writer) const override {
    if constexpr (std::ranges::contiguous_range<R> &&
                  std::endian::native == std::endian::little) {
      auto data_span = std::span{range_};
      const auto* bytes = reinterpret_cast<const uint8_t*>(data_span.data());
      writer.write(std::span<const uint8_t>{bytes, data_span.size_bytes()});
    } else {
      std::vector<uint8_t> temp;
      temp.reserve(4096);

      for (const auto& val : range_) {
        microvtk::core::write_le(val, temp);
        if (temp.size() >= 4096) {
          writer.write(temp);
          temp.clear();
        }
      }
      if (!temp.empty()) {
        writer.write(temp);
      }
    }
  }

  [[nodiscard]] std::optional<std::span<const uint8_t>> contiguous_bytes()
      const override {
    using T = std::ranges::range_value_t<R>;

    if constexpr (std::ranges::contiguous_range<R> &&
                  std::is_arithmetic_v<std::remove_const_t<T>> &&
                  std::endian::native == std::endian::little) {
      auto data_span = std::span{range_};
      const auto* bytes = reinterpret_cast<const uint8_t*>(data_span.data());
      return std::span<const uint8_t>{bytes, data_span.size_bytes()};
    } else {
      return std::nullopt;
    }
  }

  [[nodiscard]] size_t size_bytes() const override {
    using T = std::ranges::range_value_t<R>;
    return std::ranges::size(range_) * sizeof(T);
  }

private:
  R range_;
};

}  // namespace microvtk::core
