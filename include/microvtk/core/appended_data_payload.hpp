#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <microvtk/core/appended_data_block.hpp>
#include <microvtk/core/binary_utils.hpp>
#include <microvtk/core/compressor.hpp>
#include <microvtk/core/data_accessor.hpp>
#include <ostream>
#include <span>
#include <vector>

namespace microvtk::core {

struct PreparedCompressedBlock {
  uint64_t originalSize = 0;
  std::vector<uint8_t> compressedBuffer;
};

struct PreparedAppendedData {
  bool usingCompression = false;
  std::vector<PreparedCompressedBlock> compressedBlocks;
};

inline void recomputeRawOffsets(
    const std::vector<AppendedDataBlockInfo*>& orderedBlocks,
    const std::vector<std::unique_ptr<DataAccessor>>& accessors) {
  uint64_t runningOffset = 0;
  for (auto* block : orderedBlocks) {
    if (!block->valid) continue;
    block->offset = runningOffset;
    runningOffset +=
        sizeof(uint64_t) + accessors[block->accessorIndex]->size_bytes();
  }
}

inline void prepareCompressedBlocks(
    const std::vector<AppendedDataBlockInfo*>& orderedBlocks,
    const std::vector<std::unique_ptr<DataAccessor>>& accessors,
    const std::unique_ptr<Compressor>& compressor,
    PreparedAppendedData& prepared) {
  uint64_t runningOffset = 0;

  for (auto* block : orderedBlocks) {
    if (!block->valid) continue;

    const auto& accessor = accessors[block->accessorIndex];
    std::vector<uint8_t> tempRaw;
    const auto directBytes = accessor->contiguous_bytes();
    std::span<const uint8_t> rawBytes;
    if (directBytes) {
      rawBytes = *directBytes;
    } else {
      VectorByteWriter writer(tempRaw);
      accessor->write_bytes(writer);
      rawBytes = tempRaw;
    }

    auto& compressedBlock = prepared.compressedBlocks.emplace_back();
    compressedBlock.originalSize = rawBytes.size();
    compressedBlock.compressedBuffer = compressor->compress(rawBytes);

    block->offset = runningOffset;
    constexpr uint64_t headerSize = 4 * sizeof(uint64_t);
    runningOffset += headerSize + compressedBlock.compressedBuffer.size();
  }
}

inline PreparedAppendedData prepareAppendedData(
    const std::vector<AppendedDataBlockInfo*>& orderedBlocks,
    const std::vector<std::unique_ptr<DataAccessor>>& accessors,
    CompressionType compressionType) {
  PreparedAppendedData prepared;
  prepared.usingCompression = (compressionType != CompressionType::None);
  auto compressor = createCompressor(compressionType);

  if (prepared.usingCompression && !compressor) {
    prepared.usingCompression = false;
  }

  if (prepared.usingCompression) {
    prepareCompressedBlocks(orderedBlocks, accessors, compressor, prepared);
  } else {
    recomputeRawOffsets(orderedBlocks, accessors);
  }

  return prepared;
}

inline void writeCompressedBlock(std::ostream& ofs,
                                 const std::vector<uint8_t>& compressed,
                                 uint64_t originalSize) {
  const std::vector<uint64_t> header = {
      1, originalSize, originalSize, static_cast<uint64_t>(compressed.size())};

  for (auto val : header) {
    std::vector<uint8_t> tmp;
    write_le(val, tmp);
    ofs.write(reinterpret_cast<const char*>(tmp.data()),
              static_cast<std::streamsize>(tmp.size()));
  }
  ofs.write(reinterpret_cast<const char*>(compressed.data()),
            static_cast<std::streamsize>(compressed.size()));
}

inline void writeRawBlock(std::ostream& ofs, const DataAccessor& accessor) {
  const uint64_t dataSize = accessor.size_bytes();
  std::vector<uint8_t> headerBuf;
  headerBuf.reserve(8);
  write_le(dataSize, headerBuf);
  ofs.write(reinterpret_cast<const char*>(headerBuf.data()),
            static_cast<std::streamsize>(headerBuf.size()));
  OstreamByteWriter writer(ofs);
  accessor.write_bytes(writer);
}

inline void writeAppendedData(
    std::ostream& ofs, const std::vector<AppendedDataBlockInfo*>& orderedBlocks,
    const std::vector<std::unique_ptr<DataAccessor>>& accessors,
    const PreparedAppendedData& prepared) {
  if (prepared.usingCompression) {
    size_t bufIdx = 0;
    for (const auto* block : orderedBlocks) {
      if (!block->valid) continue;
      const auto& compressedBlock = prepared.compressedBlocks[bufIdx];
      writeCompressedBlock(ofs, compressedBlock.compressedBuffer,
                           compressedBlock.originalSize);
      ++bufIdx;
    }
  } else {
    for (const auto* block : orderedBlocks) {
      if (!block->valid) continue;
      writeRawBlock(ofs, *accessors[block->accessorIndex]);
    }
  }
  ofs << "</AppendedData>\n";
}

}  // namespace microvtk::core
