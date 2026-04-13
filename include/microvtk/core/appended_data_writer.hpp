#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <microvtk/common/types.hpp>
#include <microvtk/core/binary_utils.hpp>
#include <microvtk/core/compressor.hpp>
#include <microvtk/core/data_accessor.hpp>
#include <microvtk/core/xml_utils.hpp>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace microvtk::core {

struct AppendedDataBlockInfo {
  std::string name;
  uint64_t offset = 0;
  std::string typeName;
  int numComponents = 1;
  size_t accessorIndex = 0;
  size_t numberOfElements = 0;
  bool valid = false;
};

class AppendedDataWriter {
public:
  void setCompression(CompressionType type) { compressionType_ = type; }

protected:
  using DataBlockInfo = AppendedDataBlockInfo;

  template <std::ranges::range R>
  DataBlockInfo registerData(const R& data, std::string_view name,
                             int numComponents) {
    auto view = std::views::all(data);
    using ViewType = decltype(view);
    using T = std::ranges::range_value_t<ViewType>;

    DataBlockInfo info;
    info.name = name;
    info.offset = currentOffset_;
    info.typeName = microvtk::vtkTypeName<std::remove_const_t<T>>();
    info.numComponents = numComponents;
    info.accessorIndex = accessors_.size();
    info.numberOfElements = std::ranges::size(view);
    info.valid = true;

    auto accessor = std::make_unique<RangeAccessor<ViewType>>(std::move(view));
    const uint64_t payloadSize = accessor->size_bytes();
    currentOffset_ += sizeof(uint64_t) + payloadSize;
    accessors_.push_back(std::move(accessor));

    return info;
  }

  [[nodiscard]] bool prepareAppendedData(
      const std::vector<DataBlockInfo*>& orderedBlocks,
      std::vector<std::vector<uint8_t>>& compressedBuffers,
      std::vector<uint64_t>& originalSizes) {
    bool usingCompression = (compressionType_ != CompressionType::None);
    auto compressor = createCompressor(compressionType_);

    if (usingCompression && !compressor) {
      usingCompression = false;
    }

    if (usingCompression) {
      prepareCompression(orderedBlocks, compressor, compressedBuffers,
                         originalSizes);
    } else {
      recomputeRawOffsets(orderedBlocks);
    }

    return usingCompression;
  }

  void writeVtkFileHeader(XmlBuilder& xml, std::string_view type,
                          bool usingCompression) const {
    xml.startElement("VTKFile");
    xml.attribute("type", type);
    xml.attribute("version", "1.0");
    xml.attribute("byte_order", "LittleEndian");
    xml.attribute("header_type", "UInt64");

    if (usingCompression) {
      if (compressionType_ == CompressionType::ZLib) {
        xml.attribute("compressor", "vtkZLibDataCompressor");
      } else if (compressionType_ == CompressionType::LZ4) {
        xml.attribute("compressor", "vtkLZ4DataCompressor");
      }
    }
  }

  static void writeDataArrayHeader(XmlBuilder& xml, const DataBlockInfo& info) {
    if (!info.valid) return;
    xml.startElement("DataArray");
    xml.attribute("type", info.typeName);
    xml.attribute("Name", info.name);
    xml.attribute("NumberOfComponents", info.numComponents);
    xml.attribute("format", "appended");
    xml.attribute("offset", info.offset);
    xml.endElement();
  }

  void writeAppendedData(
      std::ostream& ofs, const std::vector<DataBlockInfo*>& orderedBlocks,
      bool usingCompression,
      const std::vector<std::vector<uint8_t>>& compressedBuffers,
      const std::vector<uint64_t>& originalSizes) const {
    if (usingCompression) {
      size_t bufIdx = 0;
      for (const auto* block : orderedBlocks) {
        if (!block->valid) continue;

        const auto& compressed = compressedBuffers[bufIdx];
        const std::vector<uint64_t> header = {
            1, originalSizes[bufIdx], originalSizes[bufIdx],
            static_cast<uint64_t>(compressed.size())};

        for (auto val : header) {
          std::vector<uint8_t> tmp;
          write_le(val, tmp);
          ofs.write(reinterpret_cast<const char*>(tmp.data()),
                    static_cast<std::streamsize>(tmp.size()));
        }
        ofs.write(reinterpret_cast<const char*>(compressed.data()),
                  static_cast<std::streamsize>(compressed.size()));
        ++bufIdx;
      }
    } else {
      for (const auto* block : orderedBlocks) {
        if (!block->valid) continue;

        const auto& accessor = accessors_[block->accessorIndex];
        const uint64_t dataSize = accessor->size_bytes();
        std::vector<uint8_t> headerBuf;
        headerBuf.reserve(8);
        write_le(dataSize, headerBuf);
        ofs.write(reinterpret_cast<const char*>(headerBuf.data()),
                  static_cast<std::streamsize>(headerBuf.size()));
        accessor->write_to_stream(ofs);
      }
    }
    ofs << "</AppendedData>\n";
  }

private:
  void recomputeRawOffsets(const std::vector<DataBlockInfo*>& orderedBlocks) {
    uint64_t runningOffset = 0;
    for (auto* block : orderedBlocks) {
      if (!block->valid) continue;
      block->offset = runningOffset;
      runningOffset +=
          sizeof(uint64_t) + accessors_[block->accessorIndex]->size_bytes();
    }
  }

  void prepareCompression(const std::vector<DataBlockInfo*>& orderedBlocks,
                          const std::unique_ptr<Compressor>& compressor,
                          std::vector<std::vector<uint8_t>>& compressedBuffers,
                          std::vector<uint64_t>& originalSizes) {
    uint64_t runningOffset = 0;

    for (auto* block : orderedBlocks) {
      if (!block->valid) continue;

      auto& accessor = accessors_[block->accessorIndex];
      std::vector<uint8_t> tempRaw;
      const auto directBytes = accessor->contiguous_bytes();
      std::span<const uint8_t> rawBytes;
      if (directBytes) {
        rawBytes = *directBytes;
      } else {
        accessor->write_to(tempRaw);
        rawBytes = tempRaw;
      }

      originalSizes.push_back(rawBytes.size());
      auto compressed = compressor->compress(rawBytes);
      compressedBuffers.push_back(std::move(compressed));

      block->offset = runningOffset;
      constexpr uint64_t headerSize = 4 * sizeof(uint64_t);
      runningOffset += headerSize + compressedBuffers.back().size();
    }
  }

  std::vector<std::unique_ptr<DataAccessor>> accessors_;
  uint64_t currentOffset_ = 0;

protected:
  CompressionType compressionType_ = CompressionType::None;
};

}  // namespace microvtk::core
