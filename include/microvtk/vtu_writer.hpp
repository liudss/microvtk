#pragma once

#include <fstream>
#include <microvtk/adapter.hpp>
#include <microvtk/common/types.hpp>
#include <microvtk/core/binary_utils.hpp>
#include <microvtk/core/xml_utils.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace microvtk {

class VtuWriter {
public:
  explicit VtuWriter(DataFormat /*format*/ = DataFormat::Appended) {}

  // 1. Set Points (Coordinates)
  // range: x,y,z, x,y,z... (3 components)
  template <std::ranges::range R>
  void setPoints(const R& points) {
    numberOfPoints_ = std::ranges::size(points) / 3;
    pointsBlock_ = appendData(points, "Points", 3);
  }

  // 2. Set Cells (Topology)
  // connectivity: node indices
  // offsets: end index of each cell in connectivity
  // types: CellType enum values
  template <std::ranges::range R1, std::ranges::range R2, std::ranges::range R3>
  void setCells(const R1& connectivity, const R2& offsets, const R3& types) {
    if (std::ranges::size(offsets) != std::ranges::size(types)) {
      throw std::invalid_argument(
          "VtuWriter::setCells: Size mismatch between offsets and types.");
    }
    numberOfCells_ = std::ranges::size(types);

    cellsConnBlock_ = appendData(connectivity, "connectivity", 1);
    cellsOffsetsBlock_ = appendData(offsets, "offsets", 1);
    cellsTypesBlock_ = appendData(types, "types", 1);
  }

  // 3. Add Point Data (Attribute)
  template <std::ranges::range R>
  void addPointData(std::string_view name, const R& data,
                    int numComponents = 1) {
    pointDataBlocks_.push_back(appendData(data, name, numComponents));
  }

  // 4. Add Cell Data (Attribute)
  template <std::ranges::range R>
  void addCellData(std::string_view name, const R& data,
                   int numComponents = 1) {
    cellDataBlocks_.push_back(appendData(data, name, numComponents));
  }

  // 5. Write to file
  void write(std::string_view filename) {
    std::ofstream ofs(std::string(filename), std::ios::binary);
    core::XmlBuilder xml(ofs);

    // Header
    xml.startElement("VTKFile");
    xml.attribute("type", "UnstructuredGrid");
    xml.attribute("version", "1.0");
    xml.attribute("byte_order", "LittleEndian");
    xml.attribute("header_type", "UInt64");

    {
      auto grid = xml.scopedElement("UnstructuredGrid");
      {
        auto piece = xml.scopedElement("Piece");
        piece.attr("NumberOfPoints", numberOfPoints_);
        piece.attr("NumberOfCells", numberOfCells_);

        // <Points>
        {
          auto p = xml.scopedElement("Points");
          writeArrayHeader(xml, pointsBlock_);
        }

        // <Cells>
        {
          auto c = xml.scopedElement("Cells");
          writeArrayHeader(xml, cellsConnBlock_);
          writeArrayHeader(xml, cellsOffsetsBlock_);
          writeArrayHeader(xml, cellsTypesBlock_);
        }

        // <PointData>
        if (!pointDataBlocks_.empty()) {
          auto pd = xml.scopedElement("PointData");
          for (const auto& block : pointDataBlocks_) {
            writeArrayHeader(xml, block);
          }
        }

        // <CellData>
        if (!cellDataBlocks_.empty()) {
          auto cd = xml.scopedElement("CellData");
          for (const auto& block : cellDataBlocks_) {
            writeArrayHeader(xml, block);
          }
        }
      }
    }

    // <AppendedData>
    xml.startElement("AppendedData");
    xml.attribute("encoding", "raw");
    // Start marker
    xml.writeRaw(">_");

    // Dump binary data
    // Note: XmlBuilder might have tried to close the tag or manage state.
    // We need to bypass it slightly for raw dump or ensure it's clean.
    // However, standard XML writers usually don't handle mixed content like
    // this easily. But AppendedData content is technically text/CDATA but here
    // it's raw bytes. VTK spec: <AppendedData
    // encoding="raw">_...binary...</AppendedData>

    // Write the blob
    ofs.write(reinterpret_cast<const char*>(appendedData_.data()),
              static_cast<std::streamsize>(appendedData_.size()));

    // Close AppendedData manually because of the raw dump hack or just use
    // endElement if we assume it closes correctly. XmlBuilder::endElement()
    // prints </AppendedData> But we are in "InStartTag" state internally in
    // builder if we just did startElement. We manually wrote ">_", so we need
    // to update builder state or just finish manually.

    // Actually, XmlBuilder.startElement leaves state InStartTag.
    // If we write raw to stream, we should probably tell builder we are done
    // with start tag. But let's just close it manually.
    ofs << "</AppendedData>\n";

    // We shouldn't call xml.endElement() for AppendedData because we manually
    // closed it. But we need to close VTKFile. Hack: Pop the stack in builder
    // manually or just let it close remaining? Let's rely on RAII or manual
    // closing? Better: XmlBuilder should support "writeRaw".

    // For now, I will just manually close VTKFile and be done,
    // effectively abandoning the builder for the very last part.
    ofs << "</VTKFile>";
  }

private:
  struct DataBlockInfo {
    std::string name;
    uint64_t offset;
    std::string typeName;
    int numComponents;
    bool valid = false;
  };

  template <std::ranges::range R>
  DataBlockInfo appendData(const R& data, std::string_view name,
                           int numComponents) {
    using T = std::ranges::range_value_t<R>;

    DataBlockInfo info;
    info.name = name;
    info.offset = appendedData_.size();
    info.typeName = vtkTypeName<std::remove_const_t<T>>();
    info.numComponents = numComponents;
    info.valid = true;

    // Header: Size of data in bytes (UInt64)
    uint64_t dataSize = std::ranges::size(data) * sizeof(T);
    core::write_le(dataSize, appendedData_);

    // Data
    core::append_range(data, appendedData_);

    return info;
  }

  static void writeArrayHeader(core::XmlBuilder& xml,
                               const DataBlockInfo& info) {
    if (!info.valid) return;
    xml.startElement("DataArray");
    xml.attribute("type", info.typeName);
    xml.attribute("Name", info.name);
    xml.attribute("NumberOfComponents", info.numComponents);
    xml.attribute("format", "appended");
    xml.attribute("offset", info.offset);
    xml.endElement();
  }

  std::vector<uint8_t> appendedData_;

  size_t numberOfPoints_ = 0;
  size_t numberOfCells_ = 0;

  DataBlockInfo pointsBlock_;
  DataBlockInfo cellsConnBlock_;
  DataBlockInfo cellsOffsetsBlock_;
  DataBlockInfo cellsTypesBlock_;

  std::vector<DataBlockInfo> pointDataBlocks_;
  std::vector<DataBlockInfo> cellDataBlocks_;
};

}  // namespace microvtk
