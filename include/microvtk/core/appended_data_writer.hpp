#pragma once

#include <fstream>
#include <microvtk/core/appended_data_attributes.hpp>
#include <microvtk/core/appended_data_payload.hpp>
#include <microvtk/core/compressor.hpp>
#include <microvtk/core/xml_utils.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace microvtk::core {

class AppendedDataWriter : protected AppendedDataAttributes {
public:
  using AppendedDataAttributes::addCellData;
  using AppendedDataAttributes::addPointData;

  void setCompression(CompressionType type) { compressionType_ = type; }

protected:
  using DataBlockInfo = AppendedDataAttributes::DataBlockInfo;

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

  template <typename WriteXmlStructure>
  void writeAppendedFile(std::string_view filename,
                         const std::vector<DataBlockInfo*>& orderedBlocks,
                         WriteXmlStructure&& writeXmlStructure,
                         std::string_view context) {
    const auto prepared =
        prepareAppendedData(orderedBlocks, accessors(), compressionType_);

    std::ofstream ofs(std::string(filename), std::ios::binary);
    if (!ofs.is_open()) {
      throw std::runtime_error(std::string(context) +
                               ": Failed to open output file '" +
                               std::string(filename) + "'.");
    }

    std::forward<WriteXmlStructure>(writeXmlStructure)(
        ofs, prepared.usingCompression);
    microvtk::core::writeAppendedData(ofs, orderedBlocks, accessors(),
                                      prepared);

    ofs << "</VTKFile>";
    if (!ofs) {
      throw std::runtime_error(std::string(context) +
                               ": Failed while writing '" +
                               std::string(filename) + "'.");
    }
  }

  CompressionType compressionType_ = CompressionType::None;
};

}  // namespace microvtk::core
