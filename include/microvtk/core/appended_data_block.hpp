#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

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

}  // namespace microvtk::core
