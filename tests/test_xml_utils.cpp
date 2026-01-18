#include <gtest/gtest.h>
#include <microvtk/core/xml_utils.hpp>
#include <sstream>

using namespace microvtk::core;

TEST(XmlUtils, BasicStructure) {
  std::stringstream ss;
  {
    XmlBuilder xml(ss);
    xml.startElement("VTKFile");
    xml.attribute("type", "UnstructuredGrid");
    xml.attribute("version", "1.0");

    xml.startElement("UnstructuredGrid");
    xml.endElement();  // Close UnstructuredGrid

    xml.endElement();  // Close VTKFile
  }

  std::string output = ss.str();
  EXPECT_TRUE(output.find("<VTKFile type=\"UnstructuredGrid\"") !=
              std::string::npos);
  EXPECT_TRUE(output.find("<UnstructuredGrid/>") != std::string::npos);
  EXPECT_TRUE(output.find("</VTKFile>") != std::string::npos);
}

TEST(XmlUtils, ScopedElement) {
  std::stringstream ss;
  {
    XmlBuilder xml(ss);
    auto root = xml.scopedElement("Root");
    root.attr("id", 1);
    {
      auto child = xml.scopedElement("Child");
      child.attr("val", 3.14);
    }
  }
  std::string output = ss.str();
  EXPECT_TRUE(output.find("<Root id=\"1\">") != std::string::npos);
  EXPECT_TRUE(output.find("<Child val=\"3.14\"/>") != std::string::npos);
  EXPECT_TRUE(output.find("</Root>") != std::string::npos);
}
