#pragma once

#include <array>
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

class VtiWriter {
public:
  // Constructor defining the grid geometry
  // wholeExtent: x_min, x_max, y_min, y_max, z_min, z_max
  VtiWriter(const std::array<int, 6>& wholeExtent,
            const std::array<double, 3>& origin = {0.0, 0.0, 0.0},
            const std::array<double, 3>& spacing = {1.0, 1.0, 1.0})
      : wholeExtent_(wholeExtent), origin_(origin), spacing_(spacing) {}

  void setCompression(core::CompressionType type) { compressionType_ = type; }

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
    bool usingCompression = (compressionType_ != core::CompressionType::None);
    auto compressor = core::createCompressor(compressionType_);

    if (usingCompression && !compressor) {
      usingCompression = false;
    }

    std::vector<std::vector<uint8_t>> compressedBuffers;
    std::vector<uint64_t> originalSizes;

    if (usingCompression) {
      prepareCompression(compressor, compressedBuffers, originalSizes);
    }

    std::ofstream ofs(std::string(filename), std::ios::binary);
    writeXmlStructure(ofs, usingCompression);
    writeAppendedData(ofs, usingCompression, compressedBuffers, originalSizes);

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

  void prepareCompression(const std::unique_ptr<core::Compressor>& compressor,
                          std::vector<std::vector<uint8_t>>& compressedBuffers,
                          std::vector<uint64_t>& originalSizes) {
    uint64_t runningOffset = 0;

    auto processBlock = [&](DataBlockInfo& info, size_t index) {
      if (!info.valid) return;

      auto& accessor = accessors_[index];
      std::vector<uint8_t> tempRaw;
      accessor->write_to(tempRaw);

      originalSizes.push_back(tempRaw.size());
      auto compressed = compressor->compress(tempRaw);
      compressedBuffers.push_back(std::move(compressed));

      info.offset = runningOffset;
      uint64_t headerSize = 4 * sizeof(uint64_t);
      runningOffset += headerSize + compressedBuffers.back().size();
    };

    size_t idx = 0;
    for (auto& block : pointDataBlocks_) processBlock(block, idx++);
    for (auto& block : cellDataBlocks_) processBlock(block, idx++);
  }

  void writeXmlStructure(std::ostream& ofs, bool usingCompression) {
    core::XmlBuilder xml(ofs);
    xml.startElement("VTKFile");
    xml.attribute("type", "ImageData");
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
            writeArrayHeader(xml, block);
        }

        if (!cellDataBlocks_.empty()) {
          auto cd = xml.scopedElement("CellData");
          for (const auto& block : cellDataBlocks_)
            writeArrayHeader(xml, block);
        }
      }
    }

    xml.startElement("AppendedData");
    xml.attribute("encoding", "raw");
    xml.writeRaw(">_");
  }

  void writeAppendedData(
      std::ostream& ofs, bool usingCompression,
      const std::vector<std::vector<uint8_t>>& compressedBuffers,
      const std::vector<uint64_t>& originalSizes) {
    if (usingCompression) {
      size_t bufIdx = 0;
      auto writeCompressedBlock = [&](size_t origSize) {
        const auto& compressed = compressedBuffers[bufIdx];
        std::vector<uint64_t> header = {
            1, static_cast<uint64_t>(origSize), static_cast<uint64_t>(origSize),
            static_cast<uint64_t>(compressed.size())};

        for (auto val : header) {
          std::vector<uint8_t> tmp;
          core::write_le(val, tmp);
          ofs.write(reinterpret_cast<const char*>(tmp.data()),
                    static_cast<std::streamsize>(tmp.size()));
        }
        ofs.write(reinterpret_cast<const char*>(compressed.data()),
                  static_cast<std::streamsize>(compressed.size()));
        bufIdx++;
      };

      for (const auto& b : pointDataBlocks_)
        if (b.valid) writeCompressedBlock(originalSizes[bufIdx]);
      for (const auto& b : cellDataBlocks_)
        if (b.valid) writeCompressedBlock(originalSizes[bufIdx]);
    } else {
      for (const auto& accessor : accessors_) {
        uint64_t dataSize = accessor->size_bytes();
        std::vector<uint8_t> headerBuf;
        headerBuf.reserve(8);
        core::write_le(dataSize, headerBuf);
        ofs.write(reinterpret_cast<const char*>(headerBuf.data()),
                  static_cast<std::streamsize>(headerBuf.size()));
        accessor->write_to_stream(ofs);
      }
    }
    ofs << "</AppendedData>\n";
  }

  template <std::ranges::range R>
  DataBlockInfo registerData(const R& data, std::string_view name,
                             int numComponents) {
    // Wrap in view to ensure zero-copy (stores reference)
    auto view = std::views::all(data);
    using ViewType = decltype(view);
    using T = std::ranges::range_value_t<ViewType>;

    DataBlockInfo info;
    info.name = name;
    info.offset = currentOffset_;
    info.typeName = vtkTypeName<std::remove_const_t<T>>();
    info.numComponents = numComponents;
    info.valid = true;

    auto accessor =
        std::make_unique<core::RangeAccessor<ViewType>>(std::move(view));
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

  std::array<int, 6> wholeExtent_;
  std::array<double, 3> origin_;
  std::array<double, 3> spacing_;

  std::vector<std::unique_ptr<core::DataAccessor>> accessors_;
  uint64_t currentOffset_ = 0;
  core::CompressionType compressionType_ = core::CompressionType::None;

  std::vector<DataBlockInfo> pointDataBlocks_;
  std::vector<DataBlockInfo> cellDataBlocks_;
};

}  // namespace microvtk
