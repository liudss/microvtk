#pragma once

#include <fstream>
#include <memory>
#include <microvtk/adapter.hpp>
#include <microvtk/common/types.hpp>
#include <microvtk/core/binary_utils.hpp>
#include <microvtk/core/data_accessor.hpp>
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
    pointsBlock_ = registerData(points, "Points", 3);
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

    cellsConnBlock_ = registerData(connectivity, "connectivity", 1);
    cellsOffsetsBlock_ = registerData(offsets, "offsets", 1);
    cellsTypesBlock_ = registerData(types, "types", 1);
  }

  // 3. Add Point Data (Attribute)
  template <std::ranges::range R>
  void addPointData(std::string_view name, const R& data,
                    int numComponents = 1) {
    pointDataBlocks_.push_back(registerData(data, name, numComponents));
  }

  // 4. Add Cell Data (Attribute)
  template <std::ranges::range R>
  void addCellData(std::string_view name, const R& data,
                   int numComponents = 1) {
    cellDataBlocks_.push_back(registerData(data, name, numComponents));
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

    // Stream binary data directly from registered accessors
    for (const auto& accessor : accessors_) {
      // 1. Write Header (Size of data in bytes)
      uint64_t dataSize = accessor->size_bytes();
      
      // We need to write this uint64_t in Little Endian
      // Reuse core::write_le but we need a buffer or direct stream write
      // Let's use a small local buffer for the header
      std::vector<uint8_t> headerBuf;
      headerBuf.reserve(8);
      core::write_le(dataSize, headerBuf);
      ofs.write(reinterpret_cast<const char*>(headerBuf.data()), headerBuf.size());

      // 2. Write Data Payload
      accessor->write_to_stream(ofs);
    }

    ofs << "</AppendedData>\n";
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
  DataBlockInfo registerData(const R& data, std::string_view name,
                             int numComponents) {
    using T = std::ranges::range_value_t<R>;

    DataBlockInfo info;
    info.name = name;
    // Current virtual offset
    info.offset = currentOffset_;
    info.typeName = vtkTypeName<std::remove_const_t<T>>();
    info.numComponents = numComponents;
    info.valid = true;

    // Create accessor to hold reference
    // Note: User must ensure 'data' outlives the writer or the write() call!
    auto accessor = std::make_unique<core::RangeAccessor<R>>(data);
    
    // Calculate size: Header (8 bytes) + Data Bytes
    uint64_t payloadSize = accessor->size_bytes();
    currentOffset_ += sizeof(uint64_t) + payloadSize;

    accessors_.push_back(std::move(accessor));

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

  // Replaces appendedData_
  std::vector<std::unique_ptr<core::DataAccessor>> accessors_;
  uint64_t currentOffset_ = 0;

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
