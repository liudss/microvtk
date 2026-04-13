#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <microvtk/adapter.hpp>
#include <microvtk/common/types.hpp>
#include <microvtk/core/appended_data_writer.hpp>
#include <microvtk/core/xml_utils.hpp>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace microvtk {

class VtiWriter : private core::AppendedDataWriter {
public:
  using core::AppendedDataWriter::setCompression;

  // Constructor defining the grid geometry
  // wholeExtent: x_min, x_max, y_min, y_max, z_min, z_max
  VtiWriter(const std::array<int, 6>& wholeExtent,
            const std::array<double, 3>& origin = {0.0, 0.0, 0.0},
            const std::array<double, 3>& spacing = {1.0, 1.0, 1.0})
      : wholeExtent_(wholeExtent), origin_(origin), spacing_(spacing) {}

  // 1. Add Point Data (Attribute)
  template <std::ranges::range R>
  void addPointData(std::string_view name, const R& data,
                    int numComponents = 1) {
    pointDataBlocks_.push_back(registerData(data, name, numComponents));
  }

  // 2. Add Cell Data (Attribute)
  template <std::ranges::range R>
  void addCellData(std::string_view name, const R& data,
                   int numComponents = 1) {
    cellDataBlocks_.push_back(registerData(data, name, numComponents));
  }

  // 3. Write to file
  void write(std::string_view filename) {
    validateDataSizes();

    auto orderedBlocks = dataBlocksInWriteOrder();
    std::vector<std::vector<uint8_t>> compressedBuffers;
    std::vector<uint64_t> originalSizes;
    const bool usingCompression =
        prepareAppendedData(orderedBlocks, compressedBuffers, originalSizes);

    std::ofstream ofs(std::string(filename), std::ios::binary);
    writeXmlStructure(ofs, usingCompression);
    writeAppendedData(ofs, orderedBlocks, usingCompression, compressedBuffers,
                      originalSizes);

    ofs << "</VTKFile>";
  }

private:
  [[nodiscard]] size_t pointCount() const {
    size_t count = 1;
    for (size_t axis = 0; axis < 3; ++axis) {
      const int min = wholeExtent_[axis * 2];
      const int max = wholeExtent_[axis * 2 + 1];
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
      const int max = wholeExtent_[axis * 2 + 1];
      if (max < min) {
        throw std::invalid_argument("VtiWriter: invalid extent.");
      }
      const auto pointCount = static_cast<size_t>(max - min + 1);
      count *= std::max<size_t>(pointCount - 1, 1);
    }
    return count;
  }

  void validateDataSizes() const {
    const auto expectedPointElements = pointCount();
    const auto expectedCellElements = cellCount();

    for (const auto& block : pointDataBlocks_) {
      if (block.numberOfElements !=
          expectedPointElements * block.numComponents) {
        throw std::invalid_argument(
            "VtiWriter::write: Size mismatch in PointData '" + block.name +
            "'.");
      }
    }
    for (const auto& block : cellDataBlocks_) {
      if (block.numberOfElements !=
          expectedCellElements * block.numComponents) {
        throw std::invalid_argument(
            "VtiWriter::write: Size mismatch in CellData '" + block.name +
            "'.");
      }
    }
  }

  std::vector<DataBlockInfo*> dataBlocksInWriteOrder() {
    std::vector<DataBlockInfo*> blocks;
    blocks.reserve(pointDataBlocks_.size() + cellDataBlocks_.size());
    for (auto& block : pointDataBlocks_) blocks.push_back(&block);
    for (auto& block : cellDataBlocks_) blocks.push_back(&block);
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

        if (!pointDataBlocks_.empty()) {
          auto pd = xml.scopedElement("PointData");
          for (const auto& block : pointDataBlocks_)
            writeDataArrayHeader(xml, block);
        }

        if (!cellDataBlocks_.empty()) {
          auto cd = xml.scopedElement("CellData");
          for (const auto& block : cellDataBlocks_)
            writeDataArrayHeader(xml, block);
        }
      }
    }

    xml.startElement("AppendedData");
    xml.attribute("encoding", "raw");
    xml.writeRaw(">_");
  }

  std::array<int, 6> wholeExtent_;
  std::array<double, 3> origin_;
  std::array<double, 3> spacing_;

  std::vector<DataBlockInfo> pointDataBlocks_;
  std::vector<DataBlockInfo> cellDataBlocks_;
};

}  // namespace microvtk
