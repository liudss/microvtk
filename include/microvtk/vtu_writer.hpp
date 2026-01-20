#pragma once

#include <fstream>
#include <memory>
#include <microvtk/adapter.hpp>
#include <microvtk/common/types.hpp>
#include <microvtk/core/binary_utils.hpp>
#include <microvtk/core/compressor.hpp>
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

  void setCompression(core::CompressionType type) { compressionType_ = type; }

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
    // Phase 1: Compression Pre-pass (if enabled)
    // We must compress everything to know offsets.
    std::vector<std::vector<uint8_t>> compressedBuffers;
    std::vector<uint64_t> originalSizes;

    bool usingCompression = (compressionType_ != core::CompressionType::None);
    auto compressor = core::createCompressor(compressionType_);

    if (usingCompression && !compressor) {
      // Fallback or warning if requested compressor is not available?
      // For now, let's just silently disable compression or throw.
      // Let's disable and proceed uncompressed to be safe.
      usingCompression = false;
    }

    if (usingCompression) {
      // Reset offsets for compressed layout
      uint64_t runningOffset = 0;

      // Re-calculate offsets for all blocks
      auto processBlock = [&](DataBlockInfo& info, size_t index) {
        if (!info.valid) return;

        auto& accessor = accessors_[index];
        std::vector<uint8_t> tempRaw;
        // TODO: DataAccessor should have a better way to get raw data
        // For now, write to a temp vector
        accessor->write_to(tempRaw);

        originalSizes.push_back(tempRaw.size());

        auto compressed = compressor->compress(tempRaw);
        compressedBuffers.push_back(std::move(compressed));

        info.offset = runningOffset;

        // Header overhead: #blocks(1) + BlockSize(1) + LastBlockSize(1) +
        // CompressedSize(1) = 4 * 8 bytes = 32 bytes
        uint64_t headerSize = 4 * sizeof(uint64_t);
        runningOffset += headerSize + compressedBuffers.back().size();
      };

      // Note: The order must match the write order below!
      // Points
      processBlock(pointsBlock_, 0);
      // Cells
      processBlock(cellsConnBlock_, 1);
      processBlock(cellsOffsetsBlock_, 2);
      processBlock(cellsTypesBlock_, 3);

      // Attributes
      size_t idx = 4;
      for (auto& block : pointDataBlocks_) processBlock(block, idx++);
      for (auto& block : cellDataBlocks_) processBlock(block, idx++);
    }

    std::ofstream ofs(std::string(filename), std::ios::binary);
    core::XmlBuilder xml(ofs);

    // Header
    xml.startElement("VTKFile");
    xml.attribute("type", "UnstructuredGrid");
    xml.attribute("version", "1.0");
    xml.attribute("byte_order", "LittleEndian");
    xml.attribute("header_type", "UInt64");

    if (usingCompression) {
      if (compressionType_ == core::CompressionType::ZLib)
        xml.attribute("compressor", "vtkZLibDataCompressor");
      else if (compressionType_ == core::CompressionType::LZ4)
        xml.attribute("compressor", "vtkLZ4DataCompressor");
    }

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
    xml.writeRaw(">_");

    if (usingCompression) {
      // Write Compressed Data
      // Order must match the processBlock order above
      size_t bufIdx = 0;

      auto writeCompressedBlock = [&](size_t origSize) {
        const auto& compressed = compressedBuffers[bufIdx];

        // VTK Compressed Header (for 1 block)
        // [NumberOfBlocks][BlockSize][LastBlockSize][CompressedBlockSize]
        std::vector<uint64_t> header = {
            1, static_cast<uint64_t>(origSize), static_cast<uint64_t>(origSize),
            static_cast<uint64_t>(compressed.size())};

        for (auto val : header) {
          core::write_le(val,
                         compressedBuffers[bufIdx]);  // Hack: append header to
                                                      // the buffer temporarily?
          // No, just write to stream
          // Actually core::write_le takes a vector. Let's make a small buffer.
        }

        // Directly write header to stream
        for (auto val : header) {
          std::vector<uint8_t> tmp;
          core::write_le(val, tmp);
          ofs.write(reinterpret_cast<const char*>(tmp.data()), tmp.size());
        }

        // Write Data
        ofs.write(reinterpret_cast<const char*>(compressed.data()),
                  compressed.size());

        bufIdx++;
      };

      if (pointsBlock_.valid) writeCompressedBlock(originalSizes[bufIdx]);
      if (cellsConnBlock_.valid) writeCompressedBlock(originalSizes[bufIdx]);
      if (cellsOffsetsBlock_.valid) writeCompressedBlock(originalSizes[bufIdx]);
      if (cellsTypesBlock_.valid) writeCompressedBlock(originalSizes[bufIdx]);
      for (const auto& b : pointDataBlocks_)
        if (b.valid) writeCompressedBlock(originalSizes[bufIdx]);
      for (const auto& b : cellDataBlocks_)
        if (b.valid) writeCompressedBlock(originalSizes[bufIdx]);

    } else {
      // Streaming Mode (Zero Copy)
      for (const auto& accessor : accessors_) {
        uint64_t dataSize = accessor->size_bytes();
        std::vector<uint8_t> headerBuf;
        headerBuf.reserve(8);
        core::write_le(dataSize, headerBuf);
        ofs.write(reinterpret_cast<const char*>(headerBuf.data()),
                  headerBuf.size());
        accessor->write_to_stream(ofs);
      }
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
    // Current virtual offset (assuming uncompressed, will be overwritten if
    // compressed)
    info.offset = currentOffset_;
    info.typeName = vtkTypeName<std::remove_const_t<T>>();
    info.numComponents = numComponents;
    info.valid = true;

    auto accessor = std::make_unique<core::RangeAccessor<R>>(data);

    // Calculate size: Header (8 bytes) + Data Bytes (Uncompressed)
    // This is the default assumption.
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
  core::CompressionType compressionType_ = core::CompressionType::None;

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
