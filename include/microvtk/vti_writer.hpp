#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
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
      : wholeExtent_(wholeExtent), origin_(origin), spacing_(spacing) {}

  // 3. Write to file
  void write(std::string_view filename) {
    validateDataSizes();
    writeAppendedFile(
        filename, dataBlocksInWriteOrder(),
        [this](std::ostream& ofs, bool usingCompression) {
          writeXmlStructure(ofs, usingCompression);
        },
        "VtiWriter::write");
  }

private:
  [[nodiscard]] size_t pointCount() const {
    size_t count = 1;
    for (size_t axis = 0; axis < 3; ++axis) {
      const int min = wholeExtent_[axis * 2];
      const int max = wholeExtent_[(axis * 2) + 1];
      if (max < min) {
        throw std::invalid_argument("VtiWriter: invalid extent.");
      }
      count *= static_cast<size_t>(max - min + 1);
    }
    return count;
  }

  [[nodiscard]] size_t cellCount() const {
    size_t count = 1;
    for (size_t axis = 0; axis < 3; ++axis) {
      const int min = wholeExtent_[axis * 2];
      const int max = wholeExtent_[(axis * 2) + 1];
      if (max < min) {
        throw std::invalid_argument("VtiWriter: invalid extent.");
      }
      const auto pointCount =
          (static_cast<size_t>(max) - static_cast<size_t>(min)) + 1;
      count *= std::max<size_t>(pointCount - 1, 1);
    }
    return count;
  }

  void validateDataSizes() const {
    validateAttributeDataSizes(pointCount(), cellCount(), "VtiWriter::write");
  }

  std::vector<DataBlockInfo*> dataBlocksInWriteOrder() {
    std::vector<DataBlockInfo*> blocks;
    blocks.reserve(attributeBlockCount());
    appendAttributeBlocks(blocks);
    return blocks;
  }

  void writeXmlStructure(std::ostream& ofs, bool usingCompression) {
    core::XmlBuilder xml(ofs);
    writeVtkFileHeader(xml, "ImageData", usingCompression);

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

      grid.attr("WholeExtent", formatArray(wholeExtent_));
      grid.attr("Origin", formatArray(origin_));
      grid.attr("Spacing", formatArray(spacing_));

      {
        auto piece = xml.scopedElement("Piece");
        // For serial writing, Piece Extent is same as WholeExtent
        piece.attr("Extent", formatArray(wholeExtent_));

        writePointDataHeaders(xml);
        writeCellDataHeaders(xml);
      }
    }

    xml.startElement("AppendedData");
    xml.attribute("encoding", "raw");
    xml.writeRaw(">_");
  }

  std::array<int, 6> wholeExtent_;
  std::array<double, 3> origin_;
  std::array<double, 3> spacing_;
};

}  // namespace microvtk
