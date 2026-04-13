#include <microvtk/common/types.hpp>
#include <microvtk/pvd_writer.hpp>
#include <microvtk/vti_writer.hpp>
#include <microvtk/vtu_writer.hpp>
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>

namespace nb = nanobind;
using namespace nb::literals;

namespace microvtk {

class PyVtuWriter {
public:
  PyVtuWriter() : writer_(DataFormat::Appended) {}
  PyVtuWriter(const PyVtuWriter&) = delete;
  PyVtuWriter& operator=(const PyVtuWriter&) = delete;

  void set_compression(core::CompressionType type) {
    writer_.setCompression(type);
  }

  void set_points(nb::object points_obj) {
    auto points =
        nb::cast<nb::ndarray<double, nb::shape<-1, -1>, nb::c_contig>>(
            points_obj);
    keep_alive_.push_back(points_obj);
    int dim = static_cast<int>(points.shape(1));
    auto span = std::span<const double>(points.data(), points.size());
    writer_.setPoints(span, dim);
  }

  void set_cells(nb::object connectivity_obj, nb::object offsets_obj,
                 nb::object types_obj) {
    auto connectivity =
        nb::cast<nb::ndarray<int32_t, nb::shape<-1>, nb::c_contig>>(
            connectivity_obj);
    auto offsets = nb::cast<nb::ndarray<int32_t, nb::shape<-1>, nb::c_contig>>(
        offsets_obj);
    auto types =
        nb::cast<nb::ndarray<uint8_t, nb::shape<-1>, nb::c_contig>>(types_obj);

    keep_alive_.push_back(connectivity_obj);
    keep_alive_.push_back(offsets_obj);
    keep_alive_.push_back(types_obj);

    auto conn_span =
        std::span<const int32_t>(connectivity.data(), connectivity.size());
    auto off_span = std::span<const int32_t>(offsets.data(), offsets.size());
    auto type_span = std::span<const uint8_t>(types.data(), types.size());

    writer_.setCells(conn_span, off_span, type_span);
  }

  void add_point_data(std::string_view name, nb::object data_obj) {
    auto data =
        nb::cast<nb::ndarray<double, nb::shape<-1>, nb::c_contig>>(data_obj);
    keep_alive_.push_back(data_obj);
    auto span = std::span<const double>(data.data(), data.size());
    writer_.addPointData(name, span);
  }

  void add_cell_data(std::string_view name, nb::object data_obj) {
    auto data =
        nb::cast<nb::ndarray<double, nb::shape<-1>, nb::c_contig>>(data_obj);
    keep_alive_.push_back(data_obj);
    auto span = std::span<const double>(data.data(), data.size());
    writer_.addCellData(name, span);
  }

  void write(std::string_view filename) { writer_.write(filename); }

private:
  VtuWriter writer_;
  std::vector<nb::object> keep_alive_;
};

class PyVtiWriter {
public:
  PyVtiWriter(const std::array<int, 6>& wholeExtent,
              const std::array<double, 3>& origin = {0.0, 0.0, 0.0},
              const std::array<double, 3>& spacing = {1.0, 1.0, 1.0})
      : writer_(wholeExtent, origin, spacing) {}
  PyVtiWriter(const PyVtiWriter&) = delete;
  PyVtiWriter& operator=(const PyVtiWriter&) = delete;

  void set_compression(core::CompressionType type) {
    writer_.setCompression(type);
  }

  void add_point_data(std::string_view name, nb::object data_obj) {
    auto data =
        nb::cast<nb::ndarray<double, nb::shape<-1>, nb::c_contig>>(data_obj);
    keep_alive_.push_back(data_obj);
    auto span = std::span<const double>(data.data(), data.size());
    writer_.addPointData(name, span);
  }

  void add_cell_data(std::string_view name, nb::object data_obj) {
    auto data =
        nb::cast<nb::ndarray<double, nb::shape<-1>, nb::c_contig>>(data_obj);
    keep_alive_.push_back(data_obj);
    auto span = std::span<const double>(data.data(), data.size());
    writer_.addCellData(name, span);
  }

  void write(std::string_view filename) { writer_.write(filename); }

private:
  VtiWriter writer_;
  std::vector<nb::object> keep_alive_;
};

NB_MODULE(_microvtk, m) {
  nb::enum_<core::CompressionType>(m, "CompressionType")
      .value("NoCompression", core::CompressionType::None)
      .value("ZLib", core::CompressionType::ZLib)
      .value("LZ4", core::CompressionType::LZ4);

  nb::enum_<CellType>(m, "CellType")
      .value("Vertex", CellType::Vertex)
      .value("PolyVertex", CellType::PolyVertex)
      .value("Line", CellType::Line)
      .value("PolyLine", CellType::PolyLine)
      .value("Triangle", CellType::Triangle)
      .value("TriangleStrip", CellType::TriangleStrip)
      .value("Polygon", CellType::Polygon)
      .value("Pixel", CellType::Pixel)
      .value("Quad", CellType::Quad)
      .value("Tetra", CellType::Tetra)
      .value("Voxel", CellType::Voxel)
      .value("Hexahedron", CellType::Hexahedron)
      .value("Wedge", CellType::Wedge)
      .value("Pyramid", CellType::Pyramid);
  nb::class_<PyVtuWriter>(m, "VtuWriter",
                          "A writer for VTK Unstructured Grid (.vtu) files.")
      .def(nb::init<>())
      .def("set_compression", &PyVtuWriter::set_compression, "type"_a,
           "Set the compression type for data arrays.")
      .def("set_points", &PyVtuWriter::set_points, "points"_a,
           "Set the point coordinates from a numpy array (N, 3).")
      .def("set_cells", &PyVtuWriter::set_cells, "connectivity"_a, "offsets"_a,
           "types"_a, "Set the cell topology from numpy arrays.")
      .def("add_point_data", &PyVtuWriter::add_point_data, "name"_a, "data"_a,
           "Add a point data attribute array.")
      .def("add_cell_data", &PyVtuWriter::add_cell_data, "name"_a, "data"_a,
           "Add a cell data attribute array.")
      .def("write", &PyVtuWriter::write, "filename"_a,
           "Write the data to a .vtu file.");

  // --- VtiWriter ---
  nb::class_<PyVtiWriter>(m, "VtiWriter",
                          "A writer for VTK Image Data (.vti) files.")
      .def(nb::init<const std::array<int, 6>&, const std::array<double, 3>&,
                    const std::array<double, 3>&>(),
           "wholeExtent"_a, "origin"_a = std::array<double, 3>{0, 0, 0},
           "spacing"_a = std::array<double, 3>{1, 1, 1},
           "Construct a VTI writer with the given extent, origin, and spacing.")
      .def("set_compression", &PyVtiWriter::set_compression, "type"_a,
           "Set the compression type for data arrays.")
      .def("add_point_data", &PyVtiWriter::add_point_data, "name"_a, "data"_a,
           "Add a point data attribute array.")
      .def("add_cell_data", &PyVtiWriter::add_cell_data, "name"_a, "data"_a,
           "Add a cell data attribute array.")
      .def("write", &PyVtiWriter::write, "filename"_a,
           "Write the data to a .vti file.");

  // --- PvdWriter ---
  nb::class_<PvdWriter>(
      m, "PvdWriter",
      "A writer for VTK Meta Data (.pvd) files to manage time series.")
      .def(nb::init<std::string_view>(), "filename"_a,
           "Create a PVD writer for the given filename.")
      .def("add_step", &PvdWriter::addStep, "time"_a, "vtu_file"_a,
           "Add a time step to the series.")
      .def("save", &PvdWriter::save, "Save the PVD file to disk.");
}

}  // namespace microvtk
