#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <microvtk/adapter.hpp>
#include <microvtk/common/types.hpp>
#include <microvtk/core/appended_data_writer.hpp>
#include <microvtk/core/xml_utils.hpp>
#include <ostream>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace microvtk {

class VtiWriter : private core::AppendedDataWriter {
public:
  using core::AppendedDataWriter::addCellData;
  using core::AppendedDataWriter::addPointData;
  using core::AppendedDataWriter::setCompression;

  // Constructor defining the grid geometry
  // wholeExtent: x_min, x_max, y_min, y_max, z_min, z_max
  VtiWriter(const std::array<int, 6>& wholeExtent,
            const std::array<double, 3>& origin = {0.0, 0.0, 0.0},
            const std::array<double, 3>& spacing = {1.0, 1.0, 1.0})
      : extent_(wholeExtent), origin_(origin), spacing_(spacing) {}

  // 3. Write to file
  void write(std::string_view filename) {
    validateDataSizes();
    writeAppendedVtkFile(filename, dataBlocksInWriteOrder(), "ImageData",
                         "VtiWriter::write",
                         [this](core::XmlBuilder& xml) { writeXmlBody(xml); });
  }

private:
  struct ImageExtent {
    explicit ImageExtent(const std::array<int, 6>& values) : values_(values) {
      for (size_t axis = 0; axis < 3; ++axis) {
        if (max(axis) < min(axis)) {
          throw std::invalid_argument("VtiWriter: invalid extent.");
        }
      }
    }

    [[nodiscard]] size_t pointCount() const {
      size_t count = 1;
      for (size_t axis = 0; axis < 3; ++axis) {
        count = checkedMultiply(count, pointCount(axis));
      }
      return count;
    }

    [[nodiscard]] size_t cellCount() const {
      size_t count = 1;
      for (size_t axis = 0; axis < 3; ++axis) {
        count =
            checkedMultiply(count, std::max<size_t>(pointCount(axis) - 1, 1));
      }
      return count;
    }

    [[nodiscard]] std::string toAttributeString() const {
      std::string text;
      for (size_t i = 0; i < values_.size(); ++i) {
        if (i > 0) text += " ";
        text += std::to_string(values_[i]);
      }
      return text;
    }

  private:
    [[nodiscard]] int min(size_t axis) const { return values_[axis * 2]; }

    [[nodiscard]] int max(size_t axis) const { return values_[(axis * 2) + 1]; }

    [[nodiscard]] size_t pointCount(size_t axis) const {
      const auto width = static_cast<std::int64_t>(max(axis)) -
                         static_cast<std::int64_t>(min(axis)) + 1;
      return static_cast<size_t>(width);
    }

    [[nodiscard]] static size_t checkedMultiply(size_t lhs, size_t rhs) {
      if (rhs != 0 && lhs > std::numeric_limits<size_t>::max() / rhs) {
        throw std::invalid_argument("VtiWriter: extent size overflow.");
      }
      return lhs * rhs;
    }

    std::array<int, 6> values_;
  };

  void validateDataSizes() const {
    validateAttributeDataSizes(extent_.pointCount(), extent_.cellCount(),
                               "VtiWriter::write");
  }

  std::vector<DataBlockInfo*> dataBlocksInWriteOrder() {
    std::vector<DataBlockInfo*> blocks;
    blocks.reserve(attributeBlockCount());
    appendAttributeBlocks(blocks);
    return blocks;
  }

  void writeXmlBody(core::XmlBuilder& xml) {
    {
      auto grid = xml.scopedElement("ImageData");

      // Format arrays as strings for attributes
      auto formatArray = [](const auto& arr) {
        std::string s;
        for (size_t i = 0; i < arr.size(); ++i) {
          if (i > 0) s += " ";
          s += std::to_string(arr[i]);
        }
        return s;
      };

      grid.attr("WholeExtent", extent_.toAttributeString());
      grid.attr("Origin", formatArray(origin_));
      grid.attr("Spacing", formatArray(spacing_));

      {
        auto piece = xml.scopedElement("Piece");
        // For serial writing, Piece Extent is same as WholeExtent
        piece.attr("Extent", extent_.toAttributeString());

        writePointDataHeaders(xml);
        writeCellDataHeaders(xml);
      }
    }
  }

  ImageExtent extent_;
  std::array<double, 3> origin_;
  std::array<double, 3> spacing_;
};

}  // namespace microvtk
