#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <microvtk/pvd_writer.hpp>

using namespace microvtk;

TEST(PvdWriter, WriteSteps) {
  std::string filename = "test_series.pvd";

  {
    PvdWriter writer(filename);
    writer.addStep(0.1, "step_0.vtu");
    writer.addStep(0.2, "step_1.vtu");
  }

  ASSERT_TRUE(std::filesystem::exists(filename));

  std::ifstream ifs(filename);
  std::string content((std::istreambuf_iterator<char>(ifs)),
                      (std::istreambuf_iterator<char>()));

  EXPECT_TRUE(content.find("<VTKFile type=\"Collection\"") !=
              std::string::npos);
  EXPECT_TRUE(content.find("timestep=\"0.1\"") != std::string::npos);
  EXPECT_TRUE(content.find("file=\"step_0.vtu\"") != std::string::npos);
  EXPECT_TRUE(content.find("timestep=\"0.2\"") != std::string::npos);

  // Cleanup
  // std::filesystem::remove(filename);
}
