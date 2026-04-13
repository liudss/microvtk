#include <cstdint>
#include <microvtk/common/types.hpp>
#include <microvtk/core/compressor.hpp>
#include <microvtk/pvd_writer.hpp>
#include <microvtk/vti_writer.hpp>
#include <microvtk/vtu_writer.hpp>
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace nb = nanobind;
using namespace nb::literals;

namespace microvtk {
namespace {

template <typename T>
std::span<const T> as_span(
    const nb::ndarray<T, nb::shape<-1>, nb::c_contig>& a) {
  return {a.data(), a.size()};
}

template <typename Writer>
class PyFieldWriter {
public:
  template <typename T>
  void add_point_data(std::string_view name,
                      nb::ndarray<T, nb::shape<-1>, nb::c_contig> data,
                      int components) {
    writer_.addPointData(name, as_span(data), components);
  }

  template <typename T>
  void add_cell_data(std::string_view name,
                     nb::ndarray<T, nb::shape<-1>, nb::c_contig> data,
                     int components) {
    writer_.addCellData(name, as_span(data), components);
  }

  void set_compression(core::CompressionType type) {
    writer_.setCompression(type);
  }

  void write(std::string_view filename) { writer_.write(filename); }

protected:
  template <typename... Args>
  explicit PyFieldWriter(Args&&... args)
      : writer_(std::forward<Args>(args)...) {}

  Writer writer_;
};

class PyVtuWriter : public PyFieldWriter<VtuWriter> {
public:
  PyVtuWriter() : PyFieldWriter<VtuWriter>(DataFormat::Appended) {}
  PyVtuWriter(const PyVtuWriter&) = delete;
  PyVtuWriter& operator=(const PyVtuWriter&) = delete;

  template <typename T>
  void set_points(nb::ndarray<T, nb::shape<-1>, nb::c_contig> points,
                  int input_dim) {
    writer_.setPoints(as_span(points), input_dim);
  }

  void set_cells(nb::ndarray<int32_t, nb::shape<-1>, nb::c_contig> connectivity,
                 nb::ndarray<int32_t, nb::shape<-1>, nb::c_contig> offsets,
                 nb::ndarray<uint8_t, nb::shape<-1>, nb::c_contig> types) {
    writer_.setCells(as_span(connectivity), as_span(offsets), as_span(types));
  }
};

class PyVtiWriter : public PyFieldWriter<VtiWriter> {
public:
  PyVtiWriter(const std::array<int, 6>& extent,
              const std::array<double, 3>& origin,
              const std::array<double, 3>& spacing)
      : PyFieldWriter<VtiWriter>(extent, origin, spacing) {}
  PyVtiWriter(const PyVtiWriter&) = delete;
  PyVtiWriter& operator=(const PyVtiWriter&) = delete;
};

template <typename PyWriter, typename Class, typename T>
void bind_field_dtype(Class& cls, const char* suffix) {
  cls.def((std::string("add_point_data_") + suffix).c_str(),
          &PyWriter::template add_point_data<T>, "name"_a, "data"_a,
          "components"_a);
  cls.def((std::string("add_cell_data_") + suffix).c_str(),
          &PyWriter::template add_cell_data<T>, "name"_a, "data"_a,
          "components"_a);
}

template <typename Class, typename T>
void bind_points_dtype(Class& cls, const char* suffix) {
  cls.def((std::string("set_points_") + suffix).c_str(),
          &PyVtuWriter::template set_points<T>, "points"_a, "input_dim"_a);
}

template <typename PyWriter, typename Class>
void bind_field_dtypes(Class& cls) {
  bind_field_dtype<PyWriter, Class, int8_t>(cls, "i8");
  bind_field_dtype<PyWriter, Class, uint8_t>(cls, "u8");
  bind_field_dtype<PyWriter, Class, int16_t>(cls, "i16");
  bind_field_dtype<PyWriter, Class, uint16_t>(cls, "u16");
  bind_field_dtype<PyWriter, Class, int32_t>(cls, "i32");
  bind_field_dtype<PyWriter, Class, uint32_t>(cls, "u32");
  bind_field_dtype<PyWriter, Class, int64_t>(cls, "i64");
  bind_field_dtype<PyWriter, Class, uint64_t>(cls, "u64");
  bind_field_dtype<PyWriter, Class, float>(cls, "f32");
  bind_field_dtype<PyWriter, Class, double>(cls, "f64");
}

template <typename Class>
void bind_point_dtypes(Class& cls) {
  bind_points_dtype<Class, int8_t>(cls, "i8");
  bind_points_dtype<Class, uint8_t>(cls, "u8");
  bind_points_dtype<Class, int16_t>(cls, "i16");
  bind_points_dtype<Class, uint16_t>(cls, "u16");
  bind_points_dtype<Class, int32_t>(cls, "i32");
  bind_points_dtype<Class, uint32_t>(cls, "u32");
  bind_points_dtype<Class, int64_t>(cls, "i64");
  bind_points_dtype<Class, uint64_t>(cls, "u64");
  bind_points_dtype<Class, float>(cls, "f32");
  bind_points_dtype<Class, double>(cls, "f64");
}

}  // namespace

NB_MODULE(_microvtk, m) {
  nb::enum_<core::CompressionType>(m, "Compression")
      .value("None_", core::CompressionType::None)
      .value("NONE", core::CompressionType::None)
      .value("ZLIB", core::CompressionType::ZLib)
      .value("LZ4", core::CompressionType::LZ4);

  auto vtu =
      nb::class_<PyVtuWriter>(m, "_VtuWriter",
                              "Internal NumPy bridge for VTU writing.")
          .def(nb::init<>())
          .def("set_compression", &PyVtuWriter::set_compression, "type"_a)
          .def("set_cells", &PyVtuWriter::set_cells, "connectivity"_a,
               "offsets"_a, "types"_a)
          .def("write", &PyVtuWriter::write, "filename"_a);
  bind_field_dtypes<PyVtuWriter>(vtu);
  bind_point_dtypes(vtu);

  auto vti =
      nb::class_<PyVtiWriter>(m, "_VtiWriter",
                              "Internal NumPy bridge for VTI writing.")
          .def(nb::init<const std::array<int, 6>&, const std::array<double, 3>&,
                        const std::array<double, 3>&>(),
               "extent"_a, "origin"_a, "spacing"_a)
          .def("set_compression", &PyVtiWriter::set_compression, "type"_a)
          .def("write", &PyVtiWriter::write, "filename"_a);
  bind_field_dtypes<PyVtiWriter>(vti);

  nb::class_<PvdWriter>(m, "_PvdWriter")
      .def(nb::init<std::string_view>(), "filename"_a)
      .def("add_step", &PvdWriter::addStep, "time"_a, "file"_a)
      .def("save", &PvdWriter::save);
}

}  // namespace microvtk
