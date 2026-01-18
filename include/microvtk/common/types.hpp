#pragma once

#include <cstdint>
#include <string_view>

namespace microvtk {

enum class DataFormat : std::uint8_t { Ascii, Binary, Appended };

// Standard VTK Cell Types
// Reference: https://vtk.org/doc/nightly/html/vtkCellType_8h_source.html
enum class CellType : uint8_t {
  Vertex = 1,
  PolyVertex = 2,
  Line = 3,
  PolyLine = 4,
  Triangle = 5,
  TriangleStrip = 6,
  Polygon = 7,
  Pixel = 8,
  Quad = 9,
  Tetra = 10,
  Voxel = 11,
  Hexahedron = 12,
  Wedge = 13,
  Pyramid = 14
};

// Type Traits for VTK XML types
template <typename T>
struct TypeTraits;

template <>
struct TypeTraits<int8_t> {
  static constexpr std::string_view name = "Int8";
};
template <>
struct TypeTraits<uint8_t> {
  static constexpr std::string_view name = "UInt8";
};
template <>
struct TypeTraits<int16_t> {
  static constexpr std::string_view name = "Int16";
};
template <>
struct TypeTraits<uint16_t> {
  static constexpr std::string_view name = "UInt16";
};
template <>
struct TypeTraits<int32_t> {
  static constexpr std::string_view name = "Int32";
};
template <>
struct TypeTraits<uint32_t> {
  static constexpr std::string_view name = "UInt32";
};
template <>
struct TypeTraits<int64_t> {
  static constexpr std::string_view name = "Int64";
};
template <>
struct TypeTraits<uint64_t> {
  static constexpr std::string_view name = "UInt64";
};
template <>
struct TypeTraits<float> {
  static constexpr std::string_view name = "Float32";
};
template <>
struct TypeTraits<double> {
  static constexpr std::string_view name = "Float64";
};

template <typename T>
constexpr std::string_view vtkTypeName() {
  return TypeTraits<T>::name;
}

}  // namespace microvtk
