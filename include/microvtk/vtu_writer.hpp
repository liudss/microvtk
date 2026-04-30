#pragma once

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
#include <type_traits>
#include <utility>
#include <vector>

namespace microvtk {

class VtuWriter : private core::AppendedDataWriter {
public:
  using core::AppendedDataWriter::addCellData;
  using core::AppendedDataWriter::addPointData;
  using core::AppendedDataWriter::setCompression;

  explicit VtuWriter(DataFormat /*format*/ = DataFormat::Appended) {}

  // 1. Set Points (Coordinates)
  // range: a flattened scalar sequence (x0, y0, z0, x1, y1, z1...)
  // inputDim: components per point in the input range (1, 2, or 3)
  template <std::ranges::random_access_range R>
  void setPoints(const R& points, int inputDim = 3) {
    if (inputDim < 1 || inputDim > 3) {
      throw std::invalid_argument(
          "VtuWriter::setPoints: inputDim must be 1, 2, or 3.");
    }
    if (std::ranges::size(points) % static_cast<size_t>(inputDim) != 0) {
      throw std::invalid_argument(
          "VtuWriter::setPoints: points size must be divisible by inputDim.");
    }

    if (inputDim == 3) {
      numberOfPoints_ = std::ranges::size(points) / 3;
      pointsBlock_ = registerData(points, "Points", 3);
    } else {
      // Automatic padding to 3D via lazy transform view
      numberOfPoints_ = std::ranges::size(points) / inputDim;
      auto view = std::views::all(points);

      // We use a simple iota + transform to interleave 0.0 for missing
      // components
      auto padded =
          std::views::iota(size_t{0}, numberOfPoints_ * 3) |
          std::views::transform([view, inputDim](size_t i) -> double {
            size_t ptIdx = i / 3;
            size_t compIdx = i % 3;
            if (std::cmp_less(compIdx, inputDim)) {
              // Access the flattened input range
              return static_cast<double>(
                  view[(ptIdx * static_cast<size_t>(inputDim)) + compIdx]);
            }
            return 0.0;
          });
      pointsBlock_ = registerData(padded, "Points", 3);
    }
    hasPoints_ = true;
  }

  // 2. Set Cells (Topology)
  // connectivity: node indices
  // offsets: end index of each cell in connectivity
  // types: CellType enum values
  template <std::ranges::range R1, std::ranges::range R2, std::ranges::range R3>
  void setCells(const R1& connectivity, const R2& offsets, const R3& types) {
    if (!hasPoints_) {
      throw std::logic_error(
          "VtuWriter::setCells: points must be set before cells.");
    }
    if (std::ranges::size(offsets) != std::ranges::size(types)) {
      throw std::invalid_argument(
          "VtuWriter::setCells: Size mismatch between offsets and types.");
    }
    validateCellTopology(connectivity, offsets);
    numberOfCells_ = std::ranges::size(types);

    cellsConnBlock_ = registerData(connectivity, "connectivity", 1);
    cellsOffsetsBlock_ = registerData(offsets, "offsets", 1);
    cellsTypesBlock_ = registerData(types, "types", 1);
  }

  // 5. Write to file
  void write(std::string_view filename) {
    validateDataSizes();
    writeAppendedVtkFile(filename, dataBlocksInWriteOrder(), "UnstructuredGrid",
                         "VtuWriter::write",
                         [this](core::XmlBuilder& xml) { writeXmlBody(xml); });
  }

private:
  void validateDataSizes() const {
    if (!hasPoints_) {
      throw std::logic_error("VtuWriter::write: points have not been set.");
    }
    validateAttributeDataSizes(numberOfPoints_, numberOfCells_,
                               "VtuWriter::write");
  }

  template <std::ranges::range R1, std::ranges::range R2>
  void validateCellTopology(const R1& connectivity, const R2& offsets) const {
    size_t previous = 0;
    size_t offsetCount = 0;
    for (const auto& offsetValue : offsets) {
      using OffsetType = std::remove_cvref_t<decltype(offsetValue)>;
      if constexpr (std::is_signed_v<OffsetType>) {
        if (offsetValue < 0) {
          throw std::invalid_argument(
              "VtuWriter::setCells: offsets must be non-negative.");
        }
      }

      const auto offset = static_cast<size_t>(offsetValue);
      if (offset <= previous) {
        throw std::invalid_argument(
            "VtuWriter::setCells: offsets must be strictly increasing.");
      }
      previous = offset;
      ++offsetCount;
    }

    if (offsetCount > 0 && previous != std::ranges::size(connectivity)) {
      throw std::invalid_argument(
          "VtuWriter::setCells: final offset must match connectivity size.");
    }
    if (offsetCount == 0 && std::ranges::size(connectivity) != 0) {
      throw std::invalid_argument(
          "VtuWriter::setCells: offsets required for non-empty connectivity.");
    }

    for (const auto& indexValue : connectivity) {
      using IndexType = std::remove_cvref_t<decltype(indexValue)>;
      if constexpr (std::is_signed_v<IndexType>) {
        if (indexValue < 0) {
          throw std::invalid_argument(
              "VtuWriter::setCells: connectivity index out of range.");
        }
      }

      if (static_cast<size_t>(indexValue) >= numberOfPoints_) {
        throw std::invalid_argument(
            "VtuWriter::setCells: connectivity index out of range.");
      }
    }
  }

  std::vector<DataBlockInfo*> dataBlocksInWriteOrder() {
    std::vector<DataBlockInfo*> blocks = {&pointsBlock_, &cellsConnBlock_,
                                          &cellsOffsetsBlock_,
                                          &cellsTypesBlock_};
    appendAttributeBlocks(blocks);
    return blocks;
  }

  void writeXmlBody(core::XmlBuilder& xml) {
    {
      auto grid = xml.scopedElement("UnstructuredGrid");
      {
        auto piece = xml.scopedElement("Piece");
        piece.attr("NumberOfPoints", numberOfPoints_);
        piece.attr("NumberOfCells", numberOfCells_);

        {
          auto p = xml.scopedElement("Points");
          writeDataArrayHeader(xml, pointsBlock_);
        }

        {
          auto c = xml.scopedElement("Cells");
          writeDataArrayHeader(xml, cellsConnBlock_);
          writeDataArrayHeader(xml, cellsOffsetsBlock_);
          writeDataArrayHeader(xml, cellsTypesBlock_);
        }

        writePointDataHeaders(xml);
        writeCellDataHeaders(xml);
      }
    }
  }

  size_t numberOfPoints_ = 0;
  size_t numberOfCells_ = 0;
  bool hasPoints_ = false;

  DataBlockInfo pointsBlock_;
  DataBlockInfo cellsConnBlock_;
  DataBlockInfo cellsOffsetsBlock_;
  DataBlockInfo cellsTypesBlock_;
};

}  // namespace microvtk
