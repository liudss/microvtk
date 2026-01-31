#include <cmath>
#include <microvtk/adapter.hpp>
#include <microvtk/vti_writer.hpp>
#include <vector>

using namespace microvtk;

struct PixelData {
  double intensity;
  int category;
};

int main() {
  // 10x10x1 2D Image
  std::array<int, 6> extent = {0, 9, 0, 9, 0, 0};
  VtiWriter writer(extent);

  int width = 10;
  int height = 10;
  std::vector<PixelData> pixels;
  pixels.reserve(static_cast<size_t>(width) * height);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      double val = std::sin(x * 0.1) * std::cos(y * 0.1);
      int cat = (val > 0) ? 1 : 0;
      pixels.push_back({val, cat});
    }
  }

  // Use adapt to write AoS members
  writer.addPointData("Intensity", adapt(pixels, &PixelData::intensity));
  writer.addPointData("Category", adapt(pixels, &PixelData::category));

  writer.write("vti_adapt.vti");
  return 0;
}
