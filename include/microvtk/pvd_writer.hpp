#pragma once

#include <fstream>
#include <microvtk/core/xml_utils.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace microvtk {

class PvdWriter {
public:
  /// Create a PVD writer that manages the given filename.
  /// @param filename Path to the .pvd file.
  explicit PvdWriter(std::string_view filename) : filename_(filename) {}

  /// Add a time step.
  /// @param time Simulation time.
  /// @param vtu_file Relative path to the .vtu file for this step.
  void addStep(double time, std::string_view vtu_file) {
    steps_.emplace_back(time, std::string(vtu_file));
  }

  /// Write the PVD file to disk.
  void save() {
    std::ofstream ofs(filename_);
    core::XmlBuilder xml(ofs);

    xml.startElement("VTKFile");
    xml.attribute("type", "Collection");
    xml.attribute("version", "1.0");
    xml.attribute("byte_order", "LittleEndian");
    xml.attribute("header_type", "UInt64");

    {
      auto collection = xml.scopedElement("Collection");
      for (const auto& [t, f] : steps_) {
        // DataSet is self-closing usually, handled by ScopedElement destructor
        auto ds = xml.scopedElement("DataSet");
        ds.attr("timestep", t);
        ds.attr("group", "");
        ds.attr("part", 0);
        ds.attr("file", f);
      }
    }

    xml.endElement();  // Close VTKFile
  }

private:
  std::string filename_;
  std::vector<std::pair<double, std::string>> steps_;
};

}  // namespace microvtk
