#include <cmath>
#include <iostream>
#include <microvtk/indexing_adapter.hpp>
#include <microvtk/microvtk.hpp>
#include <vector>

// Simple Particle struct for AoS demo
struct Particle {
  double mass;
  double temperature;
};

// Generate some dummy data in Morton order
// In a real simulation, this would be your sorted particle array
template <typename T>
std::vector<T> generate_morton_data_3d(size_t n) {
  size_t size = n * n * n;
  std::vector<T> data(size);

  // We iterate in Raster order (x,y,z) to fill the data,
  // but place it into the vector at the Morton index.
  // This simulates "the simulation stores data at Morton index".
  for (size_t z = 0; z < n; ++z) {
    for (size_t y = 0; y < n; ++y) {
      for (size_t x = 0; x < n; ++x) {
        // Calculate Morton Index
        uint64_t idx = microvtk::detail::morton_encode_3d(x, y, z);

        // Value: A simple gradient x + y + z
        if constexpr (std::is_same_v<T, Particle>) {
          data[idx] = {1.0, static_cast<double>(x + y + z)};
        } else {
          data[idx] = static_cast<T>(x + y + z);
        }
      }
    }
  }
  return data;
}

template <typename T>
std::vector<T> generate_morton_data_2d(size_t n) {
  size_t size = n * n;
  std::vector<T> data(size);

  for (size_t y = 0; y < n; ++y) {
    for (size_t x = 0; x < n; ++x) {
      uint64_t idx = microvtk::detail::morton_encode_2d(x, y);
      // Value: Radial pattern
      double dx = (double)x - n / 2.0;
      double dy = (double)y - n / 2.0;
      data[idx] = static_cast<T>(std::sqrt(dx * dx + dy * dy));
    }
  }
  return data;
}

int main() {
  using namespace microvtk;

  try {
    // --------------------------------------------------------------------
    // Case 1: 3D Data (Scalar), Z-Curve Ordered
    // --------------------------------------------------------------------
    {
      const int N = 64;
      std::cout << "Generating 3D Morton data (" << N << "^3)..." << std::endl;
      auto raw_data = generate_morton_data_3d<float>(N);

      std::array<int, 6> extent = {0, N - 1, 0, N - 1, 0, N - 1};
      VtiWriter writer(extent);

      // Dimensions for the adapter
      std::array<size_t, 3> dims = {N, N, N};

      // Apply Adapter:
      // The file on disk will be written in standard Raster order,
      // readable by ParaView, VisIt, etc.
      auto view = raw_data | views::reorder_z_curve(dims);

      writer.addPointData("Temperature", view);
      writer.write("morton_3d.vti");
      std::cout << "Wrote morton_3d.vti" << std::endl;
    }

    // --------------------------------------------------------------------
    // Case 2: 2D Data (Scalar), Z-Curve Ordered
    // --------------------------------------------------------------------
    {
      const int N = 128;
      std::cout << "Generating 2D Morton data (" << N << "^2)..." << std::endl;
      auto raw_data = generate_morton_data_2d<double>(N);

      // Z extent is 0 to 0 (1 layer)
      std::array<int, 6> extent = {0, N - 1, 0, N - 1, 0, 0};
      VtiWriter writer(extent);

      std::array<size_t, 2> dims = {N, N};

      // 2D Overload automatically selected
      auto view = raw_data | views::reorder_z_curve(dims);

      writer.addPointData("Distance", view);
      writer.write("morton_2d.vti");
      std::cout << "Wrote morton_2d.vti" << std::endl;
    }

    // --------------------------------------------------------------------
    // Case 3: Combined Adapter (AoS + Z-Curve)
    // --------------------------------------------------------------------
    {
      const int N = 32;
      std::cout << "Generating 3D AoS Morton data (" << N << "^3)..."
                << std::endl;
      auto particles = generate_morton_data_3d<Particle>(N);

      std::array<int, 6> extent = {0, N - 1, 0, N - 1, 0, N - 1};
      VtiWriter writer(extent);
      std::array<size_t, 3> dims = {N, N, N};

      // Chain: Storage -> Reorder Index -> Extract Member -> Write
      auto temp_view = particles | views::reorder_z_curve(dims) |
                       adapt(&Particle::temperature);

      writer.addPointData("ParticleTemp", temp_view);
      writer.write("morton_aos_combined.vti");
      std::cout << "Wrote morton_aos_combined.vti" << std::endl;
    }

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
