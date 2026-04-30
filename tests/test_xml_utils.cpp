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

TEST(XmlUtils, EscapesStringAttributes) {
  std::stringstream ss;
  {
    XmlBuilder xml(ss);
    xml.startElement("Root");
    std::string value = "a&b<c>\"d'e";
    xml.attribute("name", value);
    xml.endElement();
  }

  std::string output = ss.str();
  EXPECT_TRUE(output.find("name=\"a&amp;b&lt;c&gt;&quot;d&apos;e\"") !=
              std::string::npos);
}

TEST(XmlUtils, StartsRawContentWithoutSelfClosingElement) {
  std::stringstream ss;
  {
    XmlBuilder xml(ss);
    xml.startElement("AppendedData");
    xml.attribute("encoding", "raw");
    xml.startRawContent();
    xml.writeRaw("_payload");
    xml.endElement();
  }

  EXPECT_EQ(ss.str(),
            "<AppendedData encoding=\"raw\">_payload</AppendedData>\n");
}
