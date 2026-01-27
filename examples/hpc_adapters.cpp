#include <microvtk/microvtk.hpp>
#include <vector>

// This example serves as a reference for integrating Kokkos Views.
// It will only be built if MICROVTK_USE_KOKKOS is enabled in CMake.

#ifdef MICROVTK_HAS_KOKKOS
#include <Kokkos_Core.hpp>
#endif

int main(int argc, char* argv[]) {
#ifdef MICROVTK_HAS_KOKKOS
  Kokkos::initialize(argc, argv);
  {
    using namespace microvtk;
    VtuWriter writer;

    int N = 100;
    // Host Space View
    Kokkos::View<double* [3], Kokkos::HostSpace> points("points", N);

    // Fill data in parallel
    Kokkos::parallel_for(
        "Fill", Kokkos::RangePolicy<Kokkos::DefaultHostExecutionSpace>(0, N),
        KOKKOS_LAMBDA(const int i) {
          points(i, 0) = i * 1.0;
          points(i, 1) = 0.0;
          points(i, 2) = 0.0;
        });

    // Use adapter to stream directly from View
    writer.setPoints(adapt(points));

    // Topology (Cells)
    // For this example we just create N vertex cells manually
    std::vector<int32_t> conn(N);
    std::vector<int32_t> offsets(N);
    std::vector<uint8_t> types(N, static_cast<uint8_t>(CellType::Vertex));
    for (int i = 0; i < N; ++i) {
      conn[i] = i;
      offsets[i] = i + 1;
    }
    writer.setCells(conn, offsets, types);

    writer.write("hpc_example.vtu");
  }
  Kokkos::finalize();
#else
  (void)argc;
  (void)argv;
#endif
  return 0;
}
