#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <microvtk/common/types.hpp>
#include <microvtk/core/appended_data_block.hpp>
#include <microvtk/core/data_accessor.hpp>
#include <microvtk/core/xml_utils.hpp>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace microvtk::core {

class AppendedDataAttributes {
public:
  template <std::ranges::range R>
  void addPointData(std::string_view name, const R& data,
                    int numComponents = 1) {
    pointDataBlocks_.push_back(registerData(data, name, numComponents));
  }

  template <std::ranges::range R>
  void addCellData(std::string_view name, const R& data,
                   int numComponents = 1) {
    cellDataBlocks_.push_back(registerData(data, name, numComponents));
  }

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
    info.typeName = microvtk::vtkTypeName<std::remove_const_t<T>>();
    info.numComponents = numComponents;
    info.accessorIndex = accessors_.size();
    info.numberOfElements = std::ranges::size(view);
    info.valid = true;

    auto accessor = std::make_unique<RangeAccessor<ViewType>>(std::move(view));
    accessors_.push_back(std::move(accessor));

    return info;
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

  void validateAttributeDataSizes(size_t expectedPointElements,
                                  size_t expectedCellElements,
                                  std::string_view context) const {
    for (const auto& block : pointDataBlocks_) {
      if (block.numberOfElements !=
          expectedPointElements * block.numComponents) {
        throw std::invalid_argument(std::string(context) +
                                    ": Size mismatch in PointData '" +
                                    block.name + "'.");
      }
    }
    for (const auto& block : cellDataBlocks_) {
      if (block.numberOfElements !=
          expectedCellElements * block.numComponents) {
        throw std::invalid_argument(std::string(context) +
                                    ": Size mismatch in CellData '" +
                                    block.name + "'.");
      }
    }
  }

  void appendAttributeBlocks(std::vector<DataBlockInfo*>& blocks) {
    for (auto& block : pointDataBlocks_) blocks.push_back(&block);
    for (auto& block : cellDataBlocks_) blocks.push_back(&block);
  }

  [[nodiscard]] size_t attributeBlockCount() const {
    return pointDataBlocks_.size() + cellDataBlocks_.size();
  }

  void writePointDataHeaders(XmlBuilder& xml) const {
    if (pointDataBlocks_.empty()) return;

    auto pd = xml.scopedElement("PointData");
    for (const auto& block : pointDataBlocks_) writeDataArrayHeader(xml, block);
  }

  void writeCellDataHeaders(XmlBuilder& xml) const {
    if (cellDataBlocks_.empty()) return;

    auto cd = xml.scopedElement("CellData");
    for (const auto& block : cellDataBlocks_) writeDataArrayHeader(xml, block);
  }

  [[nodiscard]] const std::vector<std::unique_ptr<DataAccessor>>& accessors()
      const {
    return accessors_;
  }

private:
  std::vector<std::unique_ptr<DataAccessor>> accessors_;
  std::vector<DataBlockInfo> pointDataBlocks_;
  std::vector<DataBlockInfo> cellDataBlocks_;
};

}  // namespace microvtk::core
